#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

void app_main(void)
{
    char * ourTaskName = pcTaskGetName(NULL);
    ESPLOG("The name of our task is: $s", ourTaskName);

    while(1)
    {
        ;;
    }
}
