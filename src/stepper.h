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

#include <stdio.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#define SLIDER_STATUS_IDLE "Idle"
#define SLIDER_STATUS_RUNNING "Runn"
#define SLIDER_STATUS_HALTED "Halt"
#define SLIDER_STATUS_ERROR "Err"

#define STEPS_FACTOR 100   // <x> steps for 1 mm move
#define MAX_MOTOR_SPEED 30 // min duration of signal for motor driver is 9,6us
// TODO: count precise MAX_MOTOR_SPEED

typedef struct {
  char status[10];
  uint32_t dir;
  uint32_t start_pos;
  uint32_t end_pos;
  uint32_t duration;
  uint32_t speed;
} slider_params;

typedef enum {
  FORWARD = 0,
  REVERSE = 1,
} T_DIR;

slider_params slider_status = {
    .status = SLIDER_STATUS_IDLE,
    .dir = 1,
    .start_pos = 10,
    .end_pos = 20,
    .duration = 10,
    .speed = 100,
};

extern slider_params slider_status;

static const struct gpio_dt_spec stepper_motor_step =
    GPIO_DT_SPEC_GET(DT_NODELABEL(stepper_motor_step), gpios);
static const struct gpio_dt_spec stepper_motor_dir =
    GPIO_DT_SPEC_GET(DT_NODELABEL(stepper_motor_dir), gpios);
static const struct gpio_dt_spec stepper_motor_en =
    GPIO_DT_SPEC_GET(DT_NODELABEL(stepper_motor_en), gpios);

static int stepper_gpio_configure(const struct gpio_dt_spec *pin, char *name) {
  LOG_MODULE_DECLARE(app);
  int err;
  if (!gpio_is_ready_dt(pin)) {
    LOG_ERR("%s gpio isn't ready", name);
    return 1;
  }
  err = gpio_pin_configure_dt(pin, GPIO_OUTPUT_ACTIVE);
  if (err < 0) {
    LOG_ERR("%s gpio configure failed", name);
    return err;
  }
  return 0;
}

int stepper_motor_gpio_init() {
  int err;
  LOG_MODULE_DECLARE(app);
  err = stepper_gpio_configure(&stepper_motor_step, "stepper_motor_step");
  if (err) {
    return err;
  }
  err = stepper_gpio_configure(&stepper_motor_dir, "stepper_motor_dir");
  if (err) {
    return err;
  }
  err = stepper_gpio_configure(&stepper_motor_en, "stepper_motor_en");
  if (err) {
    return err;
  }
  return 0;
}

// Function to move the stepper motor
void slider_process(slider_params *slider) {
  if (!memcmp(slider->status, SLIDER_STATUS_IDLE, 4)) {
    return;
  }
  // TODO: check if start pos and end pos are equal
  //
  if (slider->start_pos == slider->end_pos) {
    return;
  }
  uint8_t dir = slider->start_pos < slider->end_pos ? FORWARD : REVERSE;
  uint32_t steps;
  uint32_t delay = 1000 / slider->speed;
  if (dir == FORWARD) {
    steps = (slider->end_pos - slider->start_pos) * STEPS_FACTOR;
  } else {
    steps = (slider->start_pos - slider->end_pos) * STEPS_FACTOR;
  }

  gpio_pin_set_dt(&stepper_motor_dir, dir); // TODO
  while (steps) {
    for (int i = 0; i < 2; i++) {
      gpio_pin_toggle_dt(&stepper_motor_step);
      k_usleep(100 * delay);
    }
    steps--;
  }
  memcpy(slider->status, SLIDER_STATUS_IDLE, 4);
}

void slider_process_thread() {
  while (1) {
    slider_process(&slider_status);
    k_msleep(200);
  }
}

#endif
