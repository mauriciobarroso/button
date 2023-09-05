
# ESP-IDF Button Component

## Features
- Each button can have up to four different callback functions depending on the way the button is pressed (single, medium, long and double).
- Debounce algorithm is based on FSM (Finite State Machine), FreeRTOS software timers, FreeRTOS event groups and GPIO interrupts.
- Multiple instances.

## How to use
To use this component follow the next steps:

1. Configure the component in the project configuration menu (`idf.py menuconfig`)

`
Component config-->Button Configuration
`

2. Include the component header
```c
#include "button.h"
```
3. Create one or more instance of the component
```c
button_t button1;
button_t button2; 
```

4. Define button callback functions
```c
/* Callback function to handle press of button 1 without argument*/
void button1_cb(void *arg) {
    printf("Button 1 single click");
}

/* Callback function to handle press of button 2 with argument*/
void button2_cb(void *arg) {
    printf("%s\n", (char *)arg);
}
```

5. Initialize the component instances
```c
/* Initialize button instances */
ESP_ERROR_CHECK(button_init(&button1, /* Button instance */
    GPIO_NUM_0,                       /* Button GPIO number */
    tskIDLE_PRIORITY + 10,            /* Button FreeRTOS task priority */
    configMINIMAL_STACK_SIZE * 4));   /* Button FreeRTOS task stack size */

ESP_ERROR_CHECK(button_init(&button2, /* Button instance */
    GPIO_NUM_21,                      /* Button GPIO number */
    tskIDLE_PRIORITY + 11,            /* Button FreeRTOS task priority */
    configMINIMAL_STACK_SIZE * 4));   /* Button FreeRTOS task stack size */
```

6. Add the callback functions defined in 3
```c
 /* Register button1 callback for single click without argument */
button_add_cb(&button1, BUTTON_CLICK_SINGLE, button1_cb, NULL);
 
 /* Register button2 callbacks for medium and double click with different arguments */
button_add_cb(&button2, BUTTON_CLICK_MEDIUM, button2_cb, "Button 2 medium click");
button_add_cb(&button2, BUTTON_CLICK_DOUBLE, button2_cb, "Button 2 double click");
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

