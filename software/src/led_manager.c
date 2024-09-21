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

#include "led_manager.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(led_manager);

// static int led_gpio_configure(const struct gpio_dt_spec *pin);

#define GET_GPIO(name, dts) static const struct gpio_dt_spec name = GPIO_DT_SPEC_GET(DT_NODELABEL(dts), gpios)

GET_GPIO(led_1, led1);
GET_GPIO(led_2, led2);

static const struct gpio_dt_spec leds[] = 
{
    led_1,
    led_2,
};

int leds_init(void)
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

int leds_blocking_test(void)
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


// static int led_gpio_configure(const struct gpio_dt_spec *pin) 
// {
//     int result = 0;
//     if (NULL == pin)
//     {
//         LOG_ERR("led_gpio_configure: null params");
//         result = -1;
//     }
//     else if (0 == gpio_is_ready_dt(pin)) 
//     {
//         LOG_ERR("led_gpio_configure: led gpio isn't ready");
//         result = -2;
//     }
//     else if (0 > (result = gpio_pin_configure_dt(pin, GPIO_OUTPUT_INACTIVE))) 
//     {
//         LOG_ERR("led_gpio_configure: gpio configure failed");
//         result = -3;
//     }
//     return result;
// }
