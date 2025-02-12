/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdint.h>
#include <stdbool.h>
#include "ulp_lp_core.h"
#include "ulp_lp_core_utils.h"
#include "ulp_lp_core_gpio.h"

#define DEBOUNCE_SAMPLES 1
#define PWR_MON_PIN         LP_IO_NUM_1
#define PWR_OFF_PIN         LP_IO_NUM_2

static int debounce_count;

/* this variable will be exported as a public symbol, visible from main CPU: */
bool gpio_level_previous = false;


int main (void)
{
    bool gpio_level = (bool)ulp_lp_core_gpio_get_level(PWR_MON_PIN);
    /* Debounce the input, only trigger a wakeup if the changed value is stable for DEBOUNCE_SAMPLES samples */
    if (gpio_level != gpio_level_previous) {
        debounce_count++;
    } else {
        debounce_count = 0;
    }

    /* Wakes up the main CPU if pin changed its state */
    if(debounce_count >= DEBOUNCE_SAMPLES) {
        gpio_level_previous = gpio_level;
        ulp_lp_core_wakeup_main_processor();
    }

    /* ulp_lp_core_halt() is called automatically when main exits */
    return 0;
}
