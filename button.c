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

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

#include "driver/gpio.h"

/* External variables --------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
#define BUTTON_DEBOUNCE_MS_DEFAULT 40
#define BUTTON_HOLD_MS_DEFAULT 500
#define BUTTON_WAIT_DOUBLE_MS_DEFAULT 100
#define BUTTON_LONG_MS_DEFAULT (3000 - BUTTON_HOLD_MS_DEFAULT)

#define BUTTON_TICK_MS_DEFAULT 20
#define BUTTON_NUM_MAX_DEFAULT 5

#define BUTTON_TASK_PRIORITY_DEFAULT 1
#define BUTTON_TASK_STACK_SIZE_DEFAULT 4096
#define BUTTON_QUEUE_LEN_DEFAULT 10

#ifndef BUTTON_TICK_MS
#ifdef CONFIG_BUTTON_TICK_MS
#define BUTTON_TICK_MS CONFIG_BUTTON_TICK_MS
#else
#define BUTTON_TICK_MS BUTTON_TICK_MS_DEFAULT
#endif /* CONFIG_BUTTON_TICK_MS */
#endif /* BUTTON_TICK_MS */

#ifndef BUTTON_NUM_MAX
#ifdef CONFIG_BUTTON_NUM_MAX
#define BUTTON_NUM_MAX CONFIG_BUTTON_NUM_MAX
#else
#define BUTTON_NUM_MAX BUTTON_NUM_MAX_DEFAULT
#endif /* CONFIG_BUTTON_NUM_MAX */
#endif /* BUTTON_NUM_MAX */

#ifndef BUTTON_TASK_PRIORITY
#ifdef CONFIG_BUTTON_TASK_PRIORITY
#define BUTTON_TASK_PRIORITY CONFIG_BUTTON_TASK_PRIORITY
#else
#define BUTTON_TASK_PRIORITY BUTTON_TASK_PRIORITY_DEFAULT
#endif /* CONFIG_BUTTON_TASK_PRIORITY */
#endif /* BUTTON_TASK_PRIORITY */

#ifndef BUTTON_TASK_STACK_SIZE
#ifdef CONFIG_BUTTON_TASK_STACK_SIZE
#define BUTTON_TASK_STACK_SIZE CONFIG_BUTTON_TASK_STACK_SIZE
#else
#define BUTTON_TASK_STACK_SIZE BUTTON_TASK_STACK_SIZE_DEFAULT
#endif /* CONFIG_BUTTON_TASK_STACK_SIZE */
#endif /* BUTTON_TASK_STACK_SIZE */

#ifndef BUTTON_QUEUE_LEN
#ifdef CONFIG_BUTTON_QUEUE_LEN
#define BUTTON_QUEUE_LEN CONFIG_BUTTON_QUEUE_LEN
#else
#define BUTTON_QUEUE_LEN BUTTON_QUEUE_LEN_DEFAULT
#endif /* CONFIG_BUTTON_QUEUE_LEN */
#endif /* BUTTON_QUEUE_LEN */

/* Private function prototypes -----------------------------------------------*/
/* FSM utils */
static uint32_t get_ms(void);
static bool eval_eq(int a, int b);
static void fsm_set(button_t *button);
static void timer_handler(TimerHandle_t timer);

/* FSM actions */
static void on_single_click(void *arg);
static void on_double_click(void *arg);
static void on_pressed_click(void *arg);
static void on_hold_click(void *arg);
static void on_long_click(void *arg);

/* Dispatcher */
static void dispatcher_task(void *arg);
static button_err_t dispatcher_init(void);
static void dispatcher_send(button_t *button, button_type_t type);

/* Generic GPIO */
static button_err_t generic_gpio_init(uint8_t num, uint8_t port);
static int generic_gpio_get_level(uint8_t num, uint8_t port);

/* Private variables ---------------------------------------------------------*/
static TaskHandle_t button_task_handle = NULL;
static QueueHandle_t button_queue_handle = NULL;
static TimerHandle_t button_timer_handle = NULL;
static uint8_t button_num = 0;
static button_t *button_list[BUTTON_NUM_MAX] = {NULL};
static portMUX_TYPE button_spinlock = portMUX_INITIALIZER_UNLOCKED;

/* Exported functions --------------------------------------------------------*/
/**
 * @brief Initialize a button instance
 */
button_err_t button_init(button_t *const me, int gpio_num, int gpio_port) {
  /* Error code variable */
  button_err_t err = BUTTON_ERR_OK;

  portENTER_CRITICAL(&button_spinlock);
  /* Check the buttons already initialized */
  if (button_num >= BUTTON_NUM_MAX) {
    portEXIT_CRITICAL(&button_spinlock);
    return BUTTON_ERR_NUM_MAX;
  }

  /* Assign button_num as button instance ID and add to list */
  me->id = button_num++;
  button_list[me->id] = me;
  portEXIT_CRITICAL(&button_spinlock);

  /* Initialize GPIO */
  err = generic_gpio_init(gpio_num, gpio_port);

  if (err != BUTTON_ERR_OK) {
    portENTER_CRITICAL(&button_spinlock);
    button_list[me->id] = NULL;
    button_num--;
    portEXIT_CRITICAL(&button_spinlock);
    return err;
  }

  me->gpio_num = gpio_num;
  me->gpio_port = gpio_port;
  me->gpio_level = generic_gpio_get_level(me->gpio_num, 0);

  /* Initialize actions function and argument pointers */
  for (uint8_t i = 0; i < BUTTON_TYPE_MAX; i++) {
    me->actions[i].fn = NULL;
    me->actions[i].arg = NULL;
  }

  /* Initialize and configure FSM */
  fsm_set(me);

  /* Initialize queue, task and timer */
  err = dispatcher_init();

  if (err != BUTTON_ERR_OK) {
    return BUTTON_ERR_FAIL;
  }

  /* Return BUTTON_ERR_OK for no errors */
  return err;
}

/**
 * @brief Deinitialize a button instance
 */
button_err_t button_deinit(button_t *const me) {
  /* Error code variable */
  button_err_t err = BUTTON_ERR_OK;

  /* Check if the parameters are valid */
  if (me == NULL || me->id >= button_num || button_list[me->id] != me) {
    return BUTTON_ERR_INVALID_PARAM;
  }

  portENTER_CRITICAL(&button_spinlock);
  /* Shift left buttons anc clear the last entry */
  for (uint8_t i = me->id; i < button_num - 1; i++) {
    button_list[i] = button_list[i + 1];
    button_list[i]->id = i;
  }

  button_list[button_num - 1] = NULL;

  /* Decrement button_num */
  button_num--;

  if (!button_num) {
    xTimerStop(button_timer_handle, 0);
    xTimerDelete(button_timer_handle, 0);
    button_timer_handle = NULL;
    vTaskDelete(button_task_handle);
    button_task_handle = NULL;
    vQueueDelete(button_queue_handle);
    button_queue_handle = NULL;
  }
  portEXIT_CRITICAL(&button_spinlock);

  /* Return BUTTON_ERR_OK for no errors */
  return err;
}

/**
 * @brief Register a button action
 */
button_err_t button_register_action(button_t *const me, button_type_t type,
                                    button_fn_t fn, void *arg) {
  /* Error code variable */
  button_err_t err = BUTTON_ERR_OK;

  /* Check if function is valid */
  if (fn == NULL) {
    return BUTTON_ERR_INVALID_PARAM;
  }

  /* Check if the cilck type is valid */
  if (type < BUTTON_TYPE_SINGLE || type >= BUTTON_TYPE_MAX) {
    return BUTTON_ERR_INVALID_PARAM;
  }

  /* Assign function and argument values */
  me->actions[type].fn = fn;
  me->actions[type].arg = arg;

  /* Return BUTTON_ERR_OK for no errors */
  return err;
}

/**
 * @brief Unregister a button action
 */
button_err_t button_unregister_action(button_t *const me, button_type_t type) {
  /* Error code variable */
  button_err_t err = BUTTON_ERR_OK;

  /* Check if the cilck type is valid */
  if (type < BUTTON_TYPE_SINGLE || type >= BUTTON_TYPE_MAX) {
    return BUTTON_ERR_INVALID_PARAM;
  }

  /* Assign function and argument values */
  me->actions[type].fn = NULL;
  me->actions[type].arg = NULL;

  /* Return BUTTON_ERR_OK for no errors */
  return err;
}

/**
 * @brief Set a specific button timing option
 */
button_err_t button_set_timing(button_t *const me, button_timing_t timing,
                               uint32_t ms) {
  /* Error code variable */
  button_err_t err = BUTTON_ERR_OK;

  switch (timing) {
  case BUTTON_TIMING_DEBOUNCE:
    /* Check if ms is valid */
    if (ms > 100 || ms < BUTTON_TICK_MS * 2) {
      return BUTTON_ERR_INVALID_PARAM;
    }
    *me->cfg.timing.debounce_fw_ms = ms;
    *me->cfg.timing.debounce_bw_ms = ms;
    break;

  case BUTTON_TIMING_HOLD:
    /* Check if ms is valid */
    if (ms > 1000 || ms < 100) {
      return BUTTON_ERR_INVALID_PARAM;
    }
    *me->cfg.timing.hold_ms = ms;
    break;

  case BUTTON_TIMING_LONG:
    /* Check if ms is valid */
    if (ms < 1000 || ms > 10000) {
      return BUTTON_ERR_INVALID_PARAM;
    }
    *me->cfg.timing.long_ms = ms - *me->cfg.timing.hold_ms;
    break;

  case BUTTON_TIMING_WAIT_DOUBLE:
    /* Check if ms is valid */
    if (ms > 500 || ms < 50) {
      return BUTTON_ERR_INVALID_PARAM;
    }
    *me->cfg.timing.wait_double_ms = ms;
    break;

  default:
    /* Return BUTTON_ERR_OK for no errors */
    return BUTTON_ERR_INVALID_PARAM;
  }

  /* Return BUTTON_ERR_OK for no errors */
  return err;
}

/* Private functions ---------------------------------------------------------*/
/* FSM Utils */
static uint32_t get_ms(void) {
  return (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
}

static bool eval_eq(int a, int b) { return (a == b); }

static void fsm_set(button_t *button) {
  fsm_init(&button->fsm, BUTTON_STATE_IDLE, get_ms);

  fsm_trans_t *trans;
  fsm_add_transition(&button->fsm, &trans, BUTTON_STATE_IDLE,
                     BUTTON_STATE_DEBOUNCE);
  fsm_set_events(&button->fsm, trans, &button->gpio_level, 0, eval_eq, 0,
                 FSM_OP_AND);

  fsm_add_transition(&button->fsm, &trans, BUTTON_STATE_DEBOUNCE,
                     BUTTON_STATE_IDLE);
  button->cfg.timing.debounce_bw_ms = &trans->events.timeout;
  fsm_set_events(&button->fsm, trans, &button->gpio_level, 1, eval_eq,
                 BUTTON_DEBOUNCE_MS_DEFAULT, FSM_OP_AND);

  fsm_add_transition(&button->fsm, &trans, BUTTON_STATE_DEBOUNCE,
                     BUTTON_STATE_PRESSED);
  button->cfg.timing.debounce_fw_ms = &trans->events.timeout;
  fsm_set_events(&button->fsm, trans, &button->gpio_level, 0, eval_eq,
                 BUTTON_DEBOUNCE_MS_DEFAULT, FSM_OP_AND);

  fsm_add_transition(&button->fsm, &trans, BUTTON_STATE_PRESSED,
                     BUTTON_STATE_HOLD);
  button->cfg.timing.hold_ms = &trans->events.timeout;
  fsm_set_events(&button->fsm, trans, NULL, 0, NULL, BUTTON_HOLD_MS_DEFAULT,
                 FSM_OP_AND);

  fsm_add_transition(&button->fsm, &trans, BUTTON_STATE_PRESSED,
                     BUTTON_STATE_WAIT);
  fsm_set_events(&button->fsm, trans, &button->gpio_level, 1, eval_eq, 0,
                 FSM_OP_AND);

  fsm_add_transition(&button->fsm, &trans, BUTTON_STATE_HOLD,
                     BUTTON_STATE_LONG);
  button->cfg.timing.long_ms = &trans->events.timeout;
  fsm_set_events(&button->fsm, trans, &button->gpio_level, 1, eval_eq,
                 BUTTON_LONG_MS_DEFAULT, FSM_OP_AND);

  fsm_add_transition(&button->fsm, &trans, BUTTON_STATE_HOLD,
                     BUTTON_STATE_SINGLE);
  fsm_set_events(&button->fsm, trans, &button->gpio_level, 1, eval_eq, 0,
                 FSM_OP_AND);

  fsm_add_transition(&button->fsm, &trans, BUTTON_STATE_WAIT,
                     BUTTON_STATE_SINGLE);
  button->cfg.timing.wait_double_ms = &trans->events.timeout;
  fsm_set_events(&button->fsm, trans, NULL, 0, NULL,
                 BUTTON_WAIT_DOUBLE_MS_DEFAULT, FSM_OP_AND);

  fsm_add_transition(&button->fsm, &trans, BUTTON_STATE_WAIT,
                     BUTTON_STATE_DOUBLE);
  fsm_set_events(&button->fsm, trans, &button->gpio_level, 0, eval_eq, 0,
                 FSM_OP_AND);

  fsm_add_transition(&button->fsm, &trans, BUTTON_STATE_LONG,
                     BUTTON_STATE_IDLE);

  fsm_add_transition(&button->fsm, &trans, BUTTON_STATE_SINGLE,
                     BUTTON_STATE_IDLE);

  fsm_add_transition(&button->fsm, &trans, BUTTON_STATE_DOUBLE,
                     BUTTON_STATE_IDLE);
  fsm_set_events(&button->fsm, trans, &button->gpio_level, 1, eval_eq, 0,
                 FSM_OP_AND);

  fsm_register_state_actions(&button->fsm, BUTTON_STATE_SINGLE, on_single_click,
                             button, NULL, NULL, NULL, NULL);
  fsm_register_state_actions(&button->fsm, BUTTON_STATE_DOUBLE, on_double_click,
                             button, NULL, NULL, NULL, NULL);
  fsm_register_state_actions(&button->fsm, BUTTON_STATE_PRESSED,
                             on_pressed_click, button, NULL, NULL, NULL, NULL);
  fsm_register_state_actions(&button->fsm, BUTTON_STATE_HOLD, NULL, NULL,
                             on_hold_click, button, NULL, NULL);
  fsm_register_state_actions(&button->fsm, BUTTON_STATE_LONG, on_long_click,
                             button, NULL, NULL, NULL, NULL);
}

static void timer_handler(TimerHandle_t timer) {
  for (uint8_t i = 0; i < button_num; i++) {
    button_t *button = button_list[i];
    button->gpio_level = generic_gpio_get_level(button->gpio_num, 0);
    fsm_run(&button->fsm);
  }
}

/* FSM actions */
static void on_single_click(void *arg) {
  dispatcher_send((button_t *)arg, BUTTON_TYPE_SINGLE);
}

static void on_double_click(void *arg) {
  dispatcher_send((button_t *)arg, BUTTON_TYPE_DOUBLE);
}
static void on_pressed_click(void *arg) {
  dispatcher_send((button_t *)arg, BUTTON_TYPE_PRESSED);
}
static void on_hold_click(void *arg) {
  dispatcher_send((button_t *)arg, BUTTON_TYPE_HOLD);
}
static void on_long_click(void *arg) {
  dispatcher_send((button_t *)arg, BUTTON_TYPE_LONG);
}

/* Dispatcher */
static void dispatcher_task(void *arg) {
  button_msg_t cmd;

  for (;;) {
    xQueueReceive(button_queue_handle, &cmd, portMAX_DELAY);

    if (cmd.action.fn != NULL) {
      cmd.action.fn(cmd.action.arg);
    }
  }
}

static button_err_t dispatcher_init(void) {
  button_err_t err = BUTTON_ERR_OK;

  /* Create task queue */
  if (button_queue_handle == NULL) {
    button_queue_handle =
        xQueueCreate(BUTTON_QUEUE_LEN * BUTTON_NUM_MAX, sizeof(button_msg_t));

    if (button_queue_handle == NULL) {
      return BUTTON_ERR_NO_MEM;
    }
  }

  /* Create RTOS task */
  if (button_task_handle == NULL) {
    BaseType_t status;
    status = xTaskCreate(dispatcher_task, "Button Task", BUTTON_TASK_STACK_SIZE,
                         NULL, BUTTON_TASK_PRIORITY, &button_task_handle);

    if (status != pdPASS) {
      return BUTTON_ERR_NO_MEM;
    }
  }

  /* Create tick timer */
  if (button_timer_handle == NULL) {
    button_timer_handle =
        xTimerCreate("Button Timer", pdMS_TO_TICKS(BUTTON_TICK_MS), pdTRUE,
                     NULL, timer_handler);

    if (button_timer_handle == NULL) {
      return BUTTON_ERR_NO_MEM;
    }

    xTimerStart(button_timer_handle, 0);
  }

  /* Return BUTTON_ERR_OK for success */
  return err;
}

static void dispatcher_send(button_t *button, button_type_t type) {
  button_msg_t cmd;
  cmd.id = button->id;
  cmd.action = button->actions[type];

  if (cmd.action.fn != NULL) {
    if (type == BUTTON_TYPE_HOLD) {
      xQueueSendToBack(button_queue_handle, &cmd, 0);
    } else {
      xQueueSendToFront(button_queue_handle, &cmd, 0);
    }
  }
}

/* Generic GPIO */
static button_err_t generic_gpio_init(uint8_t num, uint8_t port) {
  button_err_t err = BUTTON_ERR_OK;

  gpio_config_t gpio_cfg;
  gpio_cfg.pin_bit_mask = 1ULL << num;
  gpio_cfg.mode = GPIO_MODE_INPUT;
  gpio_cfg.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpio_cfg.intr_type = GPIO_INTR_DISABLE;
  err = gpio_config(&gpio_cfg);

  if (err == ESP_ERR_INVALID_ARG) {
    return BUTTON_ERR_INVALID_PARAM;
  }

  if (err != ESP_OK) {
    return BUTTON_ERR_FAIL;
  }

  return err;
}

static int generic_gpio_get_level(uint8_t num, uint8_t port) {
  return gpio_get_level(num);
}

/***************************** END OF FILE ************************************/
