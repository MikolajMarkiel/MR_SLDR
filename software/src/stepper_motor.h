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

#ifndef __STEPPER_MOTOR_HEADER__
#define __STEPPER_MOTOR_HEADER__

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/drivers/gpio.h>

typedef struct stepperMotor stepperMotor_t;

typedef stepperMotor_t* stepperMotor_ptr_t;

typedef enum 
{
  stepperMotor_dir_forward = 0,
  stepperMotor_dir_reverse = 1,
} stepperMotor_dir_t;

// return NULL on error, stepperMotor_ptr_t on success
stepperMotor_t* stepperMotor_init(struct gpio_dt_spec gpio_step, struct gpio_dt_spec gpio_en, struct gpio_dt_spec gpio_dir, struct gpio_dt_spec gpio_reset);

// return negative on error, 0 on success
int stepperMotor_deinit(stepperMotor_t* pHandle);

// return negative on error, 0 on success
int stepperMotor_setDir(stepperMotor_t* const handle, stepperMotor_dir_t dir);

// return negative on error, 0 on success
int stepperMotor_getDir(stepperMotor_t* const handle, stepperMotor_dir_t * dir);

// return true if stepper motor is enabled or false if is disabled
bool stepperMotor_isEnabled(stepperMotor_t* handle);

// return negative on error, 0 on success
int stepperMotor_enable(stepperMotor_t* const handle);

// return negative on error, 0 on success
int stepperMotor_disable(stepperMotor_t* const handle);

// return negative on error, 0 on reseting gpio 1 on setting gpio
int stepperMotor_step(stepperMotor_t * handle);

#ifdef __cplusplus
}
#endif

#endif // __STEPPER_MOTOR_HEADER__
