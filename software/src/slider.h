/*
Copyright (c) 2023 Mikolaj Markiel

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
*/

#ifndef __STEPPER_HEADER__
#define __STEPPER_HEADER__

#ifdef __cplusplus
extern "C" {
#endif


#include "stepper_motor.h"  
#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

typedef enum 
{
  sliderStatus_idle,
  sliderStatus_running,
  sliderStatus_halted, 
  sliderStatus_error,
  sliderStatus_calib,
  sliderStatus_end
} sliderStatus_t;

#define STEPS_FACTOR 100 // <x> steps for 1 mm move
#define MIN_MOTOR_DELAY 50

#ifndef DEFAULT_START_POS
  #define DEFAULT_START_POS 0
#endif // !DEFAULT_START_POS

#ifndef DEFAULT_END_POS
  #define DEFAULT_END_POS 50
#endif // !DEFAULT_END_POS

#ifndef DEFAULT_SPEED
  #define DEFAULT_SPEED 1000
#endif // !DEFAULT_SPEED

#ifndef DEFAULT_DIR
  #define DEFAULT_DIR stepperMotor_dir_forward
#endif // !DEFAULT_DIR

// the default number of intervals in single slider process
#ifndef DEFAULT_INTERVALS
  #define DEFAULT_INTERVALS 4
#endif // !DEFAULT_INTERVALS

#ifndef DEFAULT_INTERVAL_DELAY
  #define DEFAULT_INTERVAL_DELAY 250
#endif // !DEFAULT_INTERVAL_DELAY

#ifndef DEFAULT_SOFT_START
  #define DEFAULT_SOFT_START 500
#endif // !DEFAULT_SOFT_START

#ifndef DISABLE_MOTOR_AT_INTERVALS
  #define DISABLE_MOTOR_AT_INTERVALS true
#endif // !DISABLE_MOTOR_AT_INTERVALS

// Stepper Motor DTS
#define MOTOR_GPIO_EN_DTS     stepper_motor_en
#define MOTOR_GPIO_DIR_DTS    stepper_motor_dir
#define MOTOR_GPIO_STEP_DTS   stepper_motor_step
#define MOTOR_GPIO_RESET_DTS  stepper_motor_reset

typedef struct slider* slider_ptr_t;
typedef struct slider_thread_data* slider_thread_data_ptr_t;

slider_ptr_t slider_init(void);
int slider_deInit(slider_ptr_t pHandle);
int slider_stop(slider_ptr_t pHandle);
int slider_start(slider_ptr_t pHandle);

#ifdef __cplusplus
}
#endif

#endif // __STEPPER_HEADER__
