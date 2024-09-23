
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
#include "slider_bt.h"
#include "slider.h"
#include "led_manager.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <stdbool.h>

LOG_MODULE_REGISTER(app);

K_THREAD_DEFINE(bt_notify, 1024, bt_notify_handler, NULL, NULL, NULL, 7, 0, 0);

struct appHandler {
    slider_ptr_t slider;
    // rangefinder_ptr_t rangefinder;
};

struct appHandler appHandler;

int app_init(void);

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
    else if (0 != (err = slider_bt_init(appHandler.slider))) {
        LOG_ERR("Bluetooth module init failed [%d]", err);
        result = -4;
    }
    else 
    {
        LOG_INF("Application started successfully");
        // slider_start(appHandler.slider); // TODO remove

        // slider_config_t s_config = {
        //     .start_pos      = 5,
        //     .end_pos        = 3,
        //     .duration       = 10,
        //     .speed          = 10,
        //     .intervals      = 10,
        //     .interval_delay = 10,
        //     .soft_start     = 10,
        // };
        // slider_updateConfig(appHandler.slider, &s_config);



        result = 0;
    }

    return result;
}
