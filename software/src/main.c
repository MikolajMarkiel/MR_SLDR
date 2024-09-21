
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

// #include "rangefinder.h"
// #include "slider_bt.h"
#include "rangefinder.h"
#include "slider.h"
#include "zephyr/drivers/gpio.h"
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app);

// K_THREAD_DEFINE(bt_notify, 1024, bt_notify_handler, NULL, NULL, NULL, 7, 0, 0);

struct appHandler {
    slider_ptr_t slider;
    // rangefinder_ptr_t rangefinder;
};

struct appHandler appHandler;

int app_init(void);

#define GET_GPIO(name, dts) static const struct gpio_dt_spec name = GPIO_DT_SPEC_GET(DT_NODELABEL(dts), gpios)

GET_GPIO(led_1, led1);
GET_GPIO(led_2, led2);

static const struct gpio_dt_spec leds[] = 
{
    led_1,
    led_2,
};

int leds_init()
{
    int result = 0, val = 0;
    size_t num_leds = sizeof(leds)/sizeof(leds[0]);
    for (size_t i = 0; i < num_leds; i++)
    {
        if (0 == gpio_is_ready_dt(&leds[i])) 
        {
            LOG_ERR("led %zu isn't ready", i);
            result = -1;
        }
        else if (0 > (val = gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_ACTIVE))) 
        {
            LOG_ERR("led %zu configure failed %d", i, val);
            result = -2;
        }
    }
    return result;
}

int leds_blocking_test()
{
    k_msleep(1000);
    size_t num_leds = sizeof(leds)/sizeof(leds[0]);
    int result = 0;
    for (size_t i = 0; i < num_leds; i++)
    {
        result = gpio_pin_toggle_dt(&leds[i]);
    }
    return 0;
}

int main(void) 
{
    int result = 0;
    if (0 == (result = app_init())) 
    {
        while (1) 
        {
            //     rangefinder_meas();
            leds_blocking_test();
            k_msleep(100);
        }
    }
    return result;
}

int app_init(void) {
    int result;
    int err;
    if (0 != (err = leds_init())) 
    {
        LOG_ERR("Leds init failed [%d]", err);
        result = -1;
    }
    else if (NULL == (appHandler.slider = slider_init())) 
    {
        LOG_ERR("Slider init failed");
        result = -2;
    }
    // else if (0 != (err = rangefinder_init(appHandler.rangefinder))) {
    //     LOG_ERR("Distance meter init failed (err %d)", err);
    //     result = -3;
    // }
    // else if (0 != (err = slider_bt_init())) {
    //     LOG_ERR("Bluetooth module init failed [%d]", err);
    //     result = -4;
    // }
    else 
    {
        LOG_INF("Application started successfully");
        slider_start(appHandler.slider); // TODO remove
        result = 0;
    }

    return result;
}
