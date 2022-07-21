/**
  ******************************************************************************
  * @file           : button.c
  * @author         : Mauricio Barroso Benavides
  * @date           : Mar 20, 2022
  * @brief          : This file provides code for the configuration and control
  *                   of bounce of the buttons instances
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

/* Includes ------------------------------------------------------------------*/
#include "button.h"
#include "esp_log.h"

/* External variables --------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
/* Button event bits */
#define BUTTON_SHORT_PRESS_BIT	BIT0
#define BUTTON_MEDIUM_PRESS_BIT	BIT1
#define BUTTON_LONG_PRESS_BIT	BIT2

/* Button  */
#define BUTTON_SHORT_TIME	CONFIG_BUTTON_DEBOUNCE_SHORT_TIME
#define BUTTON_MEDIUM_TIME	CONFIG_BUTTON_DEBOUNCE_MEDIUM_TIME
#define BUTTON_LONG_TIME	CONFIG_BUTTON_DEBOUNCE_LONG_TIME

/* Private function prototypes -----------------------------------------------*/
static void IRAM_ATTR isr_handler(void * arg);
static void button_task (void * arg);

static void timer_handler(TimerHandle_t timer);

/* Private variables ---------------------------------------------------------*/
/* Tag for debug */
static const char * TAG = "button";

/* Exported functions --------------------------------------------------------*/
esp_err_t button_init(button_t * const me,
		gpio_num_t gpio,
		UBaseType_t task_priority,
		uint32_t task_stack_size) {
	ESP_LOGI(TAG, "Initializing button component...");

	/* Error code variable */
	esp_err_t ret;

	/* Initialize callback variables */
	me->short_function.function = NULL;
	me->short_function.arg = NULL;
	me->medium_function.function = NULL;
	me->medium_function.arg = NULL;
	me->long_function.function = NULL;
	me->long_function.arg = NULL;

	/* Create button event group */
	me->event_group = xEventGroupCreate();

	if(me->event_group == NULL) {
		ESP_LOGE(TAG, "Error allocating memory to create event group");

		return ESP_ERR_NO_MEM;
	}

	/* Initialize button GPIO */
	if(gpio < GPIO_NUM_0 || gpio >= GPIO_NUM_MAX) {
		ESP_LOGE(TAG, "Error in gpio number argument");

		return ESP_ERR_INVALID_ARG;
	}

	me->gpio = gpio;

	gpio_config_t gpio_conf;
	gpio_conf.pin_bit_mask = 1ULL << me->gpio;
	gpio_conf.mode = GPIO_MODE_INPUT;

//	if(mode != FALLING_MODE && mode != RISING_MODE) {
//		ESP_LOGE(TAG, "Error in mode argument");
//
//		return ESP_ERR_INVALID_ARG;
//	}

	me->mode = FALLING_MODE;

	if(me->mode == FALLING_MODE) {
		gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;
		gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
		me->state = DOWN_STATE;
		gpio_conf.intr_type = GPIO_INTR_NEGEDGE;
	}
//	else {
//		gpio_conf.pull_up_en = GPIO_PULLUP_DISABLE;
//		gpio_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
//		me->state = UP_STATE;
//		gpio_conf.intr_type = GPIO_INTR_POSEDGE;
//	}

//	gpio_conf.intr_type = GPIO_INTR_ANYEDGE;
	ret = gpio_config(&gpio_conf);

	if(ret != ESP_OK) {
		ESP_LOGE(TAG, "Error configuring GPIO");

		return ret;
	}

	/* Install ISR service and add ISR handler */
	ret = gpio_install_isr_service(0);

	if(ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
		ESP_LOGE(TAG, "Error configuring GPIO ISR service");

		return ret;
	}

	ret = gpio_isr_handler_add(me->gpio, isr_handler, (void *)me);

	if(ret != ESP_OK) {
		ESP_LOGE(TAG, "Error adding GPIO ISR handler");

		return ret;
	}

	/* Initialize tick counter variable */
	me->tick_counter = 0;

	/* Initialize button task variables */
	me->task_priority = task_priority;
	me->task_stack_size = task_stack_size;

	/* Create RTOS task */
    if(xTaskCreate(button_task,
    		"Button Task",
			me->task_stack_size,
			(void *)me,
			me->task_priority,
			NULL) != pdPASS) {
    	ESP_LOGE(TAG, "Error allocating memory to create task");

    	return ESP_ERR_NO_MEM;
    }

    /* Creater FreeRTOS software timer to avoid bounce button */
    me->timer = xTimerCreate("Test timer",
    		pdMS_TO_TICKS(BUTTON_SHORT_TIME),
			pdFALSE,
			(void *)me,
			timer_handler); /* todo: implement error handler */

	/* Return error code */
	return ret;
}

esp_err_t button_register_cb(button_t * const me,
		button_time_e time,
		button_cb_t function,
		void * arg) {
	ESP_LOGI(TAG, "Registering a callback for button...");

	/* Error code variable */
	esp_err_t ret;

	if(function == NULL && arg == NULL) {
		ESP_LOGI(TAG, "Error in function argument");
		return ESP_ERR_INVALID_ARG;
	}

	/* Select callback by time */
	switch(time) {
		case SHORT_TIME:
			me->short_function.function = function;
			me->short_function.arg = arg;

			break;

		case MEDIUM_TIME:
			me->medium_function.function = function;
			me->medium_function.arg = arg;

			break;

		case LONG_TIME:
			me->long_function.function = function;
			me->long_function.arg = arg;

			break;

		default:
			ESP_LOGI(TAG, "Invalid time value");
			ret = ESP_ERR_INVALID_ARG;
			break;
	}

	/* Return error code */
	return ret;
}

esp_err_t button_unregister_cb(button_t * const me, button_time_e time) {
	ESP_LOGI(TAG, "Initializing button component...");

	/* Error code variable */
	esp_err_t ret;

	/* Select callback by time and clear button callback function and argument*/
	switch(time) {
		case SHORT_TIME:
			me->short_function.function = NULL;
			me->short_function.arg = NULL;

			break;

		case MEDIUM_TIME:
			me->medium_function.function = NULL;
			me->medium_function.arg = NULL;

			break;

		case LONG_TIME:
			me->long_function.function = NULL;
			me->long_function.arg = NULL;

			break;

		default:
			ESP_LOGI(TAG, "Error in arg argument");
			ret = ESP_ERR_INVALID_ARG;
			break;
	}

	/* Return error code */
	return ret;
}
/* Private functions ---------------------------------------------------------*/
static void IRAM_ATTR isr_handler(void * arg) {
	button_t * button = (button_t *)arg;

	static TickType_t elapsed_time = 0;

	switch(button->state) {
		case DOWN_STATE:
			/* Disable button interrupt */
			gpio_set_intr_type(button->gpio, GPIO_INTR_DISABLE);

			/* Get initial tick counter */
			button->tick_counter = xTaskGetTickCountFromISR();

			/* Start debounce timer */
			xTimerStartFromISR(button->timer, NULL);

			break;
		case UP_STATE:
			/* Disable button interrupt */
			gpio_set_intr_type(button->gpio, GPIO_INTR_DISABLE);

			/* Calculate and print button elapsed time pressed */
			elapsed_time = pdTICKS_TO_MS(xTaskGetTickCountFromISR() - button->tick_counter);
			ESP_DRAM_LOGI(TAG, "button %d pressed for %d ms", button->gpio, elapsed_time);

			/* Execute function callback according button elapsed time pressed */
			if(elapsed_time >= BUTTON_SHORT_TIME && elapsed_time < BUTTON_MEDIUM_TIME) {
				xEventGroupSetBitsFromISR(button->event_group, BUTTON_SHORT_PRESS_BIT, NULL);
			}
			else if((elapsed_time >= BUTTON_MEDIUM_TIME && elapsed_time < BUTTON_LONG_TIME)) {
				xEventGroupSetBitsFromISR(button->event_group, BUTTON_MEDIUM_PRESS_BIT, NULL);
			}
			else if(elapsed_time >= BUTTON_LONG_TIME) {
				xEventGroupSetBitsFromISR(button->event_group, BUTTON_LONG_PRESS_BIT, NULL);
			}
			else {
//				ESP_DRAM_LOGI(TAG, "button %d bounce", button->gpio);
				break;
			}

			/* Reset button elapsed time pressed */
			elapsed_time = 0;

			/* Start debounce timer */
			xTimerStartFromISR(button->timer, NULL);

			break;

		default:
//			ESP_DRAM_LOGI(TAG, "button error");
			break;
	}

	portYIELD_FROM_ISR();
}

static void button_task (void * arg) {
	button_t * button = (button_t *)arg;
	EventBits_t bits;
	const EventBits_t bits_wait_for = (BUTTON_SHORT_PRESS_BIT |
			BUTTON_MEDIUM_PRESS_BIT |
			BUTTON_LONG_PRESS_BIT);

	for(;;) {
		/* Wait until some bit is set */
		bits = xEventGroupWaitBits(button->event_group,
				bits_wait_for,
				pdTRUE,
				pdFALSE,
				portMAX_DELAY);

		if(bits & BUTTON_SHORT_PRESS_BIT) {
			ESP_LOGI(TAG, "BUTTON_SHORT_PRESS_BIT set");
//			ESP_LOGI(TAG, "Button %d", button->gpio);

			/* Execute callback function */
			if(button->short_function.function != NULL) {
				button->short_function.function(button->short_function.arg);
			}
			else {
				ESP_LOGW(TAG, "Not defined callback function");
			}
		}

		else if(bits & BUTTON_MEDIUM_PRESS_BIT) {
			ESP_LOGI(TAG, "BUTTON_MEDIUM_PRESS_BIT set");
//			ESP_LOGI(TAG, "Button %d", button->gpio);

			/* Execute callback function */
			/* Execute callback function */
			if(button->medium_function.function != NULL) {
				button->medium_function.function(button->medium_function.arg);
			}
			else {
				ESP_LOGW(TAG, "Not defined callback function");
			}
		}
		else if(bits & BUTTON_LONG_PRESS_BIT) {
			ESP_LOGI(TAG, "BUTTON_LONG_PRESS_BIT set");
//			ESP_LOGI(TAG, "Button %d", button->gpio);

			/* Execute callback function */
			if(button->long_function.function != NULL) {
				button->long_function.function(button->long_function.arg);
			}
			else {
				ESP_LOGW(TAG, "Not defined callback function");
			}
		}
		else {
			ESP_LOGI(TAG, "Button unexpected event");
		}
	}
}

static void timer_handler(TimerHandle_t timer) {

	button_t * button = (button_t *)pvTimerGetTimerID(timer);

//	ESP_DRAM_LOGI(TAG, "Button: %d, state %s", button->gpio, button->state? "UP_STATE": "DOWN_STATE");

	if(button->state == DOWN_STATE) {
		if(!gpio_get_level(button->gpio)) {
			/* Change to next state */
			button->state = UP_STATE;

			/* Enable button interrupt to detect positive edge */
			gpio_set_intr_type(button->gpio, GPIO_INTR_POSEDGE);
		}
		else {
//			ESP_DRAM_LOGI(TAG, "POSEDGE very fast");
			/* Enable button interrupt to detect negative edge */
			gpio_set_intr_type(button->gpio, GPIO_INTR_NEGEDGE);
		}

	}
	else {
		if(gpio_get_level(button->gpio)) {
			/* Change to next state */
			button->state = DOWN_STATE;

			/* Enable button interrupt to detect negative edge */
			gpio_set_intr_type(button->gpio, GPIO_INTR_NEGEDGE);
		}
		else {
			/* Enable button interrupt to detect positive edge */
			gpio_set_intr_type(button->gpio, GPIO_INTR_POSEDGE);
//			ESP_DRAM_LOGI(TAG, "NEGEDGE very fast");
		}
	}
}

/***************************** END OF FILE ************************************/

