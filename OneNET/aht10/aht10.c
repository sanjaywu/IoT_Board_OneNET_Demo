#include "i2c.h"
#include "aht10.h"
#include "rtthread.h"
#include "board.h"

#define AHT10_DEVICE_ADDR 0x38
#define AHT10_CALIBRATION_REG 0xE1
#define AHT10_MEASUREMENT_REG 0xAC
#define AHT10_NORMAL_REG 0xA8


/**************************************************************
函数名称 : aht10_read_data
函数功能 : aht10读取温湿度数据
输入参数 : 无
返回值  	 : 无
备注		 : Temp = (T[19:0]/(1<<20))*200 - 50
		   Humi = (H[19:0]/(1<<20))*100
**************************************************************/
void aht10_read_data(float *temperature, float *humidity)
{
	u8 raw_tmp[6];
	u32 temp;
	u32 humi;
	//u8 status;
	
	i2c_read_bytes(AHT10_DEVICE_ADDR, AHT10_MEASUREMENT_REG, raw_tmp, 6);

	//status = raw_tmp[0];/* 状态位 */
	temp = (((raw_tmp[3] & 0x0F) << 16) | (raw_tmp[4] << 8) | (raw_tmp[5]));/* 20 bit 原始温度数据 */
	humi = (((raw_tmp[1]) << 12) | (raw_tmp[2] << 4) | (raw_tmp[3] & 0xF0));/* 20 bit原始湿度数据 */
	//rt_kprintf("status:%x\r\n", status);
	//rt_kprintf("tmep:%d\r\n", temp);
	//rt_kprintf("humi:%d\r\n", humi);
	*humidity = ((humi * 100.0) / (1 << 20));
	*temperature = ((temp * 200.0) / (1 << 20)) - 50.0;

	//rt_kprintf("humidity   : %d.%d %%\n", (int)*humidity, (int)(*humidity * 10) % 10); /* former is integer and behind is decimal */
	//rt_kprintf("temperature: %d.%d \n", (int)*temperature, (int)(*temperature * 10) % 10); /* former is integer and behind is decimal */
	
}

/**************************************************************
函数名称 : aht10_init
函数功能 : aht10初始化
输入参数 : 无
返回值  	 : 0：成功
		   1：失败
备注		 : 传感器上电之后需先发0xE1,此命令刚上电只需发送一次
**************************************************************/
int aht10_init(void)
{
	u8 temp[2] = {0, 0};
	
	i2c_init();

	if(i2c_write_one_byte(AHT10_DEVICE_ADDR, AHT10_NORMAL_REG, NULL))
	{
		return RT_ERROR;
	}
	rt_hw_ms_delay(300);	

	temp[0] = 0x08;
    temp[1] = 0x00;
	/* 当传感器启动通信后，先发送命令 0xE1，使输出数据进入校准状态 */
	if(i2c_write_bytes(AHT10_DEVICE_ADDR, AHT10_CALIBRATION_REG, temp, 2))
	{
		return RT_ERROR;
	}
	rt_hw_ms_delay(300);/* 在 0xE1 发送后需要等待不少于300ms才能读取温湿度数据 */
	
	return RT_EOK;
}


INIT_BOARD_EXPORT(aht10_init);







