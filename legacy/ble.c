#include "ble.h"
#include "layout.h"
#include "rtt_log.h"
#include "sys.h"
#include "timer.h"
#include "usart.h"

static usart_msg ble_usart_msg;
static bool get_ble_name = false;
static bool get_ble_ver = false;
static bool ble_connect = false;
static bool ble_switch = true;
static bool get_ble_switch = false;
static char ble_name[BLE_NAME_LEN + 1] = {0};
static char ble_ver[6] = {0};

static uint8_t calXor(uint8_t *buf, uint32_t len) {
  uint8_t tmp = 0;
  uint32_t i;
  for (i = 0; i < len; i++) {
    tmp ^= buf[i];
  }
  return tmp;
}

static void ble_cmd_packet(uint8_t *value, uint8_t value_len) {
  uint8_t cmd[64] = {0};
  cmd[0] = 0x5a;
  cmd[1] = 0xa5;
  cmd[2] = ((value_len + 1) >> 8) & 0xff;
  cmd[3] = value_len & 0xff;
  memcpy(cmd + 4, value, value_len);
  cmd[value_len + 4] = calXor(cmd, value_len + 4);
  ble_usart_send(cmd, value_len + 5);
}

void ble_request_name(void) {
  uint8_t cmd[64] = {0};
  cmd[0] = BLE_CMD_BT_NAME;
  cmd[1] = BLE_NAME_LEN;
  ble_cmd_packet(cmd, BLE_NAME_LEN + 2);
}

void ble_ctl_onoff(void) {
  uint8_t cmd[64] = {0};
  cmd[0] = BLE_CMD_ONOFF_BLE;
  cmd[1] = 0x01;
  cmd[2] = 0x03;
  ble_cmd_packet(cmd, 0x03);
}

void change_ble_sta(uint8_t mode) {
  uint8_t cmd[64] = {0};
  cmd[0] = BLE_CMD_ONOFF_BLE;
  cmd[1] = 0x01;
  cmd[2] = mode;
  ble_cmd_packet(cmd, 0x03);
}

bool ble_connect_state(void) { return ble_connect; }
bool ble_name_state(void) { return get_ble_name; }
bool ble_ver_state(void) { return get_ble_ver; }
bool ble_switch_state(void) { return get_ble_switch; }
char *ble_get_name(void) { return ble_name; }
char *ble_get_ver(void) { return ble_ver; }

void ble_set_switch(bool flag) { ble_switch = flag; }
bool ble_get_switch(void) { return ble_switch; }

void ble_reset(void) {
  ble_power_off();
  delay_ms(100);
  ble_power_on();
}

void ble_uart_poll(void) {
  static uint8_t read_status = UARTSTATE_IDLE;
  static uint8_t buf[128] = {0};
  uint8_t passkey[7] = {0};
  static uint8_t index = 0;
  volatile uint8_t xor ;
  if (ble_read_byte(buf + index) == false) {
    return;
  }
  index++;
  if (index > sizeof(buf)) {
    index = 0;
    read_status = UARTSTATE_IDLE;
    return;
  }
  if (read_status == UARTSTATE_IDLE) {
    if (index >= 2) {
      if ((buf[0] != 0x5A) && ((buf[1] != 0xA5))) {
        index = 0;
        return;
      }
      read_status = UARTSTATE_READ_LEN;
    }
  } else if (read_status == UARTSTATE_READ_LEN) {
    if (index >= 4) {
      ble_usart_msg.len = (buf[2] << 8) + buf[3];
      if (ble_usart_msg.len == 0) {
        index = 0;
        read_status = UARTSTATE_IDLE;
        return;
      }
      read_status = UARTSTATE_READ_DATA;
    }
  } else if (read_status == UARTSTATE_READ_DATA) {
    if (index == ble_usart_msg.len + 4) {
      ble_usart_msg.xor = buf[ble_usart_msg.len + 3];
      xor = calXor(buf, ble_usart_msg.len + 3);
      if (xor != ble_usart_msg.xor) {
        index = 0;
        read_status = UARTSTATE_IDLE;
        return;
      }
      ble_usart_msg.cmd = buf[4];
      ble_usart_msg.cmd_len = buf[5];
      ble_usart_msg.cmd_vale = buf + 6;
      read_status = UARTSTATE_READ_FINISHED;
    }
  }
  if (read_status == UARTSTATE_READ_FINISHED) {
    index = 0;
    read_status = UARTSTATE_IDLE;

    switch (ble_usart_msg.cmd) {
      case BLE_CMD_CONNECT_STATE:
      case BLE_CMD_PAIR_STATE:
        if (ble_usart_msg.cmd_vale[0] == 0x01)
          ble_connect = true;
        else
          ble_connect = false;
        layoutRefreshSet(true);
        break;
      case BLE_CMD_PASSKEY:
        if (ble_usart_msg.cmd_len == 0x06) {
          memcpy(passkey, ble_usart_msg.cmd_vale, 6);
          layoutBlePasskey(passkey);
        }
        break;
      case BLE_CMD_BT_NAME:
        if (ble_usart_msg.cmd_len == BLE_NAME_LEN) {
          memcpy(ble_name, ble_usart_msg.cmd_vale, BLE_NAME_LEN);
          get_ble_name = true;
          layoutRefreshSet(true);
        }
        break;
      case BLE_CMD_BATTERY:
        if (ble_usart_msg.cmd_vale[0] <= 5)
          battery_cap = ble_usart_msg.cmd_vale[0];
        break;
      case BLE_CMD_VER:
        if (ble_usart_msg.cmd_len == 5) {
          memcpy(ble_ver, ble_usart_msg.cmd_vale, 5);
          get_ble_ver = true;
        }
        break;
      case BLE_CMD_ONOFF_BLE:
        get_ble_switch = true;
        if (ble_usart_msg.cmd_vale[0] == 0) {
          ble_switch = false;
        } else {
          ble_switch = true;
        }
        break;
      case BLE_CMD_DFU_STA:
        if (ble_usart_msg.cmd_vale[0]) {
          layoutDfuStatus(ble_usart_msg.cmd_vale[0]);
        }
        break;
      default:
        break;
    }
  }
}
