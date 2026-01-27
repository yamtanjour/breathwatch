#include <stdio.h>
#include "esp_sleep.h"
#include "driver/i2c.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#define ButtonPin GPIO_NUM_13
#define ButtonWakeUpLevel 0

void app_main(void)
{
   uint8_t data1[3] = {0xAC, 0x33, 0x00};
   esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
   if (cause == ESP_SLEEP_WAKEUP_TIMER) {
      printf("Timer Wakeup");
      gpio_set_direction(GPIO_NUM_16, GPIO_MODE_OUTPUT);
      gpio_set_level(GPIO_NUM_16, 0);
      vTaskDelay(pdMS_TO_TICKS(180000)); // 3 minute delay
      i2c_master_write_to_device(I2C_NUM_0, 0x38, data1, 7, pdMS_TO_TICKS(1000));
   } else if (cause == ESP_SLEEP_WAKEUP_EXT0) {

      printf("Button Wakeup");
   } else {
      printf("Normal Boot");
   }
   esp_sleep_enable_timer_wakeup(5 * 60 * 1000000); // 5 minutes

   esp_sleep_enable_ext0_wakeup(ButtonPin, ButtonWakeUpLevel);
   esp_deep_sleep_start();
}