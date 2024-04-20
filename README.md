
# ESP-IDF Button Component

## Features
- Each button support up to four different callback functions depending on the way the button is pressed (single, medium, long and double).
- Debounce algorithm is based on FSM (Finite State Machine), FreeRTOS software timers, FreeRTOS event groups and GPIO interrupts.
- Support pull-up and pull-down button configurations.
- Multiple instances.

## How to use
To use this component follow the next steps:

1. Include the I2C driver and component headers
```c
#include "driver/i2c_master.h"
#include "sgia.h"
```
2. Declare a handle of I2C bus and an instance of SGIA
```c
i2c_master_bus_handle_t i2c_bus_handle; 
sgia_t button1;
```

3. Define a RTOS task to run SGIA process
```c
void sgia_process_task(void *arg) {
    int32_t voc_index, nox_index;
    float temp, hum;
    
    for (;;) {
        /* Run SGIA process */
        ESP_ERROR_CHECK(sgia_run(&sgia));

        /* Get and print VOC and NOx indices */
        sgia_get_index_voc_and_nox(&sgia, &voc_index, &nox_index);
        printf("VOC index: %ld\tNOx index: %ld\r\n", sgia.voc_index, sgia.nox_index);

        /* Get and print temperature and humidity values */
        sgia_get_temp_and_hum(&sgia, &temp, &hum);
        printf("Temperature: %f\tHumidity: %f\r\n\n", sgia.temp, sgia.hum);
    
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

4. Configure and create a new I2C bus
```c
/* Configure and create a new I2C bus */
i2c_master_bus_config_t i2c_bus_config = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = I2C_NUM_0,
    .scl_io_num = GPIO_NUM_48,
    .sda_io_num = GPIO_NUM_47,
    .glitch_ignore_cnt = 7
};

ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle));
```

5. Initialize SGIA instance
```c
/* Initialize SGIA instance */
sgia_init(&sgia, i2c_bus_handle);
```

6. Create the RTOS task defined in 3
```c
xTaskCreate(
    sgia_process_task, /* Task code */
    "BSEC Task", /* Task name */
    configMINIMAL_STACK_SIZE * 4, /* Task stack depth */
    NULL, /* Task parameter */
    tskIDLE_PRIORITY + 2, /* Task priority */
    NULL); /* Task handle */
```

## License
MIT License

Copyright (c) 2022 Mauricio Barroso Benavides

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

