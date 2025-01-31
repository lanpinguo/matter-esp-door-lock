/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_log.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"

#include <esp_matter.h>
#include <app-common/zap-generated/attributes/Accessors.h>

#include "bsp/esp-bsp.h"

#include <app_priv.h>

#include <soc/gpio_reg.h>
#include <soc/io_mux_reg.h>

static const char *TAG = "app_driver";

using namespace chip::app::Clusters;
using namespace esp_matter;
using namespace chip::app::Clusters::DoorLock;

app_driver_handle_t app_driver_button_init()
{
    /* Initialize button */
    button_handle_t btns[BSP_BUTTON_NUM];
    ESP_ERROR_CHECK(bsp_iot_button_create(btns, NULL, BSP_BUTTON_NUM));

    return (app_driver_handle_t)btns[0];
}



#define GPIO_OUTPUT_IO_0    GPIO_NUM_14
#define GPIO_OUTPUT_IO_1    GPIO_NUM_19
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_OUTPUT_IO_0) | (1ULL<<GPIO_OUTPUT_IO_1))
/*
 * Let's say, GPIO_OUTPUT_IO_0=18, GPIO_OUTPUT_IO_1=19
 * In binary representation,
 * 1ULL<<GPIO_OUTPUT_IO_0 is equal to 0000000000000000000001000000000000000000 and
 * 1ULL<<GPIO_OUTPUT_IO_1 is equal to 0000000000000000000010000000000000000000
 * GPIO_OUTPUT_PIN_SEL                0000000000000000000011000000000000000000
 * */
#define GPIO_INPUT_IO_0    GPIO_NUM_3
#define GPIO_INPUT_IO_1    GPIO_NUM_20
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_IO_0) | (1ULL<<GPIO_INPUT_IO_1))
/*
 * Let's say, GPIO_INPUT_IO_0=4, GPIO_INPUT_IO_1=5
 * In binary representation,
 * 1ULL<<GPIO_INPUT_IO_0 is equal to 0000000000000000000000000000000000010000 and
 * 1ULL<<GPIO_INPUT_IO_1 is equal to 0000000000000000000000000000000000100000
 * GPIO_INPUT_PIN_SEL                0000000000000000000000000000000000110000
 * */
#define ESP_INTR_FLAG_DEFAULT 0

static QueueHandle_t gpio_evt_queue = NULL;
static bool lock_state = false;


static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    static uint32_t event_count = 0;
    
    event_count++;
    if (event_count > 10 && gpio_num == GPIO_INPUT_IO_0) {
        event_count = 0;
        // disable interrupt
        gpio_intr_disable(GPIO_INPUT_IO_0);
        // send event to queue
        xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
    }
}

esp_err_t app_driver_lock_state(uint32_t enable)
{
    lock_state = enable;
    return ESP_OK;
}

esp_err_t app_driver_unlock()
{
    gpio_set_level(GPIO_OUTPUT_IO_0, 1);
    gpio_hold_en(GPIO_OUTPUT_IO_0);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    gpio_set_level(GPIO_OUTPUT_IO_0, 0);
    gpio_hold_dis(GPIO_OUTPUT_IO_0);
    return ESP_OK;
}

static void gpio_process_task(void* arg)
{
    uint32_t io_num;
    for (;;) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            if (lock_state == false) {
                ESP_LOGI(TAG, "unlock\n");
                vTaskDelay(2000 / portTICK_PERIOD_MS);
                app_driver_unlock();
            }
            else {
                ESP_LOGI(TAG, "skip the current bell call\n");
                vTaskDelay(5000 / portTICK_PERIOD_MS);
            }
            // enable interrupt
            gpio_intr_enable(GPIO_INPUT_IO_0);
        }
    }
}


void hw_gpio_init(void)
{
    //zero-initialize the config structure.
    gpio_config_t io_conf = {};
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    //disable pull-up mode
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    //interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    //start gpio task
    xTaskCreate(gpio_process_task, "gpio_process_task", 2048, NULL, 10, NULL);

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void*) GPIO_INPUT_IO_0);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_1, gpio_isr_handler, (void*) GPIO_INPUT_IO_1);

    gpio_sleep_sel_dis(GPIO_INPUT_IO_0);
    gpio_sleep_sel_dis(GPIO_INPUT_IO_1);
    gpio_sleep_sel_dis(GPIO_OUTPUT_IO_0);
    gpio_sleep_sel_dis(GPIO_OUTPUT_IO_1);

    ESP_LOGI(TAG, "hw_gpio_init done");
}

esp_err_t app_driver_attribute_update(app_driver_handle_t driver_handle, uint16_t endpoint_id, uint32_t cluster_id,
                                      uint32_t attribute_id, esp_matter_attr_val_t *val)
{
    esp_err_t err = ESP_OK;
    ESP_LOGI(TAG, "attribute update endpoint %u - cluster %lu - attribute %lu -> value %lu", endpoint_id, cluster_id, attribute_id, val->val.u32);

    if (cluster_id == DoorLock::Id) {
        if (attribute_id == DoorLock::Attributes::LockState::Id) {
            DlLockState lock_state = (DlLockState)val->val.u32;
            if (lock_state == DlLockState::kLocked) {
                app_driver_lock_state(true);
            } 
            else if (lock_state == DlLockState::kUnlocked) {
                app_driver_lock_state(false);
            }
            else if (lock_state == DlLockState::kUnlatched) {
                app_driver_lock_state(false);
            }
        }
    }
    return err;
}

esp_err_t app_driver_init()
{
    esp_err_t err = ESP_OK;
    hw_gpio_init();
    return err;
}
