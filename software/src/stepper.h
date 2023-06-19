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

#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#define SLIDER_STATUS_IDLE "Idle"
#define SLIDER_STATUS_RUNNING "Runn"
#define SLIDER_STATUS_HALTED "Halt"
#define SLIDER_STATUS_ERROR "Erro"
#define SLIDER_STATUS_CALIB "Cali"

#define STEPS_FACTOR 100 // <x> steps for 1 mm move
#define MIN_MOTOR_DELAY 50

#define DEFAULT_START_POS 0
#define DEFAULT_END_POS 50
#define DEFAULT_SPEED 1000
#define DEFAULT_DIR 1
#define DEFAULT_INTERVAL_STEPS 2
#define DEFAULT_INTERVAL_DELAY 250
#define DEFAULT_SOFT_START 500

typedef struct slider_params {
  char status[10];
  uint32_t dir;
  uint32_t start_pos;
  uint32_t end_pos;
  uint32_t duration;
  uint32_t speed;
  uint32_t steps;
  uint32_t interval_steps;
  uint32_t interval_delay;
  uint32_t soft_start;
} slider_params;

extern slider_params slider;

int stepper_motor_init(void);

void slider_stop();
int slider_calib();
void slider_thread();

#ifdef __cplusplus
}
#endif

#endif // __STEPPER_HEADER__
