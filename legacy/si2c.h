#ifndef _si2c_H_
#define _si2c_H_

#include <stdint.h>
#include "trans_fifo.h"

#define _SUPPORT_SOFTI2C_ 0
#define SI2C_BUF_MAX_LEN (1024 * 15)

typedef enum _ChannelType {
  CHANNEL_NULL,
  CHANNEL_USB,
  CHANNEL_SLAVE,
} ChannelType;

extern ChannelType host_channel;

// I2C gpio
#define GPIO_SI2C_PORT GPIOB
#define GPIO_SI2C_SCL GPIO10
#define GPIO_SI2C_SDA GPIO11

#define SI2C_ADDR 0x48  // 90

extern uint8_t i2c_data_in[SI2C_BUF_MAX_LEN];
extern volatile uint32_t i2c_data_inlen;
extern volatile bool i2c_recv_done;
extern uint8_t i2c_data_out[SI2C_BUF_MAX_LEN];
extern volatile uint32_t i2c_data_outlen, i2c_data_out_pos;

extern trans_fifo i2c_fifo_in, i2c_fifo_out;

void i2c_slave_init_irq(void);
void i2c_slave_init(void);

void i2c_slave_send(uint32_t data_len);
void i2c_slave_send_ex(uint32_t data_len);

#endif
