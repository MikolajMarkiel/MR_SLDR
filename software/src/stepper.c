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

#include "stepper.h"
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#define TIMER timer0
#define STEPPER_MOTOR_ENABLE() gpio_pin_set_dt(&stepper_motor_en, 0)
#define STEPPER_MOTOR_DISABLE() gpio_pin_set_dt(&stepper_motor_en, 1)

#define CB_FOR_ONE_STEP 2
#define US_IN_ONE_SEC 1000000
#define CB_DELAY(spd) US_IN_ONE_SEC / spd / CB_FOR_ONE_STEP
typedef enum {
  FORWARD = 0,
  REVERSE = 1,
} T_DIR;

slider_params slider;
uint32_t timer_status;

struct counter_alarm_cfg alarm_cfg;

static const struct gpio_dt_spec stepper_motor_step =
    GPIO_DT_SPEC_GET(DT_NODELABEL(stepper_motor_step), gpios);
static const struct gpio_dt_spec stepper_motor_dir =
    GPIO_DT_SPEC_GET(DT_NODELABEL(stepper_motor_dir), gpios);
static const struct gpio_dt_spec stepper_motor_en =
    GPIO_DT_SPEC_GET(DT_NODELABEL(stepper_motor_en), gpios);

static uint32_t steps;

static void select_dir(slider_params *slider) {
  if (slider->end_pos > slider->start_pos) {
    slider->dir = FORWARD;
  } else {
    slider->dir = REVERSE;
  }
}
static int timer_enable(const struct device *dev) {
  timer_status = 1;
  return counter_start(dev);
}

static int timer_disable(const struct device *dev) {
  timer_status = 0;
  return counter_stop(dev);
}

static void count_steps(slider_params *slider) {
  select_dir(slider);
  if (slider->dir == FORWARD) {
    slider->steps = (slider->end_pos - slider->start_pos) * STEPS_FACTOR;
  } else {
    slider->steps = (slider->start_pos - slider->end_pos) * STEPS_FACTOR;
  }
}

static void count_duration(slider_params *slider) {
  // TODO
  slider->duration = 0;
}

static void slider_init_params(slider_params *slider) {
  //   memcpy(slider->status, SLIDER_STATUS_IDLE, sizeof(SLIDER_STATUS_IDLE));
  memcpy(slider->status, SLIDER_STATUS_RUNNING, sizeof(SLIDER_STATUS_RUNNING));
  slider->start_pos = DEFAULT_START_POS;
  slider->end_pos = DEFAULT_END_POS;
  slider->speed = DEFAULT_SPEED;
  slider->dir = DEFAULT_DIR;
  slider->interval_steps = DEFAULT_INTERVAL_STEPS;
  slider->interval_delay = DEFAULT_INTERVAL_DELAY;
  slider->soft_start = DEFAULT_SOFT_START;
  count_steps(slider);
  count_duration(slider);
}

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

const struct device *const counter_dev = DEVICE_DT_GET(DT_NODELABEL(TIMER));

static void counter_callback(const struct device *counter_dev, uint8_t chan_id,
                             uint32_t ticks, void *user_data) {
  static uint8_t pin_state = 0;
  uint32_t *steps = user_data;
  pin_state ^= 1;
  if (!pin_state) {
    (*steps)--;
  }
  gpio_pin_set_dt(&stepper_motor_step, pin_state);
  if (*steps == 0) {
    timer_disable(counter_dev);
    STEPPER_MOTOR_DISABLE();
  } else {
    counter_set_channel_alarm(counter_dev, ALARM_CHANNEL_ID, &alarm_cfg);
  }
  return;
}

int stepper_motor_init(void) {
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

  err = gpio_pin_set_dt(&stepper_motor_step, 0); // TODO set in dt
  if (err) {
    return err;
  }

  slider_init_params(&slider);
  if (!device_is_ready(counter_dev)) {
    LOG_ERR("counter_dev not ready.\n");
    return -ENODEV;
  }

  alarm_cfg.flags = 0;
  alarm_cfg.callback = counter_callback;
  //   alarm_cfg.user_data = NULL;

  LOG_INF("stepper motor init successfull\n");
  return 0;
}
static int set_interval_process() {
  int err;
  uint32_t delay;
  const uint32_t min_delay = counter_us_to_ticks(counter_dev, MIN_MOTOR_DELAY);
  LOG_MODULE_DECLARE(app);
  gpio_pin_set_dt(&stepper_motor_dir, slider.dir);
  delay = counter_us_to_ticks(counter_dev, CB_DELAY(slider.speed));
  delay = delay < min_delay ? min_delay : delay;
  STEPPER_MOTOR_ENABLE();
  alarm_cfg.ticks = counter_us_to_ticks(counter_dev, delay);
  err = timer_enable(counter_dev);
  if (err) {
  } // TODO
  err = counter_set_channel_alarm(counter_dev, ALARM_CHANNEL_ID, &alarm_cfg);
  if (err) {
  } // TODO
  return 0;
}

void slider_process_thread() {
  LOG_MODULE_DECLARE(app);
  int err;
  static uint32_t steps_per_interval;
  static uint32_t c_interval;
  static uint32_t c_step;
  alarm_cfg.user_data = &c_step;
  while (1) {
    if (memcmp(slider.status, SLIDER_STATUS_RUNNING, 4)) {
      k_msleep(100);
      continue;
    }
    steps_per_interval = slider.steps / slider.interval_steps;
    for (c_interval = 1; c_interval <= slider.interval_steps; c_interval++) {
      c_step = steps_per_interval;
      LOG_INF("interval %d start", c_interval);
      set_interval_process();
      while (timer_status) {
        k_msleep(1);
      }
      k_msleep(slider.interval_delay);
    }
    memcpy(slider.status, SLIDER_STATUS_IDLE, 4);
  }
}
