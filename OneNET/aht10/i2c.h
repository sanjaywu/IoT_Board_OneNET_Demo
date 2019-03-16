#ifndef __I2C_H__
#define __I2C_H__

#include "data_typedef.h"

void i2c_init(void);
void i2c_start(void);
void i2c_stop(void);
u8 i2c_wait_ack(u16 time_out);
void i2c_ack(void);
void i2c_nack(void);
void i2c_send_byte(u8 byte);
u8 i2c_read_byte(void);
u8 i2c_write_one_byte(u8 slave_addr, u8 reg_addr, u8 *byte);
u8 i2c_read_one_byte(u8 slave_addr, u8 reg_addr, u8 *val);
u8 i2c_write_bytes(u8 slave_addr, u8 reg_addr, u8 *buf, u8 num);
u8 i2c_read_bytes(u8 slave_addr, u8 reg_addr, u8 *buf, u8 num);




#endif








