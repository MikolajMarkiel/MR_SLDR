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

#include "stepper_motor.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <string.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(stepper_motor);

struct stepperMotor_gpios
{
    const struct gpio_dt_spec en; 
    const struct gpio_dt_spec reset; 
    const struct gpio_dt_spec step; 
    const struct gpio_dt_spec dir; 
};

struct stepperMotor 
{
    // stepper motor gpios
    const struct stepperMotor_gpios gpio;
    // stepper motor direction
    stepperMotor_dir_t dir;
    // last step gpio status
    bool stepState;
    // stepper motor enabled / disabled
    bool isEnabled;
};

static int stepper_gpio_configure(const struct gpio_dt_spec *pin);

stepperMotor_t* stepperMotor_init(struct gpio_dt_spec gpio_step, struct gpio_dt_spec gpio_en, struct gpio_dt_spec gpio_dir, struct gpio_dt_spec gpio_reset)
{
    int result = -1;
    stepperMotor_t *pHandle = NULL;
    stepperMotor_t handle = {
        .gpio = 
        {
            .step = gpio_step,
            .en = gpio_en,
            .dir = gpio_dir,
            .reset = gpio_reset
        },
        .dir = stepperMotor_dir_forward
    };

    if (NULL == (pHandle = malloc(sizeof(stepperMotor_t))))
    {
        LOG_ERR("stepperMotor_init: Unable to allocate memory");
        result = -1;
    }
    else if (NULL == memcpy(pHandle, &handle, sizeof(handle)))
    {
        LOG_ERR("stepperMotor_init: Unable to copy memory");
        result = -2;
    }
    else if((0 != stepper_gpio_configure(&pHandle->gpio.step))
        || ( 0 != stepper_gpio_configure(&pHandle->gpio.en))
        || ( 0 != stepper_gpio_configure(&pHandle->gpio.reset))
        || ( 0 != stepper_gpio_configure(&pHandle->gpio.dir)))
    {
        LOG_ERR("stepperMotor_init: Couldn't configure stepper gpios");
        result = -3;
    }
    else if(0 != gpio_pin_set_dt(&pHandle->gpio.step,  0)
        ||  0 != gpio_pin_set_dt(&pHandle->gpio.dir,   0))
    {
        LOG_ERR("stepperMotor_init: Couldn't set stepper gpios");
        result = -4;
    }
    else
    {
        //success
        result = 0;
    }

    if(0 > result)
    {
        free(pHandle);
        pHandle = NULL;
    }
    return pHandle;
}

int stepperMotor_deinit(stepperMotor_t* pHandle)
{
    int result = 0;
    if(NULL == pHandle)
    {
        result = -1;
    }
    else if(0 != gpio_pin_set_dt(&pHandle->gpio.step,  0)
        ||  0 != gpio_pin_set_dt(&pHandle->gpio.dir,   0)
        ||  0 != gpio_pin_set_dt(&pHandle->gpio.reset, 0))
    {
        LOG_ERR("stepperMotor_init: Couldn't set stepper gpios");
        result = -4;
    }
    free(pHandle);
    pHandle = NULL;
    return result;
}

int stepperMotor_setDir(stepperMotor_t* const handle, stepperMotor_dir_t dir) 
{
    int result = 0;
    if(NULL == handle)
    {
        LOG_ERR("stepperMotor_setDir: null params");
        result = -1;
    }
    else
    {
        handle->dir = dir;
    } 
    return result;
}

int stepperMotor_getDir(stepperMotor_t* const handle, stepperMotor_dir_t * dir) 
{
    int result = -1;
    if((NULL == handle) || (NULL == dir))
    {
        LOG_ERR("stepperMotor_getDir: null params");
        result = -1;
    }
    else
    {
        *dir = handle->dir;
        result = 0;
    } 
    return result;
}

bool stepperMotor_isEnabled(stepperMotor_t* const handle)
{
    bool result = false;
    if(NULL == handle)
    {
        LOG_ERR("stepperMotor_isEnabled: null params");
    }
    else
    {
        result = handle->isEnabled;
    }
    return result;
}

int stepperMotor_enable(stepperMotor_t* const handle)
{
    int result = 0;
    if(NULL == handle)
    {
        LOG_ERR("stepperMotor_enable: null params");
        result = -1;
    }
    else if(0 != gpio_pin_set_dt(&handle->gpio.dir, handle->dir))
    {
        LOG_ERR("stepperMotor_enable: unable set 'dir' pin");
        result = -2;
    }
    else if(0 != gpio_pin_set_dt(&handle->gpio.en, 1)) // TODO GPIO_OUTPUT_ACTIVE
    {
        LOG_ERR("stepperMotor_enable: unable set 'en' pin");
        result = -3;
    } 
    else 
    {
        handle->isEnabled = true;
        result = 0;
    }
    return result;
}

int stepperMotor_disable(stepperMotor_t* const handle)
{
    int result = 0;
    if(NULL == handle)
    {
        LOG_ERR("stepperMotor_disable: null params");
        result = -1;
    }
    else if(0 != gpio_pin_set_dt(&handle->gpio.dir, 0))
    {
        LOG_ERR("stepperMotor_disable: unable reset 'dir' pin");
        result = -2;
    }
    else if(0 != gpio_pin_set_dt(&handle->gpio.en, 0)) // TODO GPIO_OUTPUT_ACTIVE
    {
        LOG_ERR("stepperMotor_disable: unable reset 'en' pin");
        result = -3;
    } 
    else 
    {
        handle->isEnabled = false;
        result = 0;
    }
    return result;
}

int stepperMotor_step(stepperMotor_t * handle)
{
    int result = 0;
    if (NULL == handle)
    {
        LOG_ERR("stepperMotor_step: null params");
        result = -1;
    }
    else if (false == stepperMotor_isEnabled(handle))
    {
        LOG_ERR("stepperMotor_step: stepper motor is turned off");
        result = -2;
    }
    else 
    {
        handle->stepState ^= 1;
        if (0 != gpio_pin_set_dt(&handle->gpio.step, handle->stepState))
        {
            result = -3;
        }
        else 
        {
            result = handle->stepState;
        }
    }
    return result;
}

static int stepper_gpio_configure(const struct gpio_dt_spec *pin) 
{
    int result = 0;
    if (NULL == pin)
    {
        LOG_ERR("stepper_gpio_configure: null params");
        result = -1;
    }
    else if (0 == gpio_is_ready_dt(pin)) 
    {
        LOG_ERR("stepper_gpio_configure: stepper gpio isn't ready");
        result = -2;
    }
    else if (0 > (result = gpio_pin_configure_dt(pin, GPIO_OUTPUT_INACTIVE))) 
    {
        LOG_ERR("stepper_gpio_configure: gpio configure failed");
        result = -3;
    }
    return result;
}
