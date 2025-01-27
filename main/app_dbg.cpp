// Copyright 2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_matter_console.h>
#include <esp_timer.h>
#include <string.h>

namespace esp_matter {
namespace console {

static const char *TAG = "app_dbg";
static engine app_dbg_console;

static esp_err_t mem_dump_console_handler(int argc, char *argv[])
{
    printf("\tDescription\tInternal\tSPIRAM\n");
    printf("Current Free Memory\t%d\t\t%d\n",
           heap_caps_get_free_size(MALLOC_CAP_8BIT) - heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
           heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    printf("Largest Free Block\t%d\t\t%d\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL),
           heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
    printf("Min. Ever Free Size\t%d\t\t%d\n", heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL),
           heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
    return ESP_OK;
}

static esp_err_t up_time_console_handler(int argc, char *argv[])
{
    printf("%s: Uptime of the device: %lld milliseconds\n", TAG, esp_timer_get_time() / 1000);
    return ESP_OK;
}

static esp_err_t app_dbg_dispatch(int argc, char **argv)
{
    if (argc <= 0) {
        app_dbg_console.for_each_command(print_description, NULL);
        return ESP_OK;
    }
    return app_dbg_console.exec_command(argc, argv);
}

esp_err_t app_dbg_register_commands()
{
    static const command_t command = {
        .name = "dbg",
        .description = "app debug commands. Usage matter esp dbg <app_dbg_command>.",
        .handler = app_dbg_dispatch,
    };

    static const command_t app_dbg_commands[] = {
        {
            .name = "mem-dump",
            .description = "help for memory analysis",
            .handler = mem_dump_console_handler,
        },
        {
            .name = "up-time",
            .description = "print the uptime of the device",
            .handler = up_time_console_handler,
        },
    };
    app_dbg_console.register_commands(app_dbg_commands, sizeof(app_dbg_commands)/sizeof(command_t));

    return add_commands(&command, 1);
}
} // namespace console
} // namespace esp_matter
