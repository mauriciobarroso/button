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
#define BUTTON_SHORT_PRESS_BIT				BIT0
#define BUTTON_MEDIUM_PRESS_BIT				BIT1
#define BUTTON_LONG_PRESS_BIT					BIT2
#define BUTTON_DOUBLE_CLICK_PRESS_BIT	BIT3

/* Private function prototypes -----------------------------------------------*/
static void IRAM_ATTR isr_handler(void * arg);
static void button_task (void * arg);

static void debounce_timer_handler(TimerHandle_t timer);
static void click_timer_handler(TimerHandle_t timer);

/* Private variables ---------------------------------------------------------*/
/* Tag for debug */
static const char * TAG = "button";

/* Exported functions --------------------------------------------------------*/
esp_err_t button_init(button_t *const me, gpio_num_t gpio, button_edge_e edge,
		UBaseType_t task_priority, uint32_t task_stack_size) {
	ESP_LOGI(TAG, "Initializing button component...");

	/* Error code variable */
	esp_err_t ret = ESP_OK;

	/* Initialize callback variables */
	for (uint8_t i = 0; i < BUTTON_CLICK_MAX; i++) {
		me->function[i].function = NULL;
	}

	/* Create button event group */
	me->event_group = xEventGroupCreate();

	if (me->event_group == NULL) {
		ESP_LOGE(TAG, "Failed to allocate memory to create event group");
		return ESP_ERR_NO_MEM;
	}

	/* Initialize button GPIO */
	if (gpio < GPIO_NUM_0 || gpio >= GPIO_NUM_MAX) {
		ESP_LOGE(TAG, "Invalid GPIO number");
		return ESP_ERR_INVALID_ARG;
	}

	me->gpio = gpio;

	gpio_config_t gpio_conf;
	gpio_conf.pin_bit_mask = 1ULL << me->gpio;
	gpio_conf.mode = GPIO_MODE_INPUT;

	me->edge = edge;

	if (me->edge == BUTTON_EDGE_FALLING) {
		gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;
		gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
		me->state = BUTTON_STATE_DOWN;
		gpio_conf.intr_type = GPIO_INTR_NEGEDGE;
	}
	else if (me->edge == BUTTON_EDGE_RISING) {
		gpio_conf.pull_up_en = GPIO_PULLUP_DISABLE;
		gpio_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
		me->state = BUTTON_STATE_UP;
		gpio_conf.intr_type = GPIO_INTR_POSEDGE;
	}
	else {
		ESP_LOGE(TAG, "Invalid mode");
		return ESP_ERR_INVALID_ARG;
	}

	ret = gpio_config(&gpio_conf);

	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to configure GPIO");
		return ret;
	}

	/* Install ISR service and add ISR handler */
	ret = gpio_install_isr_service(0);

	if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
		ESP_LOGE(TAG, "Failed to install GPIO ISR service");
		return ret;
	}

	ret = gpio_isr_handler_add(me->gpio, isr_handler, (void *)me);

	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to add GPIO ISR handler");
		return ret;
	}

	/* Initialize tick counter variable */
	me->tick_counter = 0;

	/* Initialize button task variables */
	me->task_priority = task_priority;
	me->task_stack_size = task_stack_size;

	/* Create RTOS task */
	if (xTaskCreate(button_task,
			"Button Task",
			me->task_stack_size,
			(void *)me,
			me->task_priority,
			NULL) != pdPASS) {

		ESP_LOGE(TAG, "Failed to allocate memory to create task");
		return ESP_ERR_NO_MEM;
	}

	/* Creater FreeRTOS software timer to filter button bounce */
	me->debounce_timer = xTimerCreate("Debounce timer",
			pdMS_TO_TICKS(CONFIG_BUTTON_DEBOUNCE_SHORT_TIME),
			pdFALSE,
			(void *)me,
			debounce_timer_handler);

	if (me->debounce_timer == NULL) {
		ESP_LOGE(TAG, "Failed to allocate memory to timer");
		return ESP_ERR_NO_MEM;
	}

	/* Creater FreeRTOS software timer to count the clicks number */
	me->click_timer = xTimerCreate("Click timer",
			pdMS_TO_TICKS(CONFIG_BUTTON_DEBOUNCE_SHORT_TIME * 8),
			pdFALSE,
			(void *)me,
			click_timer_handler);

	if (me->click_timer == NULL) {
		ESP_LOGE(TAG, "Failed to allocate memory to timer");
		return ESP_ERR_NO_MEM;
	}

	/* Return ESP_OK */
	return ret;
}

esp_err_t button_add_cb(button_t *const me, button_click_e click_type,
		button_cb_t function, void *arg) {
	ESP_LOGI(TAG, "Adding callback for button...");

	/* Error code variable */
	esp_err_t ret = ESP_OK;

	/* Check function argument */
	if (function == NULL) {
		ESP_LOGI(TAG, "Invalid function argument");
		return ESP_ERR_INVALID_ARG;
	}

	if (click_type < BUTTON_CLICK_SINGLE || click_type >= BUTTON_CLICK_MAX) {
		ESP_LOGI(TAG, "Invalid mode");
		ret = ESP_ERR_INVALID_ARG;
	}

	/* Assign function and argument values */
	me->function[click_type].function = function;
	me->function[click_type].arg = arg;

	/* Return ESP_OK */
	return ret;
}

esp_err_t button_remove_cb(button_t *const me, button_click_e click_type) {
	ESP_LOGI(TAG, "Removing button callback function...");

	/* Error code variable */
	esp_err_t ret = ESP_OK;

	if (click_type < BUTTON_CLICK_SINGLE || click_type >= BUTTON_CLICK_MAX) {
		ESP_LOGI(TAG, "Invalid mode");
		ret = ESP_ERR_INVALID_ARG;
	}

	/* Assign function and argument values */
	me->function[click_type].function = NULL;
	me->function[click_type].arg = NULL;

	/* Return ESP_OK */
	return ret;
}
/* Private functions ---------------------------------------------------------*/
static void isr_handler(void *arg) {
	button_t *button = (button_t *)arg;

	static TickType_t elapsed_time = 0;

	switch (button->state) {
		case BUTTON_STATE_DOWN:
			/* Disable button interrupt */
			gpio_set_intr_type(button->gpio, GPIO_INTR_DISABLE);

			if (button->edge == BUTTON_EDGE_FALLING) {
				/* Get initial tick counter */
				button->tick_counter = xTaskGetTickCountFromISR();
			}
			else {
				/* Calculate and print button elapsed time pressed */
				elapsed_time = pdTICKS_TO_MS(xTaskGetTickCountFromISR() - button->tick_counter);
#ifdef BUTTON_ENABLE_DEBUG
				ESP_DRAM_LOGI(TAG, "button %d pressed for %d ms", button->gpio, elapsed_time);
#endif /* BUTTON_ENABLE_DEBUG */

				/* Execute function callback according button elapsed time pressed */
				if (elapsed_time >= CONFIG_BUTTON_DEBOUNCE_SHORT_TIME && elapsed_time < CONFIG_BUTTON_DEBOUNCE_MEDIUM_TIME) {
					/* Increment click counter */
					button->click_counter++;

					if (button->click_counter == 2) {
						button->click_counter = 0;
						xEventGroupSetBitsFromISR(button->event_group, BUTTON_DOUBLE_CLICK_PRESS_BIT, NULL);
					}

					/* Start click timer */
					xTimerStartFromISR(button->click_timer, NULL);
				}
				else if ((elapsed_time >= CONFIG_BUTTON_DEBOUNCE_MEDIUM_TIME && elapsed_time < CONFIG_BUTTON_DEBOUNCE_LONG_TIME)) {
					xEventGroupSetBitsFromISR(button->event_group, BUTTON_MEDIUM_PRESS_BIT, NULL);
				}
				else if (elapsed_time >= CONFIG_BUTTON_DEBOUNCE_LONG_TIME) {
					xEventGroupSetBitsFromISR(button->event_group, BUTTON_LONG_PRESS_BIT, NULL);
				}
				else {
#ifdef BUTTON_ENABLE_DEBUG
					ESP_DRAM_LOGI(TAG, "button %d bounce", button->gpio);
#endif /* BUTTON_ENABLE_DEBUG */
					break;
				}

				/* Reset button elapsed time pressed */
				elapsed_time = 0;
			}

			/* Start debounce timer */
			xTimerStartFromISR(button->debounce_timer, NULL);

			break;

		case BUTTON_STATE_UP:
			/* Disable button interrupt */
			gpio_set_intr_type(button->gpio, GPIO_INTR_DISABLE);

			if (button->edge == BUTTON_EDGE_FALLING) {
				/* Calculate and print button elapsed time pressed */
				elapsed_time = pdTICKS_TO_MS(xTaskGetTickCountFromISR() - button->tick_counter);
#ifdef BUTTON_ENABLE_DEBUG
				ESP_DRAM_LOGI(TAG, "button %d pressed for %d ms", button->gpio, elapsed_time);
#endif /* BUTTON_ENABLE_DEBUG */

				/* Execute function callback according button elapsed time pressed */
				if (elapsed_time >= CONFIG_BUTTON_DEBOUNCE_SHORT_TIME && elapsed_time < CONFIG_BUTTON_DEBOUNCE_MEDIUM_TIME) {
					/* Increment click counter */
					button->click_counter++;

					if (button->click_counter == 2) {
						button->click_counter = 0;
						xEventGroupSetBitsFromISR(button->event_group, BUTTON_DOUBLE_CLICK_PRESS_BIT, NULL);
					}

					/* Start click timer */
					xTimerStartFromISR(button->click_timer, NULL);
				}
				else if ((elapsed_time >= CONFIG_BUTTON_DEBOUNCE_MEDIUM_TIME && elapsed_time < CONFIG_BUTTON_DEBOUNCE_LONG_TIME)) {
					xEventGroupSetBitsFromISR(button->event_group, BUTTON_MEDIUM_PRESS_BIT, NULL);
				}
				else if (elapsed_time >= CONFIG_BUTTON_DEBOUNCE_LONG_TIME) {
					xEventGroupSetBitsFromISR(button->event_group, BUTTON_LONG_PRESS_BIT, NULL);
				}
				else {
#ifdef BUTTON_ENABLE_DEBUG
					ESP_DRAM_LOGI(TAG, "button %d bounce", button->gpio);
#endif /* BUTTON_ENABLE_DEBUG */
					break;
				}

				/* Reset button elapsed time pressed */
				elapsed_time = 0;
			}
			else {
				/* Get initial tick counter */
				button->tick_counter = xTaskGetTickCountFromISR();
			}

			/* Start debounce timer */
			xTimerStartFromISR(button->debounce_timer, NULL);

			break;

		default:
			break;
	}

	portYIELD_FROM_ISR();
}

static void button_task(void *arg) {
	button_t *button = (button_t *)arg;
	EventBits_t bits;
	const EventBits_t bits_wait_for = (BUTTON_SHORT_PRESS_BIT |
			BUTTON_MEDIUM_PRESS_BIT |
			BUTTON_LONG_PRESS_BIT |
			BUTTON_DOUBLE_CLICK_PRESS_BIT);

	for (;;) {
		/* Wait until some bit is set */
		bits = xEventGroupWaitBits(button->event_group,
				bits_wait_for,
				pdTRUE,
				pdFALSE,
				portMAX_DELAY);

		if (bits & BUTTON_SHORT_PRESS_BIT) {
			ESP_LOGI(TAG, "BUTTON_SHORT_PRESS_BIT set");

			/* Execute callback function */
			if (button->function[BUTTON_CLICK_SINGLE].function != NULL) {
				button->function[BUTTON_CLICK_SINGLE].function(button->function[BUTTON_CLICK_SINGLE].arg);
			}
			else {
				ESP_LOGW(TAG, "Function callback not added");
			}
		}

		else if (bits & BUTTON_MEDIUM_PRESS_BIT) {
			ESP_LOGI(TAG, "BUTTON_MEDIUM_PRESS_BIT set");

			/* Execute callback function */
			if (button->function[BUTTON_CLICK_MEDIUM].function != NULL) {
				button->function[BUTTON_CLICK_MEDIUM].function(button->function[BUTTON_CLICK_MEDIUM].arg);
			}
			else {
				ESP_LOGW(TAG, "Function callback not added");
			}
		}
		else if (bits & BUTTON_LONG_PRESS_BIT) {
			ESP_LOGI(TAG, "BUTTON_LONG_PRESS_BIT set");

			/* Execute callback function */
			if (button->function[BUTTON_CLICK_LONG].function != NULL) {
				button->function[BUTTON_CLICK_LONG].function(button->function[BUTTON_CLICK_LONG].arg);
			}
			else {
				ESP_LOGW(TAG, "Function callback not added");
			}
		}
		else if (bits & BUTTON_DOUBLE_CLICK_PRESS_BIT) {
			ESP_LOGI(TAG, "BUTTON_DOUBLE_CLICK_PRESS_BIT set");

			/* Execute callback function */
			if (button->function[BUTTON_CLICK_DOUBLE].function != NULL) {
				button->function[BUTTON_CLICK_DOUBLE].function(button->function[BUTTON_CLICK_DOUBLE].arg);
			}
			else {
				ESP_LOGW(TAG, "Function callback not added");
			}
		}
		else {
			ESP_LOGI(TAG, "Button unexpected event");
		}
	}
}

static void debounce_timer_handler(TimerHandle_t timer) {
	/* Get instance data */
	button_t *button = (button_t *)pvTimerGetTimerID(timer);

	/*  */
	if (button->edge == BUTTON_EDGE_FALLING) {
		if (button->state == BUTTON_STATE_DOWN) {
			if (!gpio_get_level(button->gpio)) {
				/* Change to next state */
				button->state = BUTTON_STATE_UP;

				/* Enable button interrupt to detect positive edge */
				gpio_set_intr_type(button->gpio, GPIO_INTR_POSEDGE);
			}
			else {
				/* Enable button interrupt to detect negative edge */
				gpio_set_intr_type(button->gpio, GPIO_INTR_NEGEDGE);
			}

		}
		else {
			if (gpio_get_level(button->gpio)) {
				/* Change to next state */
				button->state = BUTTON_STATE_DOWN;

				/* Enable button interrupt to detect negative edge */
				gpio_set_intr_type(button->gpio, GPIO_INTR_NEGEDGE);
			}
			else {
				/* Enable button interrupt to detect positive edge */
				gpio_set_intr_type(button->gpio, GPIO_INTR_POSEDGE);
			}
		}
	}
	else {
		if (button->state == BUTTON_STATE_UP) {
			if (gpio_get_level(button->gpio)) {
				/* Change to next state */
				button->state = BUTTON_STATE_DOWN;

				/* Enable button interrupt to detect positive edge */
				gpio_set_intr_type(button->gpio, GPIO_INTR_NEGEDGE);
			}
			else {
				/* Enable button interrupt to detect negative edge */
				gpio_set_intr_type(button->gpio, GPIO_INTR_POSEDGE);
			}

		}
		else {
			if (!gpio_get_level(button->gpio)) {
				/* Change to next state */
				button->state = BUTTON_STATE_UP;

				/* Enable button interrupt to detect negative edge */
				gpio_set_intr_type(button->gpio, GPIO_INTR_POSEDGE);
			}
			else {
				/* Enable button interrupt to detect positive edge */
				gpio_set_intr_type(button->gpio, GPIO_INTR_NEGEDGE);
			}
		}
	}
}

static void click_timer_handler(TimerHandle_t timer) {
	/* Get instance data */
	button_t *button = (button_t *)pvTimerGetTimerID(timer);

	/* Single click */
	if (button->click_counter == 1) {
		button->click_counter = 0;
		xEventGroupSetBitsFromISR(button->event_group, BUTTON_SHORT_PRESS_BIT, NULL);
	}
}

/***************************** END OF FILE ************************************/

