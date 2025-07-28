/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Mauricio Barroso Benavides
 * @date           : Jul 28, 2025
 * @brief          : Print Press type example
 ******************************************************************************
 * @attention
 *
 * MIT License
 *
 * Copyright (c) 2025 Mauricio Barroso Benavides
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
/* Standard includes */
#include <stdio.h>
#include <stdlib.h>

/* Component includes */
#include "button.h"

/* Macros --------------------------------------------------------------------*/
#define BUTTON_GPIO_NUM CONFIG_BUTTON_GPIO_NUM

/* Typedef -------------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static const char *TAG = "example";
static button_t btn;

/* Private function prototypes -----------------------------------------------*/
static void print_press_type(void *arg);

/* Main ----------------------------------------------------------------------*/
void app_main(void) {
	button_err_t err;

	err = button_init(&btn, BUTTON_GPIO_NUM, 0);

	if (err != BUTTON_ERR_OK) {
		printf("Failed to initialize button %d", BUTTON_GPIO_NUM);
	}

	button_register_action(&btn, BUTTON_TYPE_SINGLE, print_press_type,
						   "Single");
	button_register_action(&btn, BUTTON_TYPE_DOUBLE, print_press_type,
						   "Double");
	button_register_action(&btn, BUTTON_TYPE_PRESSED, print_press_type,
						   "Pressed");
	button_register_action(&btn, BUTTON_TYPE_HOLD, print_press_type, "Hold");
	button_register_action(&btn, BUTTON_TYPE_LONG, print_press_type, "Long");
}

/* Private function definition -----------------------------------------------*/
static void print_press_type(void *arg) {
	printf("%s\r\n", (char *)arg);
}

/***************************** END OF FILE ************************************/