#include <stdio.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_err.h"
#include "sys/param.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "a0090_servo_motor.h"
#include "task_common.h"

/**
 * Finger pin locations
 */
#define SERVO_THUMB   16
#define SERVO_INDEX   17
#define SERVO_MIDDLE  18
#define SERVO_RING    19
#define SERVO_PINKY   21

/**
 * PWM settings
 */
#define LEDC_TIMER        LEDC_TIMER_0
#define LEDC_MODE         LEDC_LOW_SPEED_MODE
#define LEDC_FREQ_HZ      50
#define LEDC_RESOLUTION   LEDC_TIMER_16_BIT

/** 
 * Servo Duty cycles
 */
#define SERVO_MIN_DUTY    1800   // 0 degrees
#define SERVO_MAX_DUTY    7864   // 180 degrees

// Convert angle to duty cycle
static uint32_t angle_to_duty(int angle) {
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
    return (angle * (SERVO_MAX_DUTY - SERVO_MIN_DUTY) / 180) + SERVO_MIN_DUTY;
}

// Move a specific servo
static void servo_set_angle(ledc_channel_t channel, int angle) {
    ledc_set_duty(LEDC_MODE, channel, angle_to_duty(angle));
    ledc_update_duty(LEDC_MODE, channel);
}

/**
 * Initialization of servo motors
 */
void a0090_servor_motor_init(void)
{
    // Timer config (shared across all servos)
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_MODE,
        .duty_resolution = LEDC_RESOLUTION,
        .timer_num       = LEDC_TIMER,
        .freq_hz         = LEDC_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    // Channel configs for each servo
    ledc_channel_config_t channels[5] = {
        { .gpio_num = SERVO_THUMB,  .channel = LEDC_CHANNEL_0 },
        { .gpio_num = SERVO_INDEX,  .channel = LEDC_CHANNEL_1 },
        { .gpio_num = SERVO_MIDDLE, .channel = LEDC_CHANNEL_2 },
        { .gpio_num = SERVO_RING,   .channel = LEDC_CHANNEL_3 },
        { .gpio_num = SERVO_PINKY,  .channel = LEDC_CHANNEL_4 },
    };

    // Initialize all channels
    for (int i = 0; i < 5; i++) {
        channels[i].speed_mode = LEDC_MODE;
        channels[i].timer_sel  = LEDC_TIMER;
        channels[i].duty       = angle_to_duty(90); // start at center
        channels[i].hpoint     = 0;
        ESP_ERROR_CHECK(ledc_channel_config(&channels[i]));
    }
}

/**
 * Setting the finger are the right angle
 */
void a0090_servor_motor_set_finger(int finger_location, int finger_angle)
{
   servo_set_angle(finger_location, finger_angle);
}
