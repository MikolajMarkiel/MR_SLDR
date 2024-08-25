
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

#include "rangefinder.h"
#include "slider_bt.h"
#include "stepper.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

LOG_MODULE_REGISTER(app);

static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_NODELABEL(led1), gpios);
// static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(DT_NODELABEL(led2), gpios);
// static const struct gpio_dt_spec led3 = GPIO_DT_SPEC_GET(DT_NODELABEL(led3), gpios);
// static const struct gpio_dt_spec led4 = GPIO_DT_SPEC_GET(DT_NODELABEL(led4), gpios);

K_THREAD_DEFINE(bt_notify, 1024, bt_notify_handler, NULL, NULL, NULL, 7, 0, 0);
K_THREAD_DEFINE(slider_id, 2048, slider_thread, NULL, NULL, NULL, 3, 0, 0);

int app_init();

int main(void) {
  int err;

  err = app_init();
  if (err) {
    return err;
  }

  while (1) {
    //     rangefinder_meas();
    gpio_pin_toggle_dt(&led1);
    k_msleep(500);
  }
  return 0;
}

int app_init() {
  int err;
  err = stepper_motor_init();
  if (err) {
    LOG_ERR("Stepper motor init failed (err %d)", err);
    return 1;
  }
  err = !device_is_ready(led1.port);
	{
		LOG_ERR("LED1 init failed");
		return 2;
	}
	err = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);
	if (err != 0)
	{
		return 3;
	}
  // err = rangefinder_init();
  // if (err) {
  //   LOG_ERR("Distance meter init failed (err %d)", err);
  //   return 2;
  // }
  err = slider_bt_init();
  if (err) {
    LOG_ERR("Bluetooth module failed (err %d)", err);
    return 3;
  }
  LOG_INF("Application started succesfully");
  return 0;
}
