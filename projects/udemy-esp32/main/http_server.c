#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "sys/param.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "http_server.h"
#include "task_common.h"
#include "wifi_app.h"
#include "a0090_servo_motor.h"

static const char TAG[] = "http_server";

// Firmware update status
static int g_fw_update_status = OTA_UPDATE_PENDING;

// Current finger angles (0 = closed, 180 = fully open)
// Order: Thumb, Index, Middle, Ring, Pinky
static int g_finger_angles[5] = { 180, 180, 180, 180, 180 };
static const char *FINGER_NAMES[5] = { "Thumb", "Index", "Middle", "Ring", "Pinky" };

// HTTP server handle
static httpd_handle_t http_server_handle = NULL;

// HTTP server monitor task handle
static TaskHandle_t task_http_server_monitor = NULL;

// Queue handle for HTTP server monitor events
static QueueHandle_t http_server_monitor_queue_handle;

// Embedded files
extern const uint8_t jquery_3_3_1_min_js_start[] asm("_binary_jquery_3_3_1_min_js_start");
extern const uint8_t jquery_3_3_1_min_js_end[]   asm("_binary_jquery_3_3_1_min_js_end");
extern const uint8_t index_html_start[]           asm("_binary_index_html_start");
extern const uint8_t index_html_end[]             asm("_binary_index_html_end");
extern const uint8_t app_css_start[]              asm("_binary_app_css_start");
extern const uint8_t app_css_end[]                asm("_binary_app_css_end");
extern const uint8_t app_js_start[]               asm("_binary_app_js_start");
extern const uint8_t app_js_end[]                 asm("_binary_app_js_end");
extern const uint8_t favicon_ico_start[]          asm("_binary_favicon_ico_start");
extern const uint8_t favicon_ico_end[]            asm("_binary_favicon_ico_end");

/**
 * HTTP server monitor task — tracks server events.
 */
static void http_server_monitor(void *parameter)
{
	http_server_queue_message_t msg;

	for (;;)
	{
		if (xQueueReceive(http_server_monitor_queue_handle, &msg, portMAX_DELAY))
		{
			switch (msg.msgID)
			{
				case HTTP_MSG_WIFI_CONNECT_INIT:
					ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_INIT");
					break;

				case HTTP_MSG_WIFI_CONNECT_SUCCESS:
					ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_SUCCESS");
					break;

				case HTTP_MSG_WIFI_CONNECT_FAIL:
					ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_FAIL");
					break;

				case HTTP_MSG_OTA_UPDATE_SUCCESSFUL:
					ESP_LOGI(TAG, "HTTP_MSG_OTA_UPDATE_SUCCESSFUL");
					g_fw_update_status = OTA_UPDATE_SUCESSFUL;
					break;

				case HTTO_MSG_OTA_UPDATE_FAILED:
					ESP_LOGI(TAG, "HTTP_MSG_OTA_UPDATE_FAILED");
					g_fw_update_status = OTA_UPDATE_FAILED;
					break;

				default:
					break;
			}
		}
	}
}

/**
 * Serves jquery-3.3.1.min.js.
 */
static esp_err_t http_server_jquery_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "Jquery requested");
	httpd_resp_set_type(req, "application/javascript");
	httpd_resp_send(req, (const char *)jquery_3_3_1_min_js_start,
	                jquery_3_3_1_min_js_end - jquery_3_3_1_min_js_start);
	return ESP_OK;
}

/**
 * Serves index.html.
 */
static esp_err_t http_server_index_html_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "index.html requested");
	httpd_resp_set_type(req, "text/html");
	httpd_resp_send(req, (const char *)index_html_start,
	                index_html_end - index_html_start);
	return ESP_OK;
}

/**
 * Serves app.css.
 */
static esp_err_t http_server_app_css_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "app.css requested");
	httpd_resp_set_type(req, "text/css");
	httpd_resp_send(req, (const char *)app_css_start,
	                app_css_end - app_css_start);
	return ESP_OK;
}

/**
 * Serves app.js.
 */
static esp_err_t http_server_app_js_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "app.js requested");
	httpd_resp_set_type(req, "application/javascript");
	httpd_resp_send(req, (const char *)app_js_start,
	                app_js_end - app_js_start);
	return ESP_OK;
}

/**
 * Serves favicon.ico.
 */
static esp_err_t http_server_favicon_ico_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "favicon.ico requested");
	httpd_resp_set_type(req, "image/x-icon");
	httpd_resp_send(req, (const char *)favicon_ico_start,
	                favicon_ico_end - favicon_ico_start);
	return ESP_OK;
}

/**
 * POST /hand/angle
 * Body (URL-encoded): finger=<name>&angle=<0-180>
 * Sets the target angle for a finger and responds with JSON status.
 * Hook this handler into your servo control logic.
 */
static esp_err_t http_server_hand_angle_handler(httpd_req_t *req)
{
	int content_len = req->content_len;
	if (content_len <= 0 || content_len > 64)
	{
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid body length");
		return ESP_FAIL;
	}

	char buf[65] = { 0 };
	int received = httpd_req_recv(req, buf, content_len);
	if (received <= 0)
	{
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive error");
		return ESP_FAIL;
	}
	buf[received] = '\0';

	// Parse: finger=Index&angle=90
	char finger[16] = { 0 };
	int  angle      = -1;
	sscanf(buf, "finger=%15[^&]&angle=%d", finger, &angle);

	if (angle < 0 || angle > 180 || finger[0] == '\0')
	{
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "finger/angle out of range");
		return ESP_FAIL;
	}

	// Store angle and log
	for (int i = 0; i < 5; i++)
	{
		if (strcmp(finger, FINGER_NAMES[i]) == 0)
		{
			g_finger_angles[i] = angle;
			ESP_LOGI(TAG, "Finger %s -> %d deg", finger, angle);
			// TODO: call your servo driver here, e.g. servo_set(i, angle);
            a0090_servor_motor_set_finger(i, angle);
			break;
		}
	}

	httpd_resp_set_type(req, "application/json");
	httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
	httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
	return ESP_OK;
}

/**
 * GET /hand/status
 * Returns current finger angles as JSON.
 */
static esp_err_t http_server_hand_status_handler(httpd_req_t *req)
{
	char response[128];
	snprintf(response, sizeof(response),
	         "{\"Thumb\":%d,\"Index\":%d,\"Middle\":%d,\"Ring\":%d,\"Pinky\":%d}",
	         g_finger_angles[0], g_finger_angles[1], g_finger_angles[2],
	         g_finger_angles[3], g_finger_angles[4]);

	httpd_resp_set_type(req, "application/json");
	httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
	httpd_resp_sendstr(req, response);
	return ESP_OK;
}

/**
 * OTA firmware update handler.
 */
static esp_err_t http_server_OTA_update_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "OTA update requested");
	esp_ota_handle_t ota_handle;

	char ota_buff[1024];
	int content_length   = req->content_len;
	int content_received = 0;
	int recv_len         = 0;
	bool is_req_body_started = false;
	bool flash_successful    = false;

	const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

	while (content_received < content_length)
	{
		recv_len = httpd_req_recv(req, ota_buff, MIN(content_length - content_received, sizeof(ota_buff)));

		if (recv_len == HTTPD_SOCK_ERR_TIMEOUT)
		{
			ESP_LOGI(TAG, "OTA: socket timeout, retrying");
			continue;
		}
		if (recv_len <= 0)
		{
			ESP_LOGE(TAG, "OTA: receive error %d", recv_len);
			break;
		}

		if (!is_req_body_started)
		{
			is_req_body_started = true;
			char *body_start_p  = strstr(ota_buff, "\r\n\r\n");
			if (body_start_p == NULL)
			{
				ESP_LOGE(TAG, "OTA: could not find body start");
				break;
			}
			body_start_p += 4;
			int body_part_len = recv_len - (body_start_p - ota_buff);

			printf("OTA file size: %d\r\n", content_length);

			esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
			if (err != ESP_OK)
			{
				ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
				break;
			}

			esp_ota_write(ota_handle, body_start_p, body_part_len);
			content_received += recv_len;
		}
		else
		{
			esp_ota_write(ota_handle, ota_buff, recv_len);
			content_received += recv_len;
		}
	}

	if (is_req_body_started && content_received == content_length)
	{
		if (esp_ota_end(ota_handle) == ESP_OK)
		{
			if (esp_ota_set_boot_partition(update_partition) == ESP_OK)
			{
				flash_successful = true;
				ESP_LOGI(TAG, "OTA flash successful");
			}
		}
	}

	if (flash_successful)
	{
		http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_SUCCESSFUL);
	}
	else
	{
		http_server_monitor_send_message(HTTO_MSG_OTA_UPDATE_FAILED);
	}

	return ESP_OK;
}

/**
 * OTA status handler — returns firmware compile date/time and update status.
 */
static esp_err_t http_server_OTA_status_handler(httpd_req_t *req)
{
	char otaJSON[100];
	ESP_LOGI(TAG, "OTA status requested");

	sprintf(otaJSON, "{\"ota_update_status\":%d,\"compile_time\":\"%s\",\"compile_date\":\"%s\"}",
	        g_fw_update_status, __TIME__, __DATE__);

	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, otaJSON, strlen(otaJSON));
	return ESP_OK;
}

/**
 * Sends a message to the HTTP server monitor queue.
 */
BaseType_t http_server_monitor_send_message(http_server_message_e msgID)
{
	http_server_queue_message_t msg;
	msg.msgID = msgID;
	return xQueueSend(http_server_monitor_queue_handle, &msg, portMAX_DELAY);
}

/**
 * Configures and starts the HTTP server, registering all URI handlers.
 */
static httpd_handle_t https_server_configure(void)
{
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	xTaskCreatePinnedToCore(&http_server_monitor, "http_server_monitor",
	                        HTTP_SERVER_MONITOR_STACK_SIZE, NULL,
	                        HTTP_SERVER_MONITOR_PRIORITY, &task_http_server_monitor,
	                        HTTP_SERVER_MONITOR_CORE_ID);

	http_server_monitor_queue_handle = xQueueCreate(3, sizeof(http_server_queue_message_t));

	config.core_id          = HTTP_SERVER_TASK_CORE_ID;
	config.task_priority    = HTTP_SERVER_TASK_PRIORITY;
	config.stack_size       = HTTP_SERVER_TASK_STACK_SIZE;
	config.max_uri_handlers = 20;
	config.recv_wait_timeout = 10;
	config.send_wait_timeout = 10;

	ESP_LOGI(TAG, "Starting server on port %d, priority %d",
	         config.server_port, config.task_priority);

	if (httpd_start(&http_server_handle, &config) == ESP_OK)
	{
		ESP_LOGI(TAG, "Registering URI handlers");

		httpd_uri_t jquery_js = {
			.uri     = "/jquery-3.3.1.min.js",
			.method  = HTTP_GET,
			.handler = http_server_jquery_handler,
			.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &jquery_js);

		httpd_uri_t index_html = {
			.uri     = "/",
			.method  = HTTP_GET,
			.handler = http_server_index_html_handler,
			.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &index_html);

		httpd_uri_t app_css = {
			.uri     = "/app.css",
			.method  = HTTP_GET,
			.handler = http_server_app_css_handler,
			.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &app_css);

		// Fixed: was incorrectly registered as "/app.css"
		httpd_uri_t app_js = {
			.uri     = "/app.js",
			.method  = HTTP_GET,
			.handler = http_server_app_js_handler,
			.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &app_js);

		// Fixed: was incorrectly registered as "/app.css"
		httpd_uri_t favicon_ico = {
			.uri     = "/favicon.ico",
			.method  = HTTP_GET,
			.handler = http_server_favicon_ico_handler,
			.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &favicon_ico);

		httpd_uri_t OTA_update = {
			.uri     = "/OTAupdate",
			.method  = HTTP_POST,
			.handler = http_server_OTA_update_handler,
			.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &OTA_update);

		httpd_uri_t OTA_status = {
			.uri     = "/OTAstatus",
			.method  = HTTP_POST,
			.handler = http_server_OTA_status_handler,
			.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &OTA_status);

		// Hand control: set finger angle
		httpd_uri_t hand_angle = {
			.uri     = "/hand/angle",
			.method  = HTTP_POST,
			.handler = http_server_hand_angle_handler,
			.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &hand_angle);

		// Hand control: get current finger angles
		httpd_uri_t hand_status = {
			.uri     = "/hand/status",
			.method  = HTTP_GET,
			.handler = http_server_hand_status_handler,
			.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &hand_status);

		return http_server_handle;
	}

	return NULL;
}

void http_server_start(void)
{
	if (http_server_handle == NULL)
	{
		http_server_handle = https_server_configure();
	}
}

void http_server_stop(void)
{
	if (http_server_handle)
	{
		httpd_stop(http_server_handle);
		ESP_LOGI(TAG, "HTTP server stopped");
		http_server_handle = NULL;
	}
}
