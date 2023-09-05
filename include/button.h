/**
  ******************************************************************************
  * @file           : button.h
  * @author         : Mauricio Barroso Benavides
  * @date           : Mar 20, 2022
  * @brief          : This file contains all the definitios, data types and
  *                   function prototypes for button.c file
  ******************************************************************************
  * @attention
  *
  * MIT License
  *
  * Copyright (c) 2022 Mauricio Barroso Benavides
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef BUTTON_H_
#define BUTTON_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"

#include "driver/gpio.h"

/* Exported types ------------------------------------------------------------*/
typedef void (* button_cb_t)(void *);

/**/
typedef struct {
	button_cb_t function;
	void *arg;
} button_function_t;

typedef enum {
	BUTTON_EDGE_FALLING = 0,
	BUTTON_EDGE_RISING
} button_edge_e;

/* Button states */
typedef enum {
	BUTTON_STATE_DOWN = 0,
	BUTTON_STATE_UP,
} button_state_e;

/**/
typedef enum {
	BUTTON_CLICK_SINGLE = 0,
	BUTTON_CLICK_MEDIUM,
	BUTTON_CLICK_LONG,
	BUTTON_CLICK_DOUBLE,
	BUTTON_CLICK_MAX
} button_click_e;

typedef struct {
	button_state_e state;											/*!< Button state */
	button_edge_e edge;												/*!< Button interrupt type */
	gpio_num_t gpio;													/*!< Button GPIO number */
	TickType_t tick_counter;									/*!< Tick counter */
	button_function_t function[BUTTON_CLICK_MAX];
	EventGroupHandle_t event_group;						/*!< Button FreeRTOS event group */
	UBaseType_t task_priority;								/*!< Button FreeRTOS task priority */
	uint32_t task_stack_size;									/*!< Button FreeRTOS task stack size */
	TimerHandle_t debounce_timer;							/*!< Button FreeRTOS debounce timer */
	TimerHandle_t click_timer;								/*!< Button FreeRTOS double click timer */
	uint8_t click_counter;										/*!< Button FreeRTOS double click timer */
} button_t;

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
/**
  * @brief Initialize a button instance
  *
  * @param me              : Pointer to button_t structure
  * @param gpio            : GPIO number to attach button
  * @param edge            : GPIO interrupt edge
  * @param task_priority   : Button task priority
  * @param task_stack_size : Button task stack size
  *
  * @retval
  * 	- ESP_OK on success
  * 	- ESP_ERR_INVALID_ARG if the argument is invalid
  */
esp_err_t button_init(button_t *const me, gpio_num_t gpio, button_edge_e edge,
		UBaseType_t task_priority, uint32_t task_stack_size);

/**
  * @brief Add a button callback function
  *
  * @param me         : Pointer to button_t structure
  * @param click_type : Button press time to register callback function
  * @param function   : Callback function code
  * @param arg        : Pointer to callback function argument
  *
  * @retval
  * 	- ESP_OK on success
  * 	- ESP_NO_MEM if is out of memory
  * 	- ESP_ERR_INVALID_ARG if the argument is invalid
  */
esp_err_t button_add_cb(button_t *const me,	button_click_e click_type,
		button_cb_t function, void * arg);

/**
  * @brief Remove a button callback function
  *
  * @param me         : Pointer to button_t structure
  * @param click_type : Button press time to unregister callback function
  *
  *
  * @retval
  * 	- ESP_OK on success
  * 	- ESP_ERR_INVALID_ARG if the argument is invalid
  */
esp_err_t button_remove_cb(button_t *const me, button_click_e click_type);

#ifdef __cplusplus
}
#endif

#endif /* BUTTON_H_ */

/***************************** END OF FILE ************************************/
