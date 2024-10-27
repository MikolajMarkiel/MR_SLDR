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

#define _GNU_SOURCE         /* See feature_test_macros(7) */

#include "slider_bt.h"
#include "slider.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

LOG_MODULE_REGISTER(slider_bt);

#define SLIDER_GATT_PART1 0x88A30000
#define SLIDER_GATT_PART2 0x6CFA
#define SLIDER_GATT_PART3 0x440D
#define SLIDER_GATT_PART4 0x8DDD
#define SLIDER_GATT_PART5 0x4A2D48A4812F
#define SLIDER_SERVICE_MASK 0xFFFF0000

#define SLIDER_UUID(num) \
    BT_UUID_128_ENCODE((SLIDER_GATT_PART1 & SLIDER_SERVICE_MASK) | num, \
                       SLIDER_GATT_PART2, SLIDER_GATT_PART3, SLIDER_GATT_PART4, SLIDER_GATT_PART5)

#define BT_UUID_SLIDER_SERVICE SLIDER_UUID(0x0000)

#define UUID_SUFFIX_SERVICE    (0x0000)
#define UUID_SUFFIX_STATUS     (0x0001)
#define UUID_SUFFIX_CMD        (0x0002)
#define UUID_SUFFIX_START_POS  (0x0010)
#define UUID_SUFFIX_END_POS    (0x0011)
#define UUID_SUFFIX_DURATION   (0x0012)
#define UUID_SUFFIX_SPEED      (0x0013)
#define UUID_SUFFIX_SOFT_START (0x0014)
#define UUID_SUFFIX_INTERVALS  (0x0015)
#define UUID_SUFFIX_DELAY      (0x0016)

#define DEFAULT_WR_PROPS (BT_GATT_CHRC_READ  | BT_GATT_CHRC_WRITE  | BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_AUTH)
#define DEFAULT_WR_PERMS (BT_GATT_PERM_READ  | BT_GATT_PERM_WRITE)
#define DEFAULT_RO_PROPS (BT_GATT_CHRC_READ  | BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_AUTH)
#define DEFAULT_RO_PERMS (BT_GATT_PERM_READ)
#define DEFAULT_WO_PROPS (BT_GATT_CHRC_WRITE | BT_GATT_CHRC_AUTH)
#define DEFAULT_WO_PERMS (BT_GATT_PERM_WRITE)


#define SLIDER_ADD_CHARACTERISTIC(_slider_bt, _type, _props, _perms, _read, _write) \
    BT_GATT_CHARACTERISTIC(&_slider_bt.chrc[_type].uuid.uuid, _props, _perms, _read, _write, &_slider_bt.chrc[_type]), \
    BT_GATT_CCC(ct_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE)
    
#define SLIDER_ADD_RO_STR_CHARACTERISTIC(_slider_bt, _type) \
    SLIDER_ADD_CHARACTERISTIC(_slider_bt, _type, DEFAULT_RO_PROPS, DEFAULT_RO_PERMS, read_str_param, NULL)
/*
#define SLIDER_ADD_WR_STR_CHARACTERISTIC(_slider_bt, _type) \
    SLIDER_ADD_CHARACTERISTIC(_slider_bt, _type, DEFAULT_WR_PROPS, DEFAULT_WR_PERMS, read_str_param, write_str_param)

#define SLIDER_ADD_WO_STR_CHARACTERISTIC(_slider_bt, _type) \
    SLIDER_ADD_CHARACTERISTIC(_slider_bt, _type, DEFAULT_WO_PROPS, DEFAULT_WO_PERMS, NULL, write_str_param)

#define SLIDER_ADD_RO_INT_CHARACTERISTIC(_slider_bt, _type) \
    SLIDER_ADD_CHARACTERISTIC(_slider_bt, _type, DEFAULT_RO_PROPS, DEFAULT_RO_PERMS, read_int_param, NULL)
*/
#define SLIDER_ADD_WR_INT_CHARACTERISTIC(_slider_bt, _type) \
    SLIDER_ADD_CHARACTERISTIC(_slider_bt, _type, DEFAULT_WR_PROPS, DEFAULT_WR_PERMS, read_int_param, write_int_param)
/*
#define SLIDER_ADD_WO_INT_CHARACTERISTIC(_slider_bt, _type) \
    SLIDER_ADD_CHARACTERISTIC(_slider_bt, _type, DEFAULT_WO_PROPS, DEFAULT_WO_PERMS, NULL, write_int_param)
*/

#define SLIDER_CHRC_COUNT 9

enum slider_chrc_type {
    SLIDER_CHRC_STATUS,
    SLIDER_CHRC_CMD,
    SLIDER_CHRC_START_POS,
    SLIDER_CHRC_END_POS,
    SLIDER_CHRC_DURATION,
    SLIDER_CHRC_SPEED,
    SLIDER_CHRC_SOFT_START,
    SLIDER_CHRC_INTERVALS,
    SLIDER_CHRC_INT_DELAY,
};

struct slider_chrc {
    struct bt_uuid_128 uuid;
    int last_int_value;
    char *last_str_value;
	int (*write_cb)(void *handler, void *value);
	int (*read_cb)(void *handler, void *value);
	int (*notify_cb)(struct slider_chrc *handler);
};

typedef struct slider_bt
{
    struct bt_conn_auth_cb auth_cb; 
    struct bt_conn_cb conn_cb;
    struct bt_uuid_128 service;
    size_t param_count;
    uint8_t conn_cnt;
    slider_ptr_t slider;
    struct slider_chrc chrc[SLIDER_CHRC_COUNT];
} slider_bt_t;

static void connected(struct bt_conn *conn, uint8_t err);
static void disconnected(struct bt_conn *conn, uint8_t reason);
static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey);
static void auth_cancel(struct bt_conn *conn);
static void ct_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value);

static int bt_notify_diff_str(struct slider_chrc *handler);
static int bt_notify_diff_int(struct slider_chrc *handler);
static void bt_notify_handler(void);
static ssize_t write_int_param(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags);
static ssize_t read_int_param(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset);
static ssize_t read_str_param(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset);
static ssize_t cmd_handler(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags);

slider_bt_t slider_bt = { 
    .auth_cb = 
    {
        .passkey_display = auth_passkey_display,
        .passkey_entry = NULL,
        .cancel = auth_cancel,
    },
    .conn_cb =
    {
        .connected = connected,
        .disconnected = disconnected,
    },
    .service    = BT_UUID_INIT_128(SLIDER_UUID(UUID_SUFFIX_SERVICE)),
    .conn_cnt = 0,
    .chrc = {
        [SLIDER_CHRC_STATUS]     = { BT_UUID_INIT_128(SLIDER_UUID(UUID_SUFFIX_STATUS)),     -1, NULL, NULL,                slider_getStatus,    bt_notify_diff_str},
        [SLIDER_CHRC_CMD]        = { BT_UUID_INIT_128(SLIDER_UUID(UUID_SUFFIX_CMD)),        -1, NULL, NULL,                NULL,                NULL},
        [SLIDER_CHRC_START_POS]  = { BT_UUID_INIT_128(SLIDER_UUID(UUID_SUFFIX_START_POS)),  -1, NULL, slider_setStartPos,  slider_getStartPos,  bt_notify_diff_int},
        [SLIDER_CHRC_END_POS]    = { BT_UUID_INIT_128(SLIDER_UUID(UUID_SUFFIX_END_POS)),    -1, NULL, slider_setEndPos,    slider_getEndPos,    bt_notify_diff_int},
        [SLIDER_CHRC_DURATION]   = { BT_UUID_INIT_128(SLIDER_UUID(UUID_SUFFIX_DURATION)),   -1, NULL, slider_setDuration,  slider_getDuration,  NULL}, // TODO: fill callback
        [SLIDER_CHRC_SPEED]      = { BT_UUID_INIT_128(SLIDER_UUID(UUID_SUFFIX_SPEED)),      -1, NULL, slider_setSpeed,     slider_getSpeed,     bt_notify_diff_int},
        [SLIDER_CHRC_SOFT_START] = { BT_UUID_INIT_128(SLIDER_UUID(UUID_SUFFIX_SOFT_START)), -1, NULL, slider_setSoftStart, slider_getSoftStart, bt_notify_diff_int},
        [SLIDER_CHRC_INTERVALS]  = { BT_UUID_INIT_128(SLIDER_UUID(UUID_SUFFIX_INTERVALS)),  -1, NULL, slider_setIntervals, slider_getIntervals, bt_notify_diff_int},
        [SLIDER_CHRC_INT_DELAY]  = { BT_UUID_INIT_128(SLIDER_UUID(UUID_SUFFIX_DELAY)),      -1, NULL, slider_setIntDelay,  slider_getIntDelay,  bt_notify_diff_int},
    },
};

static const struct bt_data ad[] = 
{
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_SLIDER_SERVICE),
    BT_DATA_BYTES(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME),
};

static const struct bt_data sd[] = 
{
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_SLIDER_SERVICE),
    BT_DATA_BYTES(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME),
};


BT_GATT_SERVICE_DEFINE
(
    slider_service, BT_GATT_PRIMARY_SERVICE(&slider_bt.service.uuid),
    SLIDER_ADD_RO_STR_CHARACTERISTIC(slider_bt, SLIDER_CHRC_STATUS),
    SLIDER_ADD_WR_INT_CHARACTERISTIC(slider_bt, SLIDER_CHRC_START_POS),
    SLIDER_ADD_WR_INT_CHARACTERISTIC(slider_bt, SLIDER_CHRC_END_POS),
    SLIDER_ADD_WR_INT_CHARACTERISTIC(slider_bt, SLIDER_CHRC_DURATION),
    SLIDER_ADD_WR_INT_CHARACTERISTIC(slider_bt, SLIDER_CHRC_SPEED),
    SLIDER_ADD_WR_INT_CHARACTERISTIC(slider_bt, SLIDER_CHRC_SOFT_START),
    SLIDER_ADD_WR_INT_CHARACTERISTIC(slider_bt, SLIDER_CHRC_INTERVALS),
    SLIDER_ADD_WR_INT_CHARACTERISTIC(slider_bt, SLIDER_CHRC_INT_DELAY),
    SLIDER_ADD_CHARACTERISTIC(slider_bt, SLIDER_CHRC_CMD, DEFAULT_WO_PROPS, DEFAULT_WO_PERMS, NULL, cmd_handler)
);

K_THREAD_DEFINE(bt_notify, 1024, bt_notify_handler, NULL, NULL, NULL, 7, 0, 0);

int slider_bt_init(slider_ptr_t slider_ptr) 
{
    int result            = -1;
    int err               = 0;

    bt_conn_cb_register(&slider_bt.conn_cb);

    if(NULL == slider_ptr)
    {
        LOG_ERR("null slider ptr");
        result = -1;
    }
    else 
    {
        if(0 != (err = bt_enable(NULL)))
        {
            LOG_ERR("Bluetooth init failed (err %d)", err);
            result = -2;
        }
        else if(0 != (err = bt_conn_auth_cb_register(&slider_bt.auth_cb)))
        {
            LOG_ERR("bt_conn_auth_cb_register failed: (err %d)", err);
            result = -3;
        }
        else if(0 != (err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd))))
        {
            LOG_ERR("Advertising failed to start (err %d)", err);
            result = -4;
        }
        else 
        {
            slider_bt.slider = slider_ptr;
            LOG_INF("slider_bt success");
            result = 0;
        }
    }

    if (0 != result)
    {
        bt_le_adv_stop();
        bt_disable();
    }

    return result;
}

static void connected(struct bt_conn *conn, uint8_t err) 
{
    if (err) 
    {
        printk("Connection failed (err %u)\n", err);
        return;
    }
    slider_bt.conn_cnt++;
    LOG_INF("Connected, current connections: %d", slider_bt.conn_cnt);
}

static void disconnected(struct bt_conn *conn, uint8_t reason) 
{
    if (slider_bt.conn_cnt > 0) 
    {
        slider_bt.conn_cnt--;
    }
    LOG_INF("Disconnected (reason %u), current connections: %d", reason, slider_bt.conn_cnt);
}

static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey) 
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    printk("Passkey for %s: %06u\n", addr, passkey);
}

static void auth_cancel(struct bt_conn *conn) 
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    printk("Pairing cancelled: %s\n", addr);
}

static void ct_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value) 
{
    /* TODO: Handle value */
}

static int bt_notify_diff_str(struct slider_chrc *handler) 
{
    int result = -1;
    char *value = NULL;
    struct bt_gatt_attr * attr = NULL;

    if((NULL == handler) || (NULL == handler->read_cb))
    {
        result = -1;
    }
    else if(0 != handler->read_cb(slider_bt.slider, &value))
    {
        result = -2;
    }
    else if((NULL != value) && (NULL != handler->last_str_value) && (0 == strcmp(value, handler->last_str_value)))
    {
        // no change
        result = 0;
    }
    else if (NULL == (attr = bt_gatt_find_by_uuid(slider_service.attrs, slider_service.attr_count, &handler->uuid.uuid)))
    {
        result = -3;
    }
    else if (0 != bt_gatt_notify(NULL, attr, value, strlen(value)))
    {
        result = -5;
    }
    else
    {
        handler->last_str_value = value;
        result = 1;
    }
    // // TODO exchange if server has opportunity to know that client notification is
    // // enabled
    return result;
}

static int bt_notify_diff_int(struct slider_chrc *handler) 
{
    int result = -1;
    int value = 0;
    int err = 0;
    char buf[6];
    struct bt_gatt_attr * attr = NULL;
    if((NULL == handler) || (NULL == handler->read_cb))
    {
        result = -1;
    }
    else if(0 != handler->read_cb(slider_bt.slider, &value))
    {
        result = -2;
    }
    else if(value == handler->last_int_value)
    {
        // no change
        result = 0;
    }
    else if (NULL == (attr = bt_gatt_find_by_uuid(slider_service.attrs, slider_service.attr_count, &handler->uuid.uuid)))
    {
        result = -3;
    }
    else if(0 >= snprintk(buf, sizeof(buf), "%04u", value))
    {
        result = -4;
    }
    else if (0 != (err = bt_gatt_notify(NULL, attr, buf, strlen(buf))))
    {
        result = -5;
    }
    else
    {
        handler->last_int_value = value;
        result = 1;
    }
    // // TODO exchange if server has opportunity to know that client notification is
    // // enabled
    return result;
}

static void bt_notify_handler(void) 
{
    int result = 0;
    while(1)
    {
        for(size_t i = 0; i < SLIDER_CHRC_COUNT; i++)
        {
            if((0 == slider_bt.conn_cnt) || (NULL == slider_bt.chrc[i].notify_cb))
            {
                continue;
            }
            else if(0 > (result = slider_bt.chrc[i].notify_cb(&slider_bt.chrc[i])))
            {
                // LOG_ERR("gatt notify type %zu failed: %d", i, result);
            }
        }
        k_msleep(100);
    }
}

static ssize_t write_int_param(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags) 
{
    int result = -1;
    int value = 0;
    char val_str[12] = {0};

    struct slider_chrc *chrc = (struct slider_chrc *)attr->user_data;
    if ((NULL == chrc) || (NULL == chrc->write_cb)) 
    {
        LOG_ERR("write int value, invalid characteristic or write callback");
        result = -1;
    }
    else if (0 > snprintk(val_str, len+1, "%s", (char *)buf)) 
    {
        LOG_ERR("write int value, buffer too small");
        result = -2; // Error: Buffer too small
    }
    else if(0 > (value = atoi(val_str)))
    {
        LOG_ERR("write int value, atoi failed");
        result = -3; // Error: atoi failed
    }
    else if (0 != (result = chrc->write_cb(slider_bt.slider, &value))) 
    {
        LOG_ERR("write int value, write cb failed");
        result = -4; // Error: write_cb failed
    }
    else 
    {
        LOG_INF("write value, int: %d", value);
        result = len; // Success
    }
    return result; // Success
}

static ssize_t read_int_param(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset) 
{
    struct slider_chrc *chrc = (struct slider_chrc *)attr->user_data; // TODO: remake
    if ((NULL == chrc) || (NULL == chrc->read_cb))
    {
        LOG_INF("read int - error null param");
        return -1; // Error: Invalid characteristic or read callback
    }

    int value;
    int result = chrc->read_cb(slider_bt.slider, &value);
    if (result < 0) 
    {
        LOG_INF("read int - error at cb %d", result);
        return result; // Error: read_cb failed
    }

    char value_str[12] = {0};
    snprintf(value_str, sizeof(value_str), "%d", value);
    LOG_INF("read value, int: %d, str: %s", value, value_str);

    return bt_gatt_attr_read(conn, attr, buf, len, offset, value_str, strlen(value_str));
}

static ssize_t read_str_param(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset) 
{
    struct slider_chrc *chrc = (struct slider_chrc *)attr->user_data; // TODO: remake
    if ((NULL == chrc) || (NULL == chrc->read_cb))
    {
        LOG_INF("read int - error null param");
        return -1; // Error: Invalid characteristic or read callback
    }

    char *value;
    int result = chrc->read_cb(slider_bt.slider, &value);
    if (result < 0) 
    {
        LOG_INF("read int - error at cb %d", result);
        return result; // Error: read_cb failed
    }

    LOG_INF("read value, str: %s", value);

    return bt_gatt_attr_read(conn, attr, buf, len, offset, value, strlen(value));
}

static ssize_t cmd_handler(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags) 
{
    int result = -1;
    char cmd[10] = {0};
    if (len > sizeof(cmd)) 
    {
        LOG_ERR("too long cmd input! resize buffer");
        result = -1;
    }
    else if (NULL == strcpy(cmd, buf)) 
    {
        result = -2;
    }
    else {
        // assume success
        result = len;
        LOG_INF("cmd handler cmd: \"%s\"", cmd);
        if(0 == strcmp(cmd, "start"))
        {
            slider_start(slider_bt.slider);
        }
        else if(0 == strcmp(cmd, "stop"))
        {
            slider_stop(slider_bt.slider);
        }
        else if(0 == strcmp(cmd, "calib"))
        {
            // slider_calib(slider_bt.slider);
        }
        else
        {
            LOG_ERR("wrong command \"%s\"", cmd);
            result = -3;
        }
    }
    return result;
}

