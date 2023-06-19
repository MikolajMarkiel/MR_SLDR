

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

#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

// #define VL53L0X_SAMPLES 1

const struct device *const vl53l0x_dev = DEVICE_DT_GET_ONE(st_vl53l0x);
struct sensor_value rangefinder_value;

int rangefinder_init(void) { return !device_is_ready(vl53l0x_dev); }

int rangefinder_meas() {
  int err;
  err = sensor_sample_fetch_chan(vl53l0x_dev, SENSOR_CHAN_DISTANCE);
  if (err) {
    return -1;
  }
  err =
      sensor_channel_get(vl53l0x_dev, SENSOR_CHAN_DISTANCE, &rangefinder_value);
  if (err) {
    return -2;
  }
  //   int val_mm = 0;
  //   for (int i = 0; i < VL53L0X_SAMPLES; i++) {
  //     err = sensor_sample_fetch_chan(vl53l0x_dev, SENSOR_CHAN_DISTANCE);
  //     if (err) {
  //       return -1;
  //     }
  //     err = sensor_channel_get(vl53l0x_dev, SENSOR_CHAN_DISTANCE,
  //                              &rangefinder_value);
  //     if (err) {
  //       return -2;
  //     }
  //     val_mm = rangefinder_value.val1 * 1000;
  //     val_mm += (rangefinder_value.val2 / 1000);
  //   }
  //   val_mm /= VL53L0X_SAMPLES;
  //   printf("val_mm = %d\n", val_mm);
  //   return val_mm;
  return 0;
}

int distance_to_cm(struct sensor_value *val) {
  return val->val1 * 100 + val->val2 / 10000;
}
