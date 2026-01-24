#include <stdio.h>
#include "esp_sleep.h"
#define ButtonPin GPIO_NUM_13
#define ButtonWakeUpLevel 0

void app_main(void)
{
     esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
     if (cause == ESP_SLEEP_WAKEUP_TIMER) {
        printf("Timer Wakeup");


     } else if (cause == ESP_SLEEP_WAKEUP_EXT0) {

        printf("Button Wakeup");
     }
     esp_sleep_enable_timer_wakeup(5 * 60 * 1000000); // 5 minutes

     esp_sleep_enable_ext0_wakeup(ButtonPin, ButtonWakeUpLevel);
}