#include <stdio.h>
#include "esp_sleep.h"
#include "driver/i2c.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "esp_system.h"
#define ButtonPin GPIO_NUM_13
#define ButtonWakeUpLevel 0

void app_main(void)
{
   i2c_config_t cfg;
   cfg.mode = I2C_MODE_MASTER;
   cfg.sda_io_num = GPIO_NUM_15;
   cfg.scl_io_num = GPIO_NUM_14;
   uint8_t data1[3] = {0xAC, 0x33, 0x00};
   uint8_t data2[2] = {0x10, 0x02};
   uint8_t sensor1_data[6];
   uint8_t sensor2_data[8];
   uint8_t reset_command[2] = {0x00, 0xFF};
   sdmmc_host_t host = SDMMC_HOST_DEFAULT();
   sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
   esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024
   };
   sdmmc_card_t* card;

   esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
   if (ret != ESP_OK) {
      printf("Failed to mount SD Card Filesystem. Error: %s\n", esp_err_to_name(ret));
      return;
   }
   esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
   if (cause == ESP_SLEEP_WAKEUP_TIMER) {
      printf("Timer Wakeup");
      gpio_set_direction(GPIO_NUM_16, GPIO_MODE_OUTPUT);
      gpio_set_level(GPIO_NUM_16, 0);
      vTaskDelay(pdMS_TO_TICKS(180000)); // 3 minute delay
      i2c_master_write_to_device(I2C_NUM_0, 0x38, data1, 3, pdMS_TO_TICKS(1000));
      vTaskDelay(pdMS_TO_TICKS(80));
      esp_err_t result = i2c_master_read_from_device(I2C_NUM_0, 0x38, sensor1_data, 6, pdMS_TO_TICKS(1000));
      if (result == ESP_OK) {
         i2c_master_write_to_device(I2C_NUM_0, 0x52, reset_command, 2, pdMS_TO_TICKS(1000));
         vTaskDelay(pdMS_TO_TICKS(100));
         i2c_master_write_to_device(I2C_NUM_0, 0x52, data2, 2, pdMS_TO_TICKS(1000));
         vTaskDelay(pdMS_TO_TICKS(100));
         esp_err_t result2 =i2c_master_read_from_device(I2C_NUM_0, 0x52, sensor2_data, 8, pdMS_TO_TICKS(1000));
         if (result2 == ESP_OK){
            printf("Data Acquired\n");
            printf("sending sensor data to SD Card\n");
            FILE* f = fopen("/sdcard/sensor_data.txt", "a");
            if (f == NULL) {
               printf("Failed to open file for writing\n");
            } else {
               fprintf(f, "Sensor 1 Data: ");
               fprintf(f, "%02X %02X %02X %02X %02X %02X ", 
                       sensor1_data[0], sensor1_data[1], sensor1_data[2],
                       sensor1_data[3], sensor1_data[4], sensor1_data[5]);

               fprintf(f, "\nSensor 2 Data: ");
               fprintf(f, "%02X %02X %02X %02X %02X %02X %02X %02X ", 
                       sensor2_data[0], sensor2_data[1], sensor2_data[2], sensor2_data[3],
                       sensor2_data[4], sensor2_data[5], sensor2_data[6], sensor2_data[7]);
               fprintf(f, "\n");
               fclose(f);
               printf("Data written to SD Card\n");
            }
         }
      } else {
         printf("I2C Failed");
      }
      
   } else if (cause == ESP_SLEEP_WAKEUP_EXT0) {

      printf("Button Wakeup");
   } else {
      printf("Normal Boot");
   }
   esp_sleep_enable_timer_wakeup(5 * 60 * 1000000); // 5 minutes

   esp_sleep_enable_ext0_wakeup(ButtonPin, ButtonWakeUpLevel);
   esp_deep_sleep_start();
}