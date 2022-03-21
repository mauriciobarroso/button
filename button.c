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

/* Private define ------------------------------------------------------------*/
#define BUTTON_SHORT_PRESS_BIT	BIT0
#define BUTTON_MEDIUM_PRESS_BIT	BIT1
#define BUTTON_LONG_PRESS_BIT	BIT2

/* Private macro -------------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
static void IRAM_ATTR isr_handler(void * arg);
static void button_task (void * arg);

/* Private variables ---------------------------------------------------------*/
/* Tag for debug */
static const char * TAG = "button";

/* Exported functions --------------------------------------------------------*/
esp_err_t button_init(button_t * const me, gpio_num_t gpio, button_mode_e mode) {
	ESP_LOGI(TAG, "Initializing button component...");

	/* Error code variable */
	esp_err_t ret;

	/* Initialize callback variables */
	me->short_cb = NULL;
	me->short_arg = NULL;

	me->medium_cb = NULL;
	me->medium_arg = NULL;

	me->long_cb = NULL;
	me->long_arg = NULL;

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

	if(mode != FALLING_MODE && mode != RISING_MODE) {
		ESP_LOGE(TAG, "Error in mode argument");

		return ESP_ERR_INVALID_ARG;
	}

	me->mode = mode;

	if(me->mode == FALLING_MODE) {
		gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;
		gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
		me->state = FALLING_STATE;
	}
	else {
		gpio_conf.pull_up_en = GPIO_PULLUP_DISABLE;
		gpio_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
		me->state = RISING_STATE;
	}

	gpio_conf.intr_type = GPIO_INTR_ANYEDGE;
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

	/* Create RTOS task */
    if(xTaskCreate(button_task,
    		"Button Task",
			configMINIMAL_STACK_SIZE * 4,
			(void *)me,
			tskIDLE_PRIORITY + 10,
			NULL) != pdPASS) {
    	ESP_LOGE(TAG, "Error allocating memory to create task");

    	return ESP_ERR_NO_MEM;
    }

	/* Return error code */
	return ret;
}

esp_err_t button_register_cb(button_t * const me, button_time_e time, button_cb_t cb, void * arg) {
	ESP_LOGI(TAG, "Registering a callback for button...");

	/* Error code variable */
	esp_err_t ret;

	if(cb == NULL) {
		ESP_LOGI(TAG, "Error in cb argument");
		return ESP_ERR_INVALID_ARG;
	}

	/* Select callback by time */
	switch(time) {
		case SHORT_TIME:
			me->short_cb = cb;
			me->short_arg = arg;

			break;

		case MEDIUM_TIME:
			me->medium_cb = cb;
			me->medium_arg = arg;

			break;

		case LONG_TIME:
			me->long_cb = cb;
			me->long_arg = arg;

			break;

		default:
			ESP_LOGI(TAG, "Error in arg argument");
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
			me->short_cb = NULL;
			me->short_arg = NULL;

			break;

		case MEDIUM_TIME:
			me->medium_cb = NULL;
			me->medium_arg = NULL;

			break;

		case LONG_TIME:
			me->long_cb = NULL;
			me->long_arg = NULL;

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

	static TickType_t elapsed_time = 0;	/* todo: test me */

	switch(button->state) {
		case FALLING_STATE:
			if(gpio_get_level(button->gpio) == button->mode) {
				button->tick_counter = xTaskGetTickCountFromISR();
				button->state = RISING_STATE;
			}
			else {
				ESP_DRAM_LOGI(TAG, "FALLING_STATE again");
			}

			break;
		case RISING_STATE:
			if(gpio_get_level(button->gpio ) == !button->mode) {
				elapsed_time = pdTICKS_TO_MS(xTaskGetTickCountFromISR() - button->tick_counter);

				if(elapsed_time >= CONFIG_BUTTON_DEBOUNCE_SHORT_TIME && elapsed_time < CONFIG_BUTTON_DEBOUNCE_MEDIUM_TIME) {
					xEventGroupSetBitsFromISR(button->event_group, BUTTON_SHORT_PRESS_BIT, NULL);
				}
				else if((elapsed_time >= CONFIG_BUTTON_DEBOUNCE_MEDIUM_TIME && elapsed_time < CONFIG_BUTTON_DEBOUNCE_LONG_TIME)) {
					xEventGroupSetBitsFromISR(button->event_group, BUTTON_MEDIUM_PRESS_BIT, NULL);
				}
				else if(elapsed_time >= CONFIG_BUTTON_DEBOUNCE_LONG_TIME) {
					xEventGroupSetBitsFromISR(button->event_group, BUTTON_LONG_PRESS_BIT, NULL);
				}
				else {
				}
				ESP_DRAM_LOGI(TAG, "button %d pressed for %d ms", button->gpio, elapsed_time);

				elapsed_time = 0;
			}

			else {
							ESP_DRAM_LOGI(TAG, "RISING_STATE again");
						}

			button->state = FALLING_STATE;

			break;

		default:
			ESP_DRAM_LOGI(TAG, "button error");
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
			ESP_LOGI(TAG, "Button %d", button->gpio);

			/* Execute callback function */
			if(button->short_cb != NULL) {
				button->short_cb(button->short_arg);
			}
			else {
				ESP_LOGW(TAG, "Not defined callback function");
			}
		}

		else if(bits & BUTTON_MEDIUM_PRESS_BIT) {
			ESP_LOGI(TAG, "BUTTON_MEDIUM_PRESS_BIT set");
			ESP_LOGI(TAG, "Button %d", button->gpio);

			/* Execute callback function */
			if(button->medium_cb != NULL) {
				button->medium_cb(button->short_arg);
			}
			else {
				ESP_LOGW(TAG, "Not defined callback function");
			}
		}
		else if(bits & BUTTON_LONG_PRESS_BIT) {
			ESP_LOGI(TAG, "BUTTON_LONG_PRESS_BIT set");
			ESP_LOGI(TAG, "Button %d", button->gpio);

			/* Execute callback function */
			if(button->long_cb != NULL) {
				button->long_cb(button->short_arg);
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

/***************************** END OF FILE ************************************/
