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
#include <stdlib.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

#include "distanceMeter.h"
#include "zephyr/logging/log.h"

#define VL53L0X_DEVICE st_vl53l0x
#define VL53L0X_SAMPLES 10

// TODO: The 250 samples took around 10 seconds to complete, so the delay for every sample is around 40ms
// need to send it to interrupt or even another thread to not block the main thread
// reduce samples to 10 or 20

LOG_MODULE_REGISTER(distanceMeter);

typedef struct distanceMeter
{
    const struct device *vl53l0x_dev;
    // struct sensor_value value;
    int val_mm;
} distanceMeter_t;

// const struct device *const vl53l0x_dev = DEVICE_DT_GET_ONE(st_vl53l0x);
// struct sensor_value distanceMeter_value;


int distanceMeter_init(distanceMeter_ptr_t *pHandle) 
{
    int result = 0;
    distanceMeter_t *distanceMeter = NULL;
    if(NULL == pHandle)
    {
        result = -1;
    }
    else if (NULL == (*pHandle = malloc(sizeof(distanceMeter_t))))
    {
        result = -2;
    }
    else 
    {
        (*pHandle)->vl53l0x_dev = DEVICE_DT_GET_ONE(VL53L0X_DEVICE);
        if(0 == device_is_ready((*pHandle)->vl53l0x_dev))
        {
            result = -3;
        }
        else
        {
            result = 0;
        }
    }

    if (0 != result)
    {
        free(*pHandle);
        *pHandle = NULL;
    }
    return result;
}

int distanceMeter_deinit(distanceMeter_ptr_t *pHandle) 
{
    return 0;
}

int distanceMeter_meas(distanceMeter_ptr_t pHandle) {

    int result = -1;
    int err;
    int value = 0;
    struct sensor_value average = {0}, temp = {0};

    if(NULL == pHandle)
    {
        result = -1;
    }
    else if(0 != sensor_sample_fetch_chan(pHandle->vl53l0x_dev, SENSOR_CHAN_DISTANCE))
    {
        result = -1;
    }
    else if(0 != sensor_channel_get(pHandle->vl53l0x_dev, SENSOR_CHAN_DISTANCE, &temp))
    {
        result = -2;
    }
    // else
    // {
    //     result = 0;
    // }

    else 
    {
        // assume good result
        result = 0;

        for (int i = 0; i < VL53L0X_SAMPLES; i++) 
        {
            if(0 != sensor_sample_fetch_chan(pHandle->vl53l0x_dev, SENSOR_CHAN_DISTANCE))
            {
                result = -1;
            }
            else if(0 != sensor_channel_get(pHandle->vl53l0x_dev, SENSOR_CHAN_DISTANCE, &temp))
            {
                result = -2;
            }
            // else 
            // {
            //     // count running average
            //     average.val1 += (temp.val1 - average.val1) / (i + 1);
            //     average.val2 += (temp.val2 - average.val2) / (i + 1);
            // }
            else
            {
                average.val1 += temp.val1;
                average.val2 += temp.val2;
            }
            // value = sensor.val1 * 1000;
            // value += (sensor.val2 / 1000);
        }
        average.val1 /= VL53L0X_SAMPLES;
        average.val2 /= VL53L0X_SAMPLES;

        value = ((average.val1 * 1000) + (average.val2 / 1000)); // value in mm
    }

    if (0 == result)
    {
        pHandle->val_mm = value;
        LOG_INF("val_mm = %d", value);
    }
    else 
    {
        LOG_ERR("distanceMeter_meas failed [%d]", result);
    }

    return result;
    }

int distance_to_cm(struct sensor_value *val) {
  return val->val1 * 100 + val->val2 / 10000;
}
