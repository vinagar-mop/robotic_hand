#include <stdio.h>
#include "driver/ledc.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Pin definitions
#define SERVO_THUMB   16
#define SERVO_INDEX   17
#define SERVO_MIDDLE  18
#define SERVO_RING    19
#define SERVO_PINKY   21

// PWM settings
#define LEDC_TIMER        LEDC_TIMER_0
#define LEDC_MODE         LEDC_LOW_SPEED_MODE
#define LEDC_FREQ_HZ      50
#define LEDC_RESOLUTION   LEDC_TIMER_16_BIT

// Try these instead
#define SERVO_MIN_DUTY    1800   // 0 degrees
#define SERVO_MAX_DUTY    7864   // 180 degrees

// Convert angle to duty cycle
uint32_t angle_to_duty(int angle) {
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
    return (angle * (SERVO_MAX_DUTY - SERVO_MIN_DUTY) / 180) + SERVO_MIN_DUTY;
}

// Move a specific servo
void servo_set_angle(ledc_channel_t channel, int angle) {
    ledc_set_duty(LEDC_MODE, channel, angle_to_duty(angle));
    ledc_update_duty(LEDC_MODE, channel);
}

void app_main(void) {
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

    int finger_place;
    int finger_postion;
    // Test sequence - open and close hand
    while (1) {

        // Open hand (180 degrees)
        for (int i = 0; i < 5; i++) {
            servo_set_angle(i, 180);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));

        // Close hand (0 degrees)
        for (int i = 0; i < 5; i++) {
            servo_set_angle(i, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}