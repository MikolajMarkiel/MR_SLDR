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

#include "slider.h"
#include "stepper_motor.h"
// #include "rangefinder.h"

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/kernel/thread.h>
#include <zephyr/device.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>

#define TIMER timer0
#define ALARM_CHANNEL_ID 0

#define STEPPER_MOTOR_ENABLE() gpio_pin_set_dt(&stepper_motor_en, 0)
#define STEPPER_MOTOR_DISABLE() gpio_pin_set_dt(&stepper_motor_en, 1)

#define CB_FOR_ONE_STEP   (2)
#define US_IN_ONE_SEC     (1000000)
#define GET_DELAY(speed)  (US_IN_ONE_SEC / speed / CB_FOR_ONE_STEP)

#define CALIB_MAX_STEPS 0xFFFFFFFF
#define CALIB_SLIDER_SPEED 500
#define CALIB_STEPS_DELAY GET_DELAY(CALIB_SLIDER_SPEED)

#define SLIDER_THREAD_STACK 2048
#define MY_PRIORITY 3

#define GET_GPIO(name, dts) const struct gpio_dt_spec (name) = GPIO_DT_SPEC_GET(DT_NODELABEL(dts), gpios)

#define max(a,b) \
({ __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a > _b ? _a : _b; })

#define min(a,b) \
({ __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b; })

LOG_MODULE_REGISTER(stepper);

K_THREAD_STACK_DEFINE(slider_thread_stack, SLIDER_THREAD_STACK);

// typedef struct slider_config
// {
//     uint32_t start_pos;
//     uint32_t end_pos;
//     uint32_t duration;
//     uint32_t speed;
//     uint32_t intervals;
//     uint32_t interval_delay;
//     uint32_t soft_start;
// } slider_config_t;
//
typedef struct slider_timer 
{
    struct counter_alarm_cfg alarm_cfg;
    const struct device *device;
    bool isEnabled;
} slider_timer_t;

typedef struct slider 
{
    // thread ID
    k_tid_t thread_id;

    // thread internal data
    struct k_thread thread_internal_data;

    // thread application data
    // slider_thread_data_ptr_t thread_data;
 
    // pointer to linear stepper motor structure
    stepperMotor_ptr_t motor;

    // slider config parameters
    slider_config_t *config;

    // slider timer pointer
    slider_timer_t *pTimer;

    // status of the slider
    sliderStatus_t status;
    
    // calculated value of all motor steps in current process
    uint32_t process_steps;

    // remaining steps in current interval
    uint32_t remaining_steps;

    // current delay between steps
    uint32_t c_delay;

    // max delay value between steps (slow motor speed)
    uint32_t max_delay;

    // min delay value between steps (fast motor speed)
    uint32_t min_delay;

} slider_t;

static slider_timer_t *timer_init(const struct device *dev, counter_alarm_callback_t callback, void *user_data);
static int timer_enable(slider_timer_t *pHandle);
static int timer_disable(slider_timer_t *pHandle);
static int timer_set_us_delay(slider_timer_t *pTimer, uint32_t delay_us);
static int timer_run_once(slider_timer_t *pTimer);
static void step_timer_cb(const struct device *counter_dev, uint8_t chan_id, uint32_t ticks, void *user_data);
//
static int count_steps(slider_ptr_t pHandle); 
static int count_duration(slider_ptr_t pHandle); 
static int count_smoothMotion(slider_ptr_t pHandle);
static int select_dir(slider_ptr_t pHandle);

static void slider_thread(void *pCtx, void *p2, void *p3);
static int slider_setIntervalProcess(slider_ptr_t pHandle);
static int slider_process(slider_ptr_t pHandle);
static int slider_calib(slider_ptr_t pHandle);

slider_ptr_t slider_init(void) 
{
    int result = 0;
    slider_ptr_t slider = NULL;

    GET_GPIO(gpio_sm_en,    MOTOR_GPIO_EN_DTS);
    GET_GPIO(gpio_sm_dir,   MOTOR_GPIO_DIR_DTS);
    GET_GPIO(gpio_sm_step,  MOTOR_GPIO_STEP_DTS);
    GET_GPIO(gpio_sm_reset, MOTOR_GPIO_RESET_DTS);

    if((NULL == (slider = malloc(sizeof(slider_t))))
        || (0 == memset(slider, 0, sizeof(slider_t)))) 
    {
        LOG_ERR("slider_init: Unable to allocate memory");
        result = -1;
    }
    else if((NULL == (slider->config = malloc(sizeof(slider_config_t))))
        || (0 == memset(slider->config, 0, sizeof(slider_config_t)))) 
    {
        LOG_ERR("slider_init: Unable to allocate memory");
        result = -1;
    }
    else if (NULL == (slider->motor = stepperMotor_init(gpio_sm_step, gpio_sm_en, gpio_sm_dir, gpio_sm_reset)))
    {
        LOG_ERR("slider_init: Slider Motor init failed");
        result = -2;
    }
    else if (NULL == (slider->pTimer = timer_init(DEVICE_DT_GET(DT_NODELABEL(TIMER)), step_timer_cb, slider)))
    {
        LOG_ERR("slider_init: Slider Timer init failed");
        result = -3;
    }
    else 
    {
        slider->status = sliderStatus_idle;
        //
        slider->config->start_pos      = DEFAULT_START_POS;
        slider->config->end_pos        = DEFAULT_END_POS;
        slider->config->speed          = DEFAULT_SPEED;
        slider->config->intervals      = DEFAULT_INTERVALS;
        slider->config->interval_delay = DEFAULT_INTERVAL_DELAY;
        slider->config->soft_start     = DEFAULT_SOFT_START;

        slider->thread_id = k_thread_create(
            &slider->thread_internal_data, 
            slider_thread_stack, 
            K_THREAD_STACK_SIZEOF(slider_thread_stack),
            slider_thread, 
            slider, 
            NULL, 
            NULL,
            MY_PRIORITY, 
            0, 
            K_NO_WAIT
        );

    }

    if (0 > result)
    {
        LOG_ERR("slider_init: Slider init failed [%d]", result);
        free(slider->pTimer);
        slider->pTimer = NULL;
        free(slider->motor);
        slider->motor = NULL;
        free(slider->config);
        slider->config = NULL;
        free(slider);
        slider = NULL;
    }
    return slider; 
}

int slider_deInit(slider_ptr_t slider) 
{
    int result = -1;
    if(NULL == slider)
    {
        result = -1;
    }
    else 
    {
        stepperMotor_deinit(slider->motor);

        slider->status = sliderStatus_end;
        k_thread_join(&slider->thread_internal_data, K_FOREVER);

        free(slider);
        LOG_INF("slider_deinit: slider deinit successfull");
        result = 0;
    }
    return result; 
}

int slider_stop(slider_ptr_t pHandle) 
{
    int result = -1;
    if(NULL == pHandle)
    {
        result = -1;
    }
    else
    {
        LOG_INF("process stopped");
        pHandle->status = sliderStatus_halted;
        // timer_disable(pHandle->pTimer);
        // stepperMotor_disable(pHandle->motor);
        result = 0;
    }
    return result; 
}

int slider_end(slider_ptr_t pHandle) 
{
    int result = -1;
    if(NULL == pHandle)
    {
        result = -1;
    }
    else
    {
        LOG_INF("process end");
        pHandle->status = sliderStatus_end;
        // timer_disable(pHandle->pTimer);
        // stepperMotor_disable(pHandle->motor);
        result = 0;
    }
    return result; 
}

int slider_start(slider_ptr_t pHandle) 
{
    int result = -1;
    if(NULL == pHandle)
    {
        result = -1;
    }
    else
    {
        LOG_INF("process started");
        pHandle->status = sliderStatus_running;
        result = 0;
    }
    return result; 
}

int slider_setParam(slider_ptr_t pHandle, sliderParam_t param, void * pValue)
{
    int result = -1;
    if ((NULL == pHandle) || (NULL == pValue))
    {
        LOG_ERR("slider setParam: Missing pHandle pointer");
        result = -1;
    }
    else 
    {
        switch(param)
        {
            case sliderParam_startPos:
            {
                pHandle->config->start_pos = *(uint32_t*)pValue;
                result = 0;
            }
            break;
            case sliderParam_endPos:
            {
                pHandle->config->end_pos = *(uint32_t*)pValue;
                result = 0;
            }
            break;
            case sliderParam_duration:
            {
                pHandle->config->duration = *(uint32_t*)pValue;
                result = 0;
            }
            break;
            case sliderParam_speed:
            {
                pHandle->config->speed = *(uint32_t*)pValue;
                result = 0;
            }
            break;
            case sliderParam_intervals:
            {
                pHandle->config->intervals = *(uint32_t*)pValue;
                result = 0;
            }
            break;
            case sliderParam_intervalDelay:
            {
                pHandle->config->interval_delay = *(uint32_t*)pValue;
                result = 0;
            }
            break;
            case sliderParam_softStart:
            {
                pHandle->config->soft_start = *(uint32_t*)pValue;
                result = 0;
            }
            break;
            case sliderParam_none:
            default:
            {
                LOG_ERR("slider setParam: wrong param");
                result = -2;
            }
        }
    }
    return result;
}

int slider_getParam(slider_ptr_t pHandle, sliderParam_t param, void * pValue)
{
    int result = -1;
    if ((NULL == pHandle) || (NULL == pValue))
    {
        LOG_ERR("slider setParam: Missing pHandle pointer");
        result = -1;
    }
    else 
    {
        switch(param)
        {
            case sliderParam_startPos:
            {
                *(uint32_t*)pValue = pHandle->config->start_pos;
                result = 0;
            }
            break;
            case sliderParam_endPos:
            {
                *(uint32_t*)pValue = pHandle->config->end_pos;
                result = 0;
            }
            break;
            case sliderParam_duration:
            {
                *(uint32_t*)pValue = pHandle->config->duration;
                result = 0;
            }
            break;
            case sliderParam_speed:
            {
                *(uint32_t*)pValue = pHandle->config->speed;
                result = 0;
            }
            break;
            case sliderParam_intervals:
            {
                *(uint32_t*)pValue = pHandle->config->intervals;
                result = 0;
            }
            break;
            case sliderParam_intervalDelay:
            {
                *(uint32_t*)pValue = pHandle->config->interval_delay;
                result = 0;
            }
            break;
            case sliderParam_softStart:
            {
                *(uint32_t*)pValue = pHandle->config->soft_start;
                result = 0;
            }
            break;
            case sliderParam_none:
            default:
            {
                LOG_ERR("slider setParam: wrong param");
                result = -2;
            }
        }
    }
    return result;
}

static void slider_thread(void *pCtx, void *p2, void *p3)
{
    slider_ptr_t pHandle = pCtx;
    bool sliderThread_running = true;

    if(NULL == pHandle)
    {
        LOG_ERR("slider thread: Missing pHandle pointer");
        return;
    }
    while (true == sliderThread_running) 
    {
        switch(pHandle->status)
        {
            case sliderStatus_idle:
            {
                k_msleep(100);
            }
            break;
            case sliderStatus_halted:
            {
                slider_end(pHandle);
            }
            break;
            case sliderStatus_error:
            {
                k_msleep(100);
            }
            break;
            case sliderStatus_running:
            {
                slider_process(pHandle);
            }
            break;
            case sliderStatus_calib:
            {
                slider_calib(pHandle);
            }
            break;
            case sliderStatus_end:
            {
                sliderThread_running = false;
            }
            break;
        }
        k_msleep(10);
    }
}

static slider_timer_t *timer_init(const struct device *dev, counter_alarm_callback_t callback, void *user_data)
{
    int result = -1;
    slider_timer_t *timer = NULL;

    if (NULL == dev)
    {
        LOG_ERR("timer_init: null params");
        result = -1;
    }
    else if (NULL == callback)
    {
        LOG_ERR("timer_init: callback is null");
        result = -2;
    }
    else if (NULL == (timer = malloc(sizeof(slider_timer_t))))
    {
        LOG_ERR("timer_init: unable to allocate memory");
        result = -3;
    }
    else 
    {
        if (!device_is_ready(dev)) 
        {
            LOG_ERR("timer_init: timer device is not ready");
            result = -4;
        }
        else 
        {
            timer->isEnabled           = false;
            timer->device              = dev;
            timer->alarm_cfg.flags     = 0;
            timer->alarm_cfg.callback  = callback;
            timer->alarm_cfg.user_data = user_data;
            
            result = 0;
        }

        if (0 > result)
        {
            free(timer);
            timer = NULL;
        }
    }
    return timer;
}

static int timer_enable(slider_timer_t *pHandle) 
{
    int result = -1;
    if((NULL == pHandle) || (NULL == pHandle->device))
    {
        LOG_ERR("timer_enable: null params");
        result = -1;
    }
    else if (0 != (result = counter_start(pHandle->device))) 
    {
        LOG_ERR("timer_enable: timer enable fail %s %d", strerror(errno), errno);
        result = -2;
    }
    else 
    {
        pHandle->isEnabled = true;
        result = 0;
    }
    return result;
}

static int timer_disable(slider_timer_t *pHandle) 
{
    int result = -1;
    if ((NULL == pHandle) || (NULL == pHandle->device))
    {
        LOG_ERR("timer_disable: null params");
        result = -1;
    }
    else if (0 != (result = counter_stop(pHandle->device))) 
    {
        LOG_ERR("timer_disable: timer disable fail");
        result = -2;
    }
    else 
    {
        pHandle->isEnabled = false;
        result = 0;
    }
    return result;
}

static int timer_set_us_delay(slider_timer_t *pTimer, uint32_t delay_us) 
{
    int result = -1;
    if ((NULL == pTimer) || (NULL == pTimer->device))
    {
        LOG_ERR("timer_set_us_delay: null params");
        result = -1;
    }
    else if (0 == delay_us)
    {
        LOG_ERR("timer_set_us_delay: invalid delay");
        result = -2;
    }
    else
    {
        pTimer->alarm_cfg.ticks = counter_us_to_ticks(pTimer->device, delay_us);
        result = 0;
    }
    return result;
}

static int timer_run_once(slider_timer_t *pTimer) 
{
    int result = -1;
    if ((NULL == pTimer) || (NULL == pTimer->device))
    {
        LOG_ERR("timer_run_once: null params");
        result = -1;
    }
    else if (0 != counter_set_channel_alarm(pTimer->device, ALARM_CHANNEL_ID, &pTimer->alarm_cfg))
    {
        LOG_ERR("timer_run_once: counter_set_channel_alarm fail");
        result = -2;
    }
    else
    {
        result = 0;
    }
    return result;
}

static int count_steps(slider_ptr_t pHandle) 
{
    int result = 0;
    if (NULL == pHandle)
    {
        LOG_ERR("count_steps: null params");
        result = -1;
    }
    else
    {
        pHandle->process_steps = abs((int)(pHandle->config->end_pos - pHandle->config->start_pos)) * STEPS_FACTOR;
        result = 0;
    } 
    return result;
}

static int count_duration(slider_ptr_t pHandle) 
{
    int result = 0;
    if (NULL == pHandle)
    {
        result = -1;
    }
    else 
    {
        // TODO
        pHandle->config->duration = 0;
    }
    return result;
}

static int count_smoothMotion(slider_ptr_t pHandle)
{
    int result = -1;
    if (NULL == pHandle)
    {
        LOG_ERR("count_smoothMotion: stepperMotor_step fail");
        result = -1;
    }
    else 
    {
        // slow down stepper motor at the end
        if (pHandle->remaining_steps <= (pHandle->max_delay - pHandle->c_delay)) 
        {
            pHandle->c_delay++;
        } 
        // speed up stepper motor at start
        else if (pHandle->c_delay > pHandle->min_delay) 
        {
            pHandle->c_delay--;
        }
        result = 0;
    }

    return result;
}

static void step_timer_cb(const struct device *counter_dev, uint8_t chan_id, uint32_t ticks, void *user_data) 
{
    int result = 1;

    struct slider *slider  = user_data;
    slider_timer_t *pTimer = slider->pTimer;
    int step_result         = 0;

    if (NULL == slider)
    {
        LOG_ERR("step_timer_cb: null params");
        result = -1;
    }
    else if(0 > (step_result = stepperMotor_step(slider->motor)))
    {
        LOG_ERR("step_timer_cb: stepperMotor_step fail");
        result = -2;
    }
    else if ((0 == step_result) && (0 == slider->remaining_steps))
    {
        // end interval
        timer_disable(slider->pTimer); // TODO adjust
        if (true == DISABLE_MOTOR_AT_INTERVALS)
        {
            stepperMotor_disable(slider->motor);
        }
        result = 0;
    }
    else
    {
        if (1 == step_result)
        {
            // decrement remaining steps
            slider->remaining_steps--;
            LOG_DBG("step_timer_cb: remaining steps: %d", slider->remaining_steps);
        }

        if (0 != count_smoothMotion(slider))
        {
            LOG_ERR("step_timer_cb: count_smoothMotion fail");
            result = -3;
        }
        else if (0 != timer_set_us_delay(pTimer, slider->c_delay))
        {
            LOG_ERR("step_timer_cb: timer_set_us_delay fail");
            result = -4;
        }
        else if (0 != timer_run_once(pTimer))
        {
            LOG_ERR("step_timer_cb: timer_run_once fail");
            result = -5;
        }
        else 
        {
            // continue process
            result = 1;
        }
    }

    // error handle
    if(0 > result)
    {
        // error handle -> disable timer, set status etc...
        // TODO handle stepper motor error
        timer_disable(pTimer); // TODO adjust
        // it should set error status and disable timer (if it is not already disabled??)
        // then interval process should monitor also the error status
        stepperMotor_disable(slider->motor);
        slider->status = sliderStatus_error;
    }
    return;
}

static int slider_setIntervalProcess(slider_ptr_t pHandle) 
{
    int result = -1;

    if (NULL == pHandle)
    {
        LOG_ERR("slider_setIntervalProcess: null handle");
        result = -1;
    }
    else if (sliderStatus_running != pHandle->status)
    {
        LOG_ERR("slider_setIntervalProcess: slider status is not running");
        result = -2;
    }
    else if (0 >= pHandle->config->speed) 
    {
        LOG_ERR("slider_setIntervalProcess: invalid speed value");
        result = -3;
    }
    else if ((true == DISABLE_MOTOR_AT_INTERVALS) && (0 != stepperMotor_enable(pHandle->motor)))
    {
        LOG_ERR("slider_setIntervalProcess: stepperMotor_enable failed");
        result = -4;
    }
    else if (0 != timer_enable(pHandle->pTimer)) // TODO is that necessary there?? 
    {
        LOG_ERR("slider_setIntervalProcess: timer_enable failed");
        result = -5;
    }
    else if (0 != timer_set_us_delay(pHandle->pTimer, pHandle->c_delay))
    {
        LOG_ERR("slider_setIntervalProcess: timer_set_us_delay failed");
        result = -6;
    }
    else if (0 != timer_run_once(pHandle->pTimer))
    {
        LOG_ERR("slider_setIntervalProcess: timer_run_once failed");
        result = -7;
    }
    else 
    {
        result = 0;
    }

    return result;
}

static int slider_process(slider_ptr_t pHandle) 
{
    int result = 1;
    uint32_t steps_per_interval;

    if(NULL == pHandle)
    {
        result = -1;
    }
    else if (0 != select_dir(pHandle))
    {
        result = -2;
    }
    else if (0 != count_steps(pHandle))
    {
        result = -3;
    }
    else if (0 != count_duration(pHandle))
    {
        result = -4;
    }
    else if(0 == pHandle->config->intervals)
    {
        LOG_ERR("slider_process: count of intervals must be higher than 0");
        result = -5;
    }
    else if ((false == DISABLE_MOTOR_AT_INTERVALS) && (0 != stepperMotor_enable(pHandle->motor)))
    {
        LOG_ERR("slider_setIntervalProcess: stepperMotor_enable failed");
        result = -4;
    }
    else 
    {
        LOG_INF("start process");

        steps_per_interval = (pHandle->process_steps / pHandle->config->intervals);
        LOG_INF("steps per interval: %d", steps_per_interval);
        LOG_INF("intervals: %d", pHandle->config->intervals);

        pHandle->min_delay = max(GET_DELAY(pHandle->config->speed), MIN_MOTOR_DELAY);
        pHandle->max_delay = max(GET_DELAY(pHandle->config->soft_start), pHandle->min_delay);

        LOG_INF("min delay: %d", pHandle->min_delay);
        LOG_INF("max delay: %d <- current delay", pHandle->max_delay);

        for (uint32_t current_interval = 1; current_interval <= pHandle->config->intervals; current_interval++) 
        {
            pHandle->remaining_steps = steps_per_interval;
            pHandle->c_delay = pHandle->max_delay;
            LOG_INF("current interval: [%d] steps remaining: [%d] min [%d] max [%d] c_del [%d]", current_interval, pHandle->remaining_steps, pHandle->min_delay, pHandle->max_delay, pHandle->c_delay);
            slider_setIntervalProcess(pHandle);
            while ((sliderStatus_running == pHandle->status) && (true == pHandle->pTimer->isEnabled)) 
            {
                k_msleep(1);
            }
            if(sliderStatus_running != pHandle->status)
            {
                LOG_ERR("slider_process: The process halted during execution"); // TODO check if that is fine
                result = -6;
                break;
            }
            k_msleep(pHandle->config->interval_delay);
        }

        if ((false == DISABLE_MOTOR_AT_INTERVALS) && (0 != stepperMotor_disable(pHandle->motor)))
        {
            LOG_ERR("disable motor fail");
            result = -7;
        }

        if (1 == result)
        {
            pHandle->status = sliderStatus_idle;
            LOG_INF("end process");
            result = 0;
        }
    }

    return result;
}

static int slider_calib(slider_ptr_t pHandle) 
{
    int result = -1;
    // TODO implement function

    // stepper_thread_data data = {
    //     .c_delay = CALIB_STEPS_DELAY,
    //     .remaining_steps = CALIB_MAX_STEPS,
    //     .c_interval = 1,
    //     .max_delay = CALIB_STEPS_DELAY,
    //     .min_delay = CALIB_STEPS_DELAY
    // };
    //
    // alarm_cfg.user_data = &data;
    // uint32_t start_pos;
    // uint32_t end_pos;
    // uint32_t steps_per_cm;
    //
    // uint32_t old_pos;
    // uint32_t curr_pos;
    // uint32_t temp;
    //
    // result = rangefinder_meas();
    // if (result) 
    // {
    //     memcpy(slider.status, SLIDER_STATUS_IDLE, 4);
    //     return result;
    // }
    // start_pos = old_pos = curr_pos = distance_to_cm(&rangefinder_value);
    //
    // set_interval_process(&data);
    // while (timer_status) 
    // {
    //     curr_pos = distance_to_cm(&rangefinder_value);
    //     if (curr_pos > old_pos) 
    //     {
    //         steps_per_cm = (CALIB_MAX_STEPS - data.remaining_steps) / (curr_pos - start_pos);
    //         old_pos = curr_pos;
    //     }
    //
    //     if ((curr_pos - start_pos >= 10) &&
    //         ((CALIB_MAX_STEPS - data.remaining_steps) >
    //         steps_per_cm * (1 + curr_pos - start_pos))) 
    //     {
    //         end_pos = curr_pos;
    //         timer_disable(counter_dev);
    //         STEPPER_MOTOR_DISABLE();
    //     }
    //     //     temp++;
    //     //     if (temp >= 10) 
    //     //     {
    //     LOG_INF("calib process: s_p: %d, c_p: %d, st_per: %d, c_st: %d", start_pos,
    //             curr_pos, steps_per_cm, CALIB_MAX_STEPS - data.remaining_steps);
    //     //       temp = 0;
    //     //     }
    //     k_msleep(1000);
    //     result = rangefinder_meas();
    // }
    // // TODO save calib data to eeprom
    // LOG_INF("calibration done");
    // LOG_INF("start pos = %d", start_pos);
    // LOG_INF("end_pos = %d", end_pos);
    // LOG_INF("steps_per_cm = %d", steps_per_cm);
    //
    // memcpy(slider.status, SLIDER_STATUS_IDLE, 4);

    return result ;
}

static int select_dir(slider_ptr_t pHandle)
{
    int result = -1;
    stepperMotor_dir_t dir;

    if(NULL == pHandle)
    {
        result = -1;
    }
    else
    {
        dir = ((pHandle->config->start_pos < pHandle->config->end_pos) ? stepperMotor_dir_forward : stepperMotor_dir_reverse);
        result = stepperMotor_setDir(pHandle->motor, dir);
    }

    return result;
}

int slider_getStatus(void *handler, void *status)
{
    int result = -1;
    slider_ptr_t pHandle = (slider_ptr_t)handler;
    const char **value = (const char **)status;

    if ((NULL == handler) || (NULL == status)) 
    {
        return -1; // Error: Invalid arguments
    }
    else {

        // assume success
        result = 0;
        switch (pHandle->status) 
        {
            case sliderStatus_idle:
                *value = "idle";
                break;
            case sliderStatus_running:
                *value = "running";
                break;
            case sliderStatus_halted:
                *value = "halted";
                break;
            case sliderStatus_error:
                *value = "error";
                break;
            case sliderStatus_calib:
                *value = "calib";
                break;
            case sliderStatus_end:
                *value = "end";
                break;
            default:
                *value = "unknown";
                result = -2; // Error: Unknown status
        }
    }
    return result; // Success
}

int slider_setStartPos(void *handler, void *value)
{
    LOG_INF("slider_setStartPos hello!");
    int result = -1;
    slider_ptr_t pHandle = (slider_ptr_t)handler;
    int *pValue = (int *)value;
    if(NULL == handler)
    {
        result = -1;
    }
    else
    {
        pHandle->config->start_pos = *pValue;
        result = 0;
    }
    return result;
}

int slider_getStartPos(void *handler, void *value)
{
    int result = -1;
    slider_ptr_t pHandle = (slider_ptr_t)handler;
    int *pValue = (int *)value;
    if(NULL == handler)
    {
        result = -1;
    }
    else
    {
        *pValue = pHandle->config->start_pos;
        result = 0;
    }
    return result;
}

int slider_setEndPos(void *handler, void *value)
{
    int result = -1;
    slider_ptr_t pHandle = (slider_ptr_t)handler;
    int *pValue = (int *)value;
    if(NULL == handler)
    {
        result = -1;
    }
    else
    {
        pHandle->config->end_pos = *pValue;
        result = 0;
    }
    return result;
}

int slider_getEndPos(void *handler, void *value)
{
    int result = -1;
    slider_ptr_t pHandle = (slider_ptr_t)handler;
    int *pValue = (int *)value;
    if(NULL == handler)
    {
        result = -1;
    }
    else
    {
        *pValue = pHandle->config->end_pos;
        result = 0;
    }
    return result;
}

int slider_setDuration(void *handler, void *value)
{
    int result = -1;
    slider_ptr_t pHandle = (slider_ptr_t)handler;
    int *pValue = (int *)value;
    if(NULL == handler)
    {
        result = -1;
    }
    else
    {
        pHandle->config->duration = *pValue;
        result = 0;
    }
    return result;
}

int slider_getDuration(void *handler, void *value)
{
    int result = -1;
    slider_ptr_t pHandle = (slider_ptr_t)handler;
    int *pValue = (int *)value;
    if(NULL == handler)
    {
        result = -1;
    }
    else
    {
        *pValue = pHandle->config->duration;
        result = 0;
    }
    return result;
}

int slider_setSpeed(void *handler, void *value)
{
    int result = -1;
    slider_ptr_t pHandle = (slider_ptr_t)handler;
    int *pValue = (int *)value;
    if(NULL == handler)
    {
        result = -1;
    }
    else
    {
        pHandle->config->speed = *pValue;
        result = 0;
    }
    return result;
}

int slider_getSpeed(void *handler, void *value)
{
    int result = -1;
    slider_ptr_t pHandle = (slider_ptr_t)handler;
    int *pValue = (int *)value;
    if(NULL == handler)
    {
        result = -1;
    }
    else
    {
        *pValue = pHandle->config->speed;
        result = 0;
    }
    return result;
}

int slider_setSoftStart(void *handler, void *value)
{
    int result = -1;
    slider_ptr_t pHandle = (slider_ptr_t)handler;
    int *pValue = (int *)value;
    if(NULL == handler)
    {
        result = -1;
    }
    else
    {
        pHandle->config->soft_start = *pValue;
        result = 0;
    }
    return result;
}

int slider_getSoftStart(void *handler, void *value)
{
    int result = -1;
    slider_ptr_t pHandle = (slider_ptr_t)handler;
    int *pValue = (int *)value;
    if(NULL == handler)
    {
        result = -1;
    }
    else
    {
        *pValue = pHandle->config->soft_start;
        result = 0;
    }
    return result;
}

int slider_setIntervals(void *handler, void *value)
{
    int result = -1;
    slider_ptr_t pHandle = (slider_ptr_t)handler;
    int *pValue = (int *)value;
    if(NULL == handler)
    {
        result = -1;
    }
    else
    {
        pHandle->config->intervals = *pValue;
        result = 0;
    }
    return result;
}

int slider_getIntervals(void *handler, void *value)
{
    int result = -1;
    slider_ptr_t pHandle = (slider_ptr_t)handler;
    int *pValue = (int *)value;
    if(NULL == handler)
    {
        result = -1;
    }
    else
    {
        *pValue = pHandle->config->intervals;
        result = 0;
    }
    return result;
}

int slider_setIntDelay(void *handler, void *value)
{
    int result = -1;
    slider_ptr_t pHandle = (slider_ptr_t)handler;
    int *pValue = (int *)value;
    if(NULL == handler)
    {
        result = -1;
    }
    else
    {
        pHandle->config->interval_delay = *pValue;
        result = 0;
    }
    return result;
}

int slider_getIntDelay(void *handler, void *value)
{
    int result = -1;
    slider_ptr_t pHandle = handler;
    int * pValue = value;
    if(NULL == handler)
    {
        result = -1;
    }
    else
    {
        *pValue = pHandle->config->interval_delay;
        result = 0;
    }
    return result;
}

