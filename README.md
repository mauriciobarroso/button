# Button Driver for Tactile Switches Buttons Compatible with ESP-IDF and STM32CubeIDE

A lightweight, FreeRTOS-based button driver for ESP32 and STM32 platforms. Provides robust debouncing, event detection, and deferred callback dispatch with minimal RTOS overhead.

## Features

* **Event detection**: single-click, double-click, press, hold, and long-press
* **Adjustable timings**: debounce interval, double-click window, hold and long-press durations
* **Efficient RTOS usage**: only one software timer, one queue, and one dispatcher task required
* **Configurable dispatcher**: set task priority, stack size, and dispatch interval to suit your application
* **Compile-time scaling**: define the maximum number of button instances for optimized resource allocation
* **Ready for ESP-IDF**: made to be integrated as component in ESP-IDF projects

## Examples

* [Print press type](examples/print_press_type)

## How to use

A quick guide to use a single button and define its associated actions.

1. **Define a button instance**

   ```c
   button_t button;
   ```

2. **Initialize the button**

   ```c
   /* Replace GPIO_NUM and GPIO_PORT with your hardware definitions */
   button_init(&button, GPIO_NUM, GPIO_PORT);
   ```

3. **Register callbacks for each event**

   ```c
   void on_single(void *arg)  { /* handle single click */ }
   void on_double(void *arg)  { /* handle double click */ }

   button_register_action(&button, BUTTON_TYPE_SINGLE, on_single,  NULL);
   button_register_action(&button, BUTTON_TYPE_DOUBLE, on_double,  NULL);
   ```

4. **Unregister unwanted callbacks**

   ```c
   button_unregister_action(&button, BUTTON_TYPE_DOUBLE);
   ```

5. **Deinitialize when done**

   ```c
   button_deinit(&button);
   ```

## Dependencies

* [FSM Library for Embedded C Projects](https://github.com/mauriciobarroso/fsm)

## Roadmap

* Implement button priority levels and conflict resolution
* Add support for bare-metal (no-RTOS) environments
* Provide keypad matrix and ADC-based button drivers

## License

MIT License

Copyright (c) 2025 Mauricio Barroso Benavides

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
