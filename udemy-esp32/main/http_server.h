#ifndef MAIN_HTTP_SERVER_H_
#define MAIN_HTTP_SERVER_H_


/**
 * Messages for the http monitor
 */
typedef enum http_server_message
{
    HTTP_MSG_WIFI_CONNECT_INIT = 0,
    HTTP_MSG_WIFI_CONNECT_SUCCESS,
    HTTP_MSG_WIFI_CONNECT_FAIL,
} http_server_message_e;

/**
 * Structure for the message queue
 */
typedef struct http_server_queue_message
{
    http_server_message_e msgID;
} http_server_queue_message_t;

/**
 * Sends a message to the queue
 * @param msgID message ID from the http_server_message_e enum.
 * @return pdTRUE if an item was sucessfully sent to the queue, otherwise pdFALSE.
 * @note EXPAND the parameter list based on your requirments e.g. how you've expanded the http_server_queue_message_t.
 */
BaseType_t http_server_monitor_send_message(http_server_message_e msgID);

/**
 * Starts the HTTP server.
 */
void http_server_start(void);

/**
 * Stops the HTTP server.
 */
void http_server_stop(void);

/**
 * Timmer callback function which call an esp restart upon sucessful firmware update.
 */
void http_server_fw_update_reset_callback(void *args);

#endif /* MAIN_HTTP_SERVER_H_ */