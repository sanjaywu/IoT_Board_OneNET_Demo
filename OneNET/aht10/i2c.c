#include "i2c.h"
#include "rtthread.h"
#include "board.h"


/*
SDA -->  PC1
SCL -->  PD6
*/
#define I2C_SDA_H	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_SET)
#define I2C_SDA_L	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_RESET)
#define I2C_SDA_R	HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_1)
#define I2C_SCL_H	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_6, GPIO_PIN_SET)
#define I2C_SCL_L	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_6, GPIO_PIN_RESET)

void I2C_SDA_IN(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;
  	/*Configure GPIO pin : PC1 */	
  	GPIO_InitStruct.Pin = GPIO_PIN_1;
  	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  	GPIO_InitStruct.Pull = GPIO_PULLUP;
  	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

void I2C_SDA_OUT(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;
	/*Configure GPIO pin : PC1 */
  	GPIO_InitStruct.Pin = GPIO_PIN_1;
  	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  	GPIO_InitStruct.Pull = GPIO_PULLUP;
  	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

/**************************************************************
函数名称 : i2c_delay
函数功能 : i2c 延时来模拟时序
输入参数 : 无
返回值  	 : 无
备注		 : 无
**************************************************************/
void i2c_delay(void)
{
	rt_hw_us_delay(2);
}

/**************************************************************
函数名称 : i2c_init
函数功能 : 软件 i2c 初始化
输入参数 : 无
返回值  	 : 无
备注		 : 由于潘多拉开发板上AHT10没外接上拉电阻，因此这里需要
		   配置为推挽上拉输出
**************************************************************/
void i2c_init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;

  	/* GPIO Ports Clock Enable */
 	__HAL_RCC_GPIOC_CLK_ENABLE();
  	__HAL_RCC_GPIOD_CLK_ENABLE();

  	/*Configure GPIO pin : PC1 */
  	GPIO_InitStruct.Pin = GPIO_PIN_1;
  	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  	GPIO_InitStruct.Pull = GPIO_PULLUP;
  	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  	/*Configure GPIO pin : PD6 */
  	GPIO_InitStruct.Pin = GPIO_PIN_6;
  	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  	GPIO_InitStruct.Pull = GPIO_PULLUP;
  	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

	I2C_SCL_H;
	I2C_SDA_H;
}

/**************************************************************
函数名称 : i2c_start
函数功能 : 软件 i2c 起始信号
输入参数 : 无
返回值  	 : 无
备注		 : 当SCL线为高时，SDA线一个下降沿代表起始信号
**************************************************************/
void i2c_start(void)
{
	I2C_SDA_OUT();	/* SDA输出 */
	I2C_SDA_H;		/* 拉高SDA线 */
	I2C_SCL_H;		/* 拉高SCL线 */
	i2c_delay();	/* 延时，速度控制 */
	
	I2C_SDA_L;		/* 当SCL线为高时，SDA线一个下降沿代表起始信号 */
	i2c_delay();	/* 延时，速度控制 */
	I2C_SCL_L;		/* 钳住SCL线，以便发送数据 */
}

/**************************************************************
函数名称 : i2c_stop
函数功能 : 软件 i2c 停止信号
输入参数 : 无
返回值  	 : 无
备注		 : 当SCL线为高时，SDA线一个上升沿代表停止信号
**************************************************************/
void i2c_stop(void)
{
	I2C_SDA_OUT();	/* SDA输出 */
	I2C_SDA_L;		/* 拉低SDA线 */
	I2C_SCL_L;		/* 拉低SCL线 */
	i2c_delay();	/* 延时，速度控制 */
	
	I2C_SCL_H;		/* 拉高SCL线 */
	I2C_SDA_H;		/* 拉高SDA线，当SCL线为高时，SDA线一个上升沿代表停止信号 */
	i2c_delay();	/* 延时，速度控制 */
}

/**************************************************************
函数名称 : i2c_wait_ack
函数功能 : 软件 i2c 等待应答
输入参数 : time_out：等待时间，单位us
返回值  	 : 0：应答成功
		   1：应答失败
备注		 : 无
**************************************************************/
u8 i2c_wait_ack(u16 time_out)
{
	I2C_SDA_IN();	/* SDA输入 */
	I2C_SDA_H;		/* 拉高SDA线 */
	i2c_delay();
	I2C_SCL_H;
	i2c_delay();	/* 拉高SCL线 */
	
	while(I2C_SDA_R)/* 如果读到SDA线为1，则等待。应答信号应是0 */
	{
		time_out--;
		
		if(time_out > 0)
		{
			rt_hw_us_delay(1);
		}
		else
		{
			i2c_stop();	/* 超时未收到应答，则停止总线 */
			return 1;	/* 返回失败 */
		}
	}
	I2C_SCL_L;		/* 拉低SCL线，以便继续收发数据 */
	
	return 0;		/* 返回成功 */
}

/**************************************************************
函数名称 : i2c_ack
函数功能 : 软件 i2c 产生ACK应答
输入参数 : 无
返回值  	 : 无
备注		 : 当SDA线为低时，SCL线一个正脉冲代表发送一个应答信号
**************************************************************/
void i2c_ack(void)
{
	I2C_SCL_L;		/* 拉低SCL线 */
	I2C_SDA_OUT();	/* SDA输出 */
	I2C_SDA_L;		/* 拉低SDA线 */
	i2c_delay();
	I2C_SCL_H;		/* 拉高SCL线 */
	i2c_delay();
	I2C_SCL_L;		/* 拉低SCL线 */
}


/**************************************************************
函数名称 : i2c_nack
函数功能 : 软件 i2c 产生NACK非应答
输入参数 : 无
返回值  	 : 无
备注		 : 当SDA线为高时，SCL线一个正脉冲代表发送一个非应答信号
**************************************************************/
void i2c_nack(void)
{
	I2C_SCL_L;		/* 拉低SCL线 */
	I2C_SDA_OUT();	/* SDA输出 */
	I2C_SDA_H;		/* 拉高SDA线 */
	i2c_delay();
	I2C_SCL_H;		/* 拉高SCL线 */
	i2c_delay();
	I2C_SCL_L;		/* 拉低SCL线 */
}

/**************************************************************
函数名称 : i2c_send_byte
函数功能 : 软件 i2c 发送一个字节
输入参数 : byte：需要发送的字节
返回值  	 : 无
备注		 : 无
**************************************************************/
void i2c_send_byte(u8 byte)
{
	u8 count = 0;

	I2C_SDA_OUT();
    I2C_SCL_L;	/* 拉低时钟开始数据传输 */
	
	/* 循环8次，每次发送一个bit */
    for(count = 0; count < 8; count++)	
    {
		if(byte & 0x80)	/* 发送最高位 */
		{
			I2C_SDA_H;
		}
		else
		{
			I2C_SDA_L;
		}
		byte <<= 1;	/* byte左移1位 */
		
		i2c_delay();
		I2C_SCL_H;
		i2c_delay();
		I2C_SCL_L;
    }
}

/**************************************************************
函数名称 : i2c_read_byte
函数功能 : 软件 i2c 读取一个字节
输入参数 : 无
返回值  	 : 读取到的字节数据
备注		 : 无
**************************************************************/
u8 i2c_read_byte(void)
{
	u8 count = 0; 
	u8 receive = 0;

	I2C_SDA_IN();	/* SDA输入 */
	//I2C_SDA_H;	/* 拉高SDA线，读取数据 */

	/* 循环8次，每次读一个bit */
    for(count = 0; count < 8; count++ )	
	{
		I2C_SCL_L;
		i2c_delay();
		I2C_SCL_H;
		
        receive <<= 1;	/* 左移一位 */
		
        if(I2C_SDA_R)	/* 如果SDA线为1，则receive变量自增，每次自增都是对bit0的+1，然后下一次循环会先左移一次 */
        {
			receive++;
        }
		i2c_delay();
    }

    return receive;
}


/**************************************************************
函数名称 : i2c_write_one_byte
函数功能 : 软件 i2c 写一个数据
输入参数 : slave_addr：从机地址
		   reg_addr：寄存器地址
		   byte：需要写入的数据
返回值  	 : 0：写入成功
		   1：写入失败
备注		 : *byte是缓存写入数据的变量的地址，
			因为有些寄存器只需要控制下寄存器，并不需要写入值
**************************************************************/
u8 i2c_write_one_byte(u8 slave_addr, u8 reg_addr, u8 *byte)
{
	i2c_start();	/* 起始信号 */
	i2c_send_byte((slave_addr << 1) | 0x00);/* 发送设备地址(写) */
	if(i2c_wait_ack(5000))		/* 等待应答 */
	{
		rt_kprintf("i2c_write_one_byte,send dev_addr ack failed\r\n");
		i2c_stop();
		return 1;
	}
	
	i2c_send_byte(reg_addr);/* 发送寄存器地址 */
	if(i2c_wait_ack(5000))	/* 等待应答 */
	{
		rt_kprintf("i2c_write_one_byte,send reg_addr ack failed\r\n");
		i2c_stop();
		return 1;
	}
	
	if(byte)
	{
		i2c_send_byte(*byte);	/* 发送数据 */
		if(i2c_wait_ack(5000))	/* 等待应答 */
		{
			rt_kprintf("i2c_write_one_byte,send data ack failed\r\n");
			i2c_stop();
			return 1;
		}
	}
	
	i2c_stop();
	
	return 0;
}

/**************************************************************
函数名称 : i2c_read_one_byte
函数功能 : 软件 i2c 读一个数据
输入参数 : slave_addr：从机地址
		   reg_addr：寄存器地址
		   val：需要读取的数据缓存
返回值  	 : 0：成功
		   1：失败
备注		 : val是一个缓存变量的地址
**************************************************************/
u8 i2c_read_one_byte(u8 slave_addr, u8 reg_addr, u8 *val)
{
	i2c_start();/* 起始信号 */
	
	i2c_send_byte((slave_addr << 1) | 0x00);/* 发送设备地址(写) */
	if(i2c_wait_ack(5000))					/* 等待应答 */
	{
		rt_kprintf("i2c_read_one_byte,send dev_addr(write) ack failed\r\n");
		i2c_stop();
		return 1;
	}
	
	i2c_send_byte(reg_addr);/* 发送寄存器地址 */
	if(i2c_wait_ack(5000))	/* 等待应答 */
	{
		rt_kprintf("i2c_read_one_byte,send reg_addr ack failed\r\n");
		i2c_stop();
		return 1;
	}
	i2c_stop();/* 停止信号，一定要向停止信号 */
	
	i2c_start();/* 重启信号 */
	
	i2c_send_byte((slave_addr << 1) | 0X01);/* 发送设备地址(读) */
	if(i2c_wait_ack(5000))	/* 等待应答 */
	{
		rt_kprintf("i2c_read_one_byte,send dev_addr(read) ack failed\r\n");
		i2c_stop();
		return 1;
	}
	
	*val = i2c_read_byte();	/* 接收数据 */
	i2c_nack();/* 产生一个非应答信号，代表读取接收 */

	i2c_stop();	/* 停止信号 */
	
	return 0;
}

/**************************************************************
函数名称 : i2c_write_bytes
函数功能 : 软件 i2c 写多个数据
输入参数 : slave_addr：从机地址
		   reg_addr：寄存器地址
		   buf：需要写入的数据缓存
		   num：数据长度
返回值  	 : 0：成功
		   1：失败
备注		 : *buf是一个数组或指向一个缓存区的指针
**************************************************************/
u8 i2c_write_bytes(u8 slave_addr, u8 reg_addr, u8 *buf, u8 num)
{
	i2c_start();/* 起始信号 */
	
	i2c_send_byte((slave_addr << 1) | 0x00);/* 发送设备地址(写) */
	if(i2c_wait_ack(5000))	/* 等待应答 */
	{
		rt_kprintf("i2c_write_bytes,send dev_addr ack failed\r\n");
		i2c_stop();
		return 1;
	}
	
	i2c_send_byte(reg_addr);/* 发送寄存器地址 */
	if(i2c_wait_ack(5000))/* 等待应答 */
	{	
		rt_kprintf("i2c_write_bytes,send reg_addr ack failed\r\n");
		i2c_stop();
		return 1;
	}
	
	while(num--)/* 循环写入数据 */
	{
		i2c_send_byte(*buf);/* 发送数据 */
		if(i2c_wait_ack(5000))/* 等待应答 */
		{
			rt_kprintf("i2c_write_bytes,send data ack failed[%d]\r\n", num);
			i2c_stop();
			return 1;
		}
		buf++;/* 数据指针偏移到下一个 */
		//rt_hw_us_delay(10);
	}
	i2c_stop();	/* 停止信号 */
	
	return 0;
}

/**************************************************************
函数名称 : i2c_read_bytes
函数功能 : 软件 i2c 读多个数据
输入参数 : slave_addr：从机地址
		   reg_addr：寄存器地址
		   buf：需要读取的数据缓存
		   num：数据长度
返回值  	 : 0：成功
		   1：失败
备注		 : *buf是一个数组或指向一个缓存区的指针
**************************************************************/
u8 i2c_read_bytes(u8 slave_addr, u8 reg_addr, u8 *buf, u8 num)
{
	u8 i;
	
	i2c_start();/* 起始信号 */
	
	i2c_send_byte((slave_addr << 1) | 0x00);/* 发送设备地址(写) */
	if(i2c_wait_ack(5000))/* 等待应答 */
	{
		rt_kprintf("i2c_read_bytes,send dev_addr(write) ack failed\r\n");
		i2c_stop();
		return 1;
	}
	
	i2c_send_byte(reg_addr);/* 发送寄存器地址 */
	if(i2c_wait_ack(5000))/* 等待应答 */
	{
		rt_kprintf("i2c_read_bytes,send reg_addr ack failed\r\n");
		i2c_stop();
		return 1;
	}
	i2c_stop();/* 停止信号，一定要向停止信号 */
	
	i2c_start();/* 重启信号 */
	i2c_send_byte((slave_addr << 1) | 0X01);/* 发送设备地址(读) */
	if(i2c_wait_ack(5000))/* 等待应答 */
	{
		rt_kprintf("i2c_read_bytes,send reg_addr ack failed\r\n");
		i2c_stop();
		return 1;
	}
	
	for(i = 0; i < num; i++)
	{
		if(i == (num - 1))
        {
            buf[i] = i2c_read_byte();
            i2c_nack();/* 最后一个数据需要回NACK */
        }
        else
        {
            buf[i] = i2c_read_byte();/* 回应ACK */
            i2c_ack();
        }
	}
	
	i2c_stop();	/* 停止信号 */
	
	return 0;
}








