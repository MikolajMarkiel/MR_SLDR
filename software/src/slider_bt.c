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
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(slider_bt);

#define DEFAULT_WR_PROPS                                                       \
  BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY |               \
      BT_GATT_CHRC_AUTH

#define DEFAULT_WR_PERMS BT_GATT_PERM_READ | BT_GATT_PERM_WRITE

#define DEFAULT_RO_PROPS                                                       \
  BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_AUTH

#define DEFAULT_RO_PERMS BT_GATT_PERM_READ

#define DEFAULT_WO_PROPS BT_GATT_CHRC_WRITE | BT_GATT_CHRC_AUTH

#define DEFAULT_WO_PERMS BT_GATT_PERM_WRITE

#define SLIDER_GATT_1 0x88A30000
#define SLIDER_GATT_2 0x6CFA
#define SLIDER_GATT_3 0x440D
#define SLIDER_GATT_4 0x8DDD
#define SLIDER_GATT_5 0x4A2D48A4812F
#define SLIDER_SERVICE_MASK 0xFFFF0000

#define SLIDER_UUID(num)                                                       \
  BT_UUID_128_ENCODE((SLIDER_GATT_1 & SLIDER_SERVICE_MASK) | num,              \
                     SLIDER_GATT_2, SLIDER_GATT_3, SLIDER_GATT_4,              \
                     SLIDER_GATT_5)

#define BT_UUID_SLIDER_SERVICE SLIDER_UUID(0x0000)

static uint8_t bt_conn_cnt = 0;

static struct bt_uuid_128 slider_service_uuid =
    BT_UUID_INIT_128(SLIDER_UUID(0x0000));
static struct bt_uuid_128 slider_status_uuid =
    BT_UUID_INIT_128(SLIDER_UUID(0x0001));
static struct bt_uuid_128 slider_cmd_uuid =
    BT_UUID_INIT_128(SLIDER_UUID(0x0002));
static struct bt_uuid_128 slider_start_pos_uuid =
    BT_UUID_INIT_128(SLIDER_UUID(0x0010));
static struct bt_uuid_128 slider_end_pos_uuid =
    BT_UUID_INIT_128(SLIDER_UUID(0x0011));
static struct bt_uuid_128 slider_duration_uuid =
    BT_UUID_INIT_128(SLIDER_UUID(0x0012));
static struct bt_uuid_128 slider_speed_uuid =
    BT_UUID_INIT_128(SLIDER_UUID(0x0013));
static struct bt_uuid_128 slider_soft_start_uuid =
    BT_UUID_INIT_128(SLIDER_UUID(0x0014));
static struct bt_uuid_128 slider_intervals_uuid =
    BT_UUID_INIT_128(SLIDER_UUID(0x0015));
static struct bt_uuid_128 slider_int_delay_uuid =
    BT_UUID_INIT_128(SLIDER_UUID(0x0016));

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_SLIDER_SERVICE),
    BT_DATA_BYTES(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME),
};

static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_SLIDER_SERVICE),
    BT_DATA_BYTES(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME),
};

int buf_to_int(const void *buf, uint16_t len, uint32_t *val) {
  const char *buf_ptr = buf;
  uint32_t temp_val;
  char temp_str_1[11];
  char temp_str_2[11];
  memcpy(temp_str_1, buf, len);
  temp_str_1[len] = 0;
  temp_val = atoi(temp_str_1);
  sprintf(temp_str_2, "%d", temp_val);
  if (strcmp(temp_str_1, temp_str_2)) {
    LOG_ERR("wrong int value, val_1: %s, val_2: %s", temp_str_1, temp_str_2);
    return -1;
  }
  memcpy(val, &temp_val, sizeof(*val));
  return 0;
}

static ssize_t write_int_param(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr, const void *buf,
                               uint16_t len, uint16_t offset, uint8_t flags) {
  uint32_t *param = attr->user_data;
  if (buf_to_int(buf, len, param)) {
    return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
  }
  LOG_INF("write value, int: %d", *param);
  return len;
}

static ssize_t read_int_param(struct bt_conn *conn,
                              const struct bt_gatt_attr *attr, void *buf,
                              uint16_t len, uint16_t offset) {
  uint32_t *param = attr->user_data;
  char value[sizeof(uint32_t)];
  sprintf(value, "%d", *param);
  LOG_INF("read value, int: %d, str: %s", *param, value);
  return bt_gatt_attr_read(conn, attr, buf, len, offset, value, strlen(value));
}

static ssize_t cmd_handler(struct bt_conn *conn,
                           const struct bt_gatt_attr *attr, const void *buf,
                           uint16_t len, uint16_t offset, uint8_t flags) {
  static char cmd[10];
  if (len > sizeof(cmd)) {
    LOG_ERR("too long cmd input! resize buffer");
    memcpy(slider.status, SLIDER_STATUS_ERROR, 4);
    return len;
  }
  memcpy(cmd, buf, len);

  LOG_INF("cmd handler cmd: \"%s\"", cmd);
  if (!memcmp(cmd, "start", len)) {
    memcpy(slider.status, SLIDER_STATUS_RUNNING, 4);
  } else if (!memcmp(cmd, "stop", len)) {
    slider_stop();
    // slider_calib();

  } else {
    LOG_ERR("wrong command \"%s\"", cmd);
  }
  memset(cmd, 0, sizeof(cmd));
  return len;
}

static void ct_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value) {
  /* TODO: Handle value */
}

BT_GATT_SERVICE_DEFINE(
    slider_service, BT_GATT_PRIMARY_SERVICE(&slider_service_uuid.uuid),
    BT_GATT_CHARACTERISTIC(&slider_status_uuid.uuid, DEFAULT_RO_PROPS,
                           DEFAULT_RO_PERMS, read_int_param, write_int_param,
                           slider.status),
    BT_GATT_CCC(ct_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC(&slider_start_pos_uuid.uuid, DEFAULT_WR_PROPS,
                           DEFAULT_WR_PERMS, read_int_param, write_int_param,
                           &slider.start_pos),
    BT_GATT_CCC(ct_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC(&slider_end_pos_uuid.uuid, DEFAULT_WR_PROPS,
                           DEFAULT_WR_PERMS, read_int_param, write_int_param,
                           &slider.end_pos),
    BT_GATT_CCC(ct_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC(&slider_duration_uuid.uuid, DEFAULT_WR_PROPS,
                           DEFAULT_WR_PERMS, read_int_param, write_int_param,
                           &slider.duration),
    BT_GATT_CCC(ct_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC(&slider_speed_uuid.uuid, DEFAULT_WR_PROPS,
                           DEFAULT_WR_PERMS, read_int_param, write_int_param,
                           &slider.speed),
    BT_GATT_CCC(ct_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC(&slider_soft_start_uuid.uuid, DEFAULT_WR_PROPS,
                           DEFAULT_WR_PERMS, read_int_param, write_int_param,
                           &slider.soft_start),
    BT_GATT_CCC(ct_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC(&slider_intervals_uuid.uuid, DEFAULT_WR_PROPS,
                           DEFAULT_WR_PERMS, read_int_param, write_int_param,
                           &slider.interval_steps),
    BT_GATT_CCC(ct_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC(&slider_int_delay_uuid.uuid, DEFAULT_WR_PROPS,
                           DEFAULT_WR_PERMS, read_int_param, write_int_param,
                           &slider.interval_delay),
    BT_GATT_CCC(ct_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC(&slider_cmd_uuid.uuid, DEFAULT_WO_PROPS,
                           DEFAULT_WO_PERMS, NULL, cmd_handler, NULL), );

#define NOTIFY_SLIDER_STATUS 2
#define NOTIFY_SLIDER_START_POS NOTIFY_SLIDER_STATUS + 3
#define NOTIFY_SLIDER_END_POS NOTIFY_SLIDER_START_POS + 3
#define NOTIFY_SLIDER_DURATION NOTIFY_SLIDER_END_POS + 3
#define NOTIFY_SLIDER_SPEED NOTIFY_SLIDER_DURATION + 3
#define NOTIFY_SLIDER_SOFT_START NOTIFY_SLIDER_SPEED + 3
#define NOTIFY_SLIDER_INTERVALS NOTIFY_SLIDER_SOFT_START + 3
#define NOTIFY_SLIDER_INTERVAL_DELAY NOTIFY_SLIDER_INTERVALS + 3

static void bt_notify_diff_str(const struct bt_gatt_attr *chrc,
                               const void *current_msg, void *old_msg,
                               size_t n) {
  static int err;
  if (!memcmp(old_msg, current_msg, n)) {
    return;
  }
  memcpy(old_msg, current_msg, n);
  // err = bt_gatt_notify(NULL, chrc, old_msg, n);
  err = bt_gatt_notify(NULL, chrc, old_msg, sizeof(old_msg));
  err = err == -ENOTCONN ? 0 : err;
  // TODO exchange if server has opportunity to know that client notification is
  // enabled
  if (err) {
    LOG_ERR("gatt notify failed, reason: %d", err);
    return;
  }
  LOG_INF("notify: %s", (char*)old_msg);
}

static void bt_notify_diff_int(const struct bt_gatt_attr *chrc,
                               const void *current_msg, void *old_msg,
                               size_t n) {
  static int err;
  static char buf[11];
  static uint32_t *ptr;
  if (!memcmp(old_msg, current_msg, n)) {
    return;
  }
  memcpy(old_msg, current_msg, n);
  ptr = (uint32_t *)current_msg;
  sprintf(buf, "%04u", *ptr);
  err = bt_gatt_notify(NULL, chrc, buf, sizeof(old_msg));
  err = err == -ENOTCONN ? 0 : err;
  // TODO exchange if server has opportunity to know that client notification is
  // enabled
  if (err) {
    LOG_ERR("gatt notify failed, reason: %d", err);
    return;
  }
  LOG_INF("notify: %u", *ptr);
}

void bt_notify_handler() {
  static slider_params old_slider;
  old_slider = slider;
  static uint8_t old_bt_conn_cnt = 0;
  while (1) {
    // if (bt_conn_cnt > old_bt_conn_cnt) { // force to notify all
    //   memset(&old_slider, 0xFF, sizeof(old_slider));
    //   LOG_INF("reset old_slider");
    // }
    old_bt_conn_cnt = bt_conn_cnt;
    bt_notify_diff_str(&slider_service.attrs[NOTIFY_SLIDER_STATUS],
                       &slider.status, &old_slider.status,
                       sizeof(slider.status));
    bt_notify_diff_int(&slider_service.attrs[NOTIFY_SLIDER_START_POS],
                       &slider.start_pos, &old_slider.start_pos,
                       sizeof(slider.start_pos));
    bt_notify_diff_int(&slider_service.attrs[NOTIFY_SLIDER_END_POS],
                       &slider.end_pos, &old_slider.end_pos,
                       sizeof(slider.end_pos));
    bt_notify_diff_int(&slider_service.attrs[NOTIFY_SLIDER_DURATION],
                       &slider.duration, &old_slider.duration,
                       sizeof(slider.duration));
    bt_notify_diff_int(&slider_service.attrs[NOTIFY_SLIDER_SPEED],
                       &slider.speed, &old_slider.speed, sizeof(slider.speed));
    bt_notify_diff_int(&slider_service.attrs[NOTIFY_SLIDER_SOFT_START],
                       &slider.soft_start, &old_slider.soft_start,
                       sizeof(slider.soft_start));
    bt_notify_diff_int(&slider_service.attrs[NOTIFY_SLIDER_INTERVALS],
                       &slider.interval_steps, &old_slider.interval_steps,
                       sizeof(slider.interval_steps));
    bt_notify_diff_int(&slider_service.attrs[NOTIFY_SLIDER_INTERVAL_DELAY],
                       &slider.interval_delay, &old_slider.interval_delay,
                       sizeof(slider.interval_delay));
    k_msleep(100);
  }
}

static void connected(struct bt_conn *conn, uint8_t err) {
  if (err) {
    printk("Connection failed (err %u)\n", err);
    return;
  }
  bt_conn_cnt++;
  LOG_INF("Connected, current connections: %d", bt_conn_cnt);
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
  if (bt_conn_cnt > 0) {
    bt_conn_cnt--;
  }
  LOG_INF("Disconnected (reason %u), current connections: %d", reason,
          bt_conn_cnt);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey) {
  char addr[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

  printk("Passkey for %s: %06u\n", addr, passkey);
}

static void auth_cancel(struct bt_conn *conn) {
  char addr[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

  printk("Pairing cancelled: %s\n", addr);
}

static struct bt_conn_auth_cb auth_cb_display = {
    .passkey_display = auth_passkey_display,
    .passkey_entry = NULL,
    .cancel = auth_cancel,
};

int slider_bt_init() {
  int err;
  err = bt_enable(NULL);
  if (err) {
    LOG_ERR("Bluetooth init failed (err %d)", err);
    return 1;
  }

  err = bt_conn_auth_cb_register(&auth_cb_display);
  if (err) {
    LOG_ERR("bt_conn_auth_cb_register failed: (err %d)", err);
    return 2;
  }

  err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
  if (err) {
    LOG_ERR("Advertising failed to start (err %d)", err);
    return 3;
  }
  return 0;
}
