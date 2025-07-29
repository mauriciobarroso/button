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
#include "fsm.h"

#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2 ||                    \
    CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32C2 ||                  \
    CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6 ||                  \
    CONFIG_IDF_TARGET_ESP32P4
#define ESP32_TARGET
#endif

/* Exported types ------------------------------------------------------------*/
typedef enum {
  BUTTON_ERR_NOT_INIT = -5,
  BUTTON_ERR_NUM_MAX = -4,
  BUTTON_ERR_INVALID_PARAM = -3,
  BUTTON_ERR_NO_MEM = -2,
  BUTTON_ERR_FAIL = -1,
  BUTTON_ERR_OK = 0,
} button_err_t;

typedef void (*button_fn_t)(void *arg);

/* Button action */
typedef struct {
  button_fn_t fn;
  void *arg;
} button_action_t;

/* Button FSM states */
typedef enum {
  BUTTON_STATE_IDLE = 0,
  BUTTON_STATE_DEBOUNCE,
  BUTTON_STATE_PRESSED,
  BUTTON_STATE_HOLD,
  BUTTON_STATE_WAIT,
  BUTTON_STATE_SINGLE,
  BUTTON_STATE_DOUBLE,
  BUTTON_STATE_LONG
} button_state_t;

/* Button type of click */
typedef enum {
  BUTTON_TYPE_SINGLE = 0,
  BUTTON_TYPE_DOUBLE,
  BUTTON_TYPE_PRESSED,
  BUTTON_TYPE_HOLD,
  BUTTON_TYPE_LONG,
  BUTTON_TYPE_MAX
} button_type_t;

typedef struct {
  uint8_t id;
  button_action_t action;
} button_msg_t;

typedef enum {
  BUTTON_TIMING_DEBOUNCE = 0,
  BUTTON_TIMING_HOLD,
  BUTTON_TIMING_LONG,
  BUTTON_TIMING_WAIT_DOUBLE
} button_timing_t;

typedef struct {
  struct {
    uint32_t *debounce_fw_ms;
    uint32_t *debounce_bw_ms;
    uint32_t *hold_ms;
    uint32_t *wait_double_ms;
    uint32_t *long_ms;
  } timing;
} button_cfg_t;

typedef struct {
  int gpio_num;
  int gpio_port;
  int gpio_level;
  uint8_t id;
  button_cfg_t cfg;
  fsm_t fsm;
  button_action_t actions[BUTTON_TYPE_MAX];
} button_t;

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
/**
 * @brief Initialize a button instance
 *
 * @param me        : Pointer to button_t structure
 * @param gpio_num  : GPIO number to attach button
 * @param gpio_port : GPIO port of GPIO number
 *
 * @retval
 * 	- BUTTON_ERR_OK: succeed
 * 	- BUTTON_ERR_FAIL: failed
 */
button_err_t button_init(button_t *const me, int gpio_num, int gpio_port);

/**
 * @brief Deinitialize a button instance
 *
 * @param me : Pointer to button_t structure
 *
 * @retval
 * 	- BUTTON_ERR_OK: succeed
 * 	- BUTTON_ERR_FAIL: failed
 */
button_err_t button_deinit(button_t *const me);

/**
 * @brief Register a button action
 *
 * @param me   : Pointer to button_t structure
 * @param type : Button press type. Can be SINGLE, DOUBLE, PRESSED, HOLD or LONG
 * @param fn   : Function code
 * @param arg  : Pointer to function argument
 *
 * @retval
 * 	- BUTTON_ERR_OK: succeed
 * 	- BUTTON_ERR_INVALID_PARAM: invalid parameter
 */
button_err_t button_register_action(button_t *const me, button_type_t type,
                                    button_fn_t fn, void *arg);

/**
 * @brief Unregister a button action
 *
 * @param me   : Pointer to button_t structure
 * @param type : Button press type. Can be SINGLE, DOUBLE, PRESSED, HOLD or LONG
 *
 * @retval
 * 	- BUTTON_ERR_OK: succeed
 * 	- BUTTON_ERR_INVALID_PARAM: invalid parameter
 */
button_err_t button_unregister_action(button_t *const me, button_type_t type);

/**
 * @brief Set a specific button timing option. This function never must be
 * called before button_init
 *
 * @param me     : Pointer to button_t structure
 * @param timing : Button timing. Can be DEBOUNCE, HOLD, LONG or WAIT DOUBLE
 * @param ms     : Time in miliseconds
 *
 * @retval
 * 	- BUTTON_ERR_OK : succeed
 * 	- BUTTON_ERR_INVALID_PARAM: invalid parameter
 */
button_err_t button_set_timing(button_t *const me, button_timing_t timing,
                               uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* BUTTON_H_ */

/***************************** END OF FILE ************************************/
