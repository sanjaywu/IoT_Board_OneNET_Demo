#include "rtthread.h"
#include <rtdevice.h>
#include "string.h"
#include "board.h"
#include "edpkit.h"
#include "socket_common.h"
#include "onenet_edp.h"
#include "data_stream.h"

#include "aht10.h"

#define USING_DEVICEID_APIKEY_LINK									/* 使用设备ID和APIKEY连接 */
#define ONENET_EDP_SRV_ADDR			"183.230.40.39"					/* OneNET EDP协议对应的服务器地址 */
#define ONENET_EDP_SRV_PORT			876								/* OneNET EDP协议对应的服务器端口 */
#define ONENET_EDP_PRODUCT_ID		"190254"						/* 产品id */
#define ONENET_EDP_PRODUCT_APIKEY	"cWPICK6PDU6cOHP=T0SqMcXWRc4="	/* api key */
#define ONENET_EDP_DEVICE_ID		"504890772"						/* 设备id */
#define ONENET_EDP_DEVICE_AUTHKEY	"edp20181122"					/* 设备鉴权信息 */


/* 信号量 */
rt_sem_t link_onenet_sem = RT_NULL;
static rt_sem_t send_heart_sem = RT_NULL;
static rt_sem_t humi_temp_sem = RT_NULL;
static rt_sem_t send_humi_temp_sem = RT_NULL;

/* 线程 */
static rt_thread_t link_dev_thread = RT_NULL;
static rt_thread_t send_heart_thread = RT_NULL;
static rt_thread_t recv_data_pro_thread = RT_NULL;
static rt_thread_t get_humi_temp_thread = RT_NULL;
static rt_thread_t send_humi_temp_thread = RT_NULL;

/* 软件定时器 */
rt_timer_t send_heart_timer = RT_NULL;
rt_timer_t send_humi_temp_timer = RT_NULL;


/* led状态 */
int g_led_status = 0;
ONENET_DATA_STREAM led_status_data_stream[] = {
													{"LED_STATUS", &g_led_status, TYPE_INT, 1}
											  };
unsigned char led_status_data_stream_cnt = sizeof(led_status_data_stream) / sizeof(led_status_data_stream[0]);

/* beep状态 */
int g_beep_status = 0;
ONENET_DATA_STREAM beep_status_data_stream[] = {
													{"BEEP_STATUS", &g_beep_status, TYPE_INT, 1}
											   };
unsigned char beep_status_data_stream_cnt = sizeof(beep_status_data_stream) / sizeof(beep_status_data_stream[0]);

/* motor状态 */
int g_motor_status = 0;
ONENET_DATA_STREAM motor_status_data_stream[] = {
													{"MOTOR_STATUS", &g_motor_status, TYPE_INT, 1}
											  	};
unsigned char motor_status_data_stream_cnt = sizeof(motor_status_data_stream) / sizeof(motor_status_data_stream[0]);

/* motor状态 */
unsigned int g_data_time = 0;
ONENET_DATA_STREAM data_time_data_stream[] = {
													{"DATA_TIME", &g_data_time, TYPE_UINT, 1}
											 };
unsigned char data_time_data_stream_cnt = sizeof(data_time_data_stream) / sizeof(data_time_data_stream[0]);


/* 温湿度 */
float g_temperature = 0.0; 
float g_humidity = 0.0;
ONENET_DATA_STREAM humi_temp_data_stream[] = {
													{"temperature", &g_temperature, TYPE_FLOAT, 1},
													{"humidity", &g_humidity, TYPE_FLOAT, 1},
									 		 };
unsigned char humi_temp_data_stream_cnt = sizeof(humi_temp_data_stream) / sizeof(humi_temp_data_stream[0]);/* 数据流个数 */



/* socket id */
int g_socket_id = -1;	
/**************************************************************
函数名称:onenet_edp_link_device
函数功能:连接OneNET平台
输入参数:srv_addr:服务器地址
		 srv_port:服务器端口
		 id:设备ID/产品ID
		 auth_key:产品APIKEY/设备鉴权信息
返 回 值:RT_EOK:连接成功，错误码:连接失败
备     注:这里包括创建socket、连接服务器、连接设备
**************************************************************/
rt_err_t onenet_edp_link_device(char *srv_addr, unsigned int srv_port, char* id, char* auth_key)
{
	EDP_PACKET_STRUCTURE edpPacket = {NULL, 0, 0, 0};	/* 协议包 */

	if(g_socket_id >= 0)
	{
		if(CLOSE_OK == socket_close(g_socket_id))
		{
			rt_kprintf("[EDP]socket close success\r\n");
		}
		else
		{
			rt_kprintf("[EDP]socket close failed\r\n");
		}
	}
	
	g_socket_id = socket_create();	/* 创建socket */
	
	if(g_socket_id < 0)
	{
		rt_kprintf("[EDP]socket create failed, socket_id < 0\r\n");
		return RT_ERROR;
	}
	if(CONNECT_ERROR == socket_connect_service(g_socket_id, srv_addr, srv_port))
	{
		rt_kprintf("[EDP]connect onenet failed\r\n");
		return RT_ERROR;
	}
	#ifdef USING_DEVICEID_APIKEY_LINK
	if(EDP_PacketConnect1(id, auth_key, 256, &edpPacket) == 1)	/* 根据devid 和 apikey封装协议包 */
	#else
	if(EDP_PacketConnect2(id, auth_key, 256, &edpPacket) == 1)	/* 根据产品id 和 鉴权信息封装协议包 */
	#endif
	{
		EDP_DeleteBuffer(&edpPacket);	/* 删包 */
		rt_kprintf("[EDP]EDP_PacketConnect failed\r\n");
		return RT_ERROR;
	}
	else
	{
		if(socket_send(g_socket_id, edpPacket._data, edpPacket._len) > 0)
		{
			rt_kprintf("[EDP]EDP_PacketConnect send success\r\n");
		}
		else
		{
			rt_kprintf("[EDP]EDP_PacketConnect send failed\r\n");
		}
		EDP_DeleteBuffer(&edpPacket);	/* 删包 */
		return RT_EOK;
	}
}

/**************************************************************
函数名称:onenet_edp_send_heart
函数功能:发送心跳,保持与OneNET的连接
输入参数:无
返 回 值:无
备     注:EDP连接默认超时时间为4分钟。设备登录后，
在超时期内无数据传输时，需要定期向平台发送PING_REQ消息以保持连接
**************************************************************/
void onenet_edp_send_heart(void)
{
	EDP_PACKET_STRUCTURE edpPacket = {NULL, 0, 0, 0};	/* 协议包 */
	EDP_PacketPing(&edpPacket);
	socket_send(g_socket_id, edpPacket._data, edpPacket._len);/* 向平台上传心跳请求 */
	rt_kprintf("[EDP]send heart\r\n");
	EDP_DeleteBuffer(&edpPacket);	/* 删包 */
}

/**************************************************************
函数名称:onenet_edp_send_data
函数功能:上传数据到平台设备
输入参数:type：发送数据的格式
		 devid：设备ID
		 apikey：设备apikey
		 streamArray：数据流
		 streamArrayNum：数据流个数
返 回 值:无
备     注:无
**************************************************************/
rt_err_t onenet_edp_send_data(ONENET_FORMAT_TYPE type, char *devid, char *apikey, ONENET_DATA_STREAM *streamArray, unsigned short streamArrayCnt)
{
	rt_err_t result = RT_ERROR;
	EDP_PACKET_STRUCTURE edpPacket = {NULL, 0, 0, 0};	/* 协议包 */
	short body_len = 0;

	body_len = DSTREAM_GetDataStream_Body_Measure(type, streamArray, streamArrayCnt, 0); /* 获取当前需要发送的数据流的总长度 */

	if(body_len > 0)
	{
		if(EDP_PacketSaveData(devid, body_len, NULL, (SaveDataType)type, &edpPacket) == 0)	/* 封包 */
		{
			body_len = DSTREAM_GetDataStream_Body(type, streamArray, streamArrayCnt, edpPacket._data, edpPacket._size, edpPacket._len);
				
			if(body_len > 0)
			{
				edpPacket._len += body_len;
				rt_kprintf("Send %d Bytes\r\n", edpPacket._len);
				if(socket_send(g_socket_id, edpPacket._data, edpPacket._len) > 0)
				{
					result = RT_EOK;;
				}
				else
				{
					result = RT_ERROR;
				}
			}
			else
			{
				result = RT_ERROR;
				rt_kprintf("DSTREAM_GetDataStream_Body Failed\r\n");
			}
			EDP_DeleteBuffer(&edpPacket);	//删包
		}
		else
		{
			result = RT_ERROR;
			rt_kprintf("EDP_NewBuffer Failed\r\n");
		}
	}	
	else
	{
		result = RT_ERROR;
		rt_kprintf("body_len < 0\r\n");
	}

	return result;
}


void motor_ctrl(rt_uint8_t on)
{
	if(on)
    {
        rt_pin_write(PIN_MOTOR_A, PIN_LOW);
        rt_pin_write(PIN_MOTOR_B, PIN_HIGH);
    }
    else
    {
        rt_pin_write(PIN_MOTOR_A, PIN_LOW);
        rt_pin_write(PIN_MOTOR_B, PIN_LOW);
    }
}

void beep_ctrl(rt_uint8_t on)
{
    if(on)
    {
        rt_pin_write(PIN_BEEP, PIN_HIGH);
    }
    else
    {
        rt_pin_write(PIN_BEEP, PIN_LOW);
    }
}

void led_ctrl(rt_uint8_t on)
{
    if(on)
    {
        rt_pin_write(PIN_LED_R, PIN_LOW);
    }
    else
    {
        rt_pin_write(PIN_LED_R, PIN_HIGH);
    }
}

/**************************************************************
函数名称:onenet_edp_command_callback
函数功能:平台下发命令回调函数
输入参数:req:下发的命令
返 回 值:无
备     注:无
**************************************************************/
void onenet_edp_command_callback(char *req)
{
	char *time_ptr = NULL;
	unsigned int current_time = 0;
	
	rt_pin_mode(PIN_LED_R, PIN_MODE_OUTPUT);
    rt_pin_mode(PIN_MOTOR_A, PIN_MODE_OUTPUT);
    rt_pin_mode(PIN_MOTOR_B, PIN_MODE_OUTPUT);
    rt_pin_mode(PIN_BEEP, PIN_MODE_OUTPUT);
	led_ctrl(0);
	beep_ctrl(0);
	motor_ctrl(0);
	
	/* LED */
	if(strstr((const char*)req, "LED"))
	{
		if(strcmp("LED:1", req) == 0)
		{
			g_led_status = 1;
			led_ctrl(1);
			/* 上传LED状态数据到平台 */
			onenet_edp_send_data(FORMAT_TYPE3, ONENET_EDP_DEVICE_ID, ONENET_EDP_PRODUCT_APIKEY, led_status_data_stream, led_status_data_stream_cnt);
		}
		else if(strcmp("LED:0", req) == 0)
		{
			g_led_status = 0;
			led_ctrl(0);
			/* 上传LED状态数据到平台 */
			onenet_edp_send_data(FORMAT_TYPE3, ONENET_EDP_DEVICE_ID, ONENET_EDP_PRODUCT_APIKEY, led_status_data_stream, led_status_data_stream_cnt);
		}
		else
		{

		}
	}
	/* 蜂鸣器 */
	else if(strstr((const char*)req, "BEEP"))
	{
		if(strcmp("BEEP:1", req) == 0)
		{
			g_beep_status = 1;
			beep_ctrl(1);
			/* 上传BEEP状态数据到平台 */
			onenet_edp_send_data(FORMAT_TYPE3, ONENET_EDP_DEVICE_ID, ONENET_EDP_PRODUCT_APIKEY, beep_status_data_stream, beep_status_data_stream_cnt);
		}
		else if(strcmp("BEEP:0", req) == 0)
		{
			g_beep_status = 0;
			beep_ctrl(0);
			/* 上传BEEP状态数据到平台 */
			onenet_edp_send_data(FORMAT_TYPE3, ONENET_EDP_DEVICE_ID, ONENET_EDP_PRODUCT_APIKEY, beep_status_data_stream, beep_status_data_stream_cnt);
		}
		else
		{

		}
	}
	/* 电机 */
	else if(strstr((const char*)req, "MOTOR"))
	{
		if(strcmp("MOTOR:1", req) == 0)
		{
			g_motor_status = 1;
			motor_ctrl(1);
			/* 上传BEEP状态数据到平台 */
			onenet_edp_send_data(FORMAT_TYPE3, ONENET_EDP_DEVICE_ID, ONENET_EDP_PRODUCT_APIKEY, motor_status_data_stream, motor_status_data_stream_cnt);
		}
		else if(strcmp("MOTOR:0", req) == 0)
		{
			g_motor_status = 0;
			motor_ctrl(0);
			/* 上传BEEP状态数据到平台 */
			onenet_edp_send_data(FORMAT_TYPE3, ONENET_EDP_DEVICE_ID, ONENET_EDP_PRODUCT_APIKEY, motor_status_data_stream, motor_status_data_stream_cnt);
		}
		else
		{

		}
	}
	else if(strstr((const char*)req, "TIME"))
	{
		time_ptr = req + 5;
		g_data_time = (RT_TICK_PER_SECOND * atoi(time_ptr));/* 单位：秒 */
		if(g_data_time > 0)
		{
			rt_kprintf("g_data_time:%d\r\n", g_data_time);
			if(RT_EOK == rt_timer_control(send_humi_temp_timer, RT_TIMER_CTRL_SET_TIME, &g_data_time))
			{
				rt_kprintf("修改定时上传温湿度周期成功，当前周期值为:%d\r\n", g_data_time);
			}
			else
			{
				rt_kprintf("修改定时上传温湿度周期失败\r\n");
			}
		}
		if(RT_EOK == rt_timer_control(send_humi_temp_timer, RT_TIMER_CTRL_GET_TIME, &current_time))
		{
			rt_kprintf("current_time:%d\r\n", current_time);
		}
		/* 上传获取数据周期值到平台 */
		onenet_edp_send_data(FORMAT_TYPE3, ONENET_EDP_DEVICE_ID, ONENET_EDP_PRODUCT_APIKEY, data_time_data_stream, data_time_data_stream_cnt);
	}
	else
	{
		rt_kprintf("receive other commond\r\n");
	}
}

/**************************************************************
函数名称:onenet_edp_receive_process
函数功能:平台下发命令或结果处理函数
输入参数:data:下发的命令或结果
返 回 值:无
备     注:对于开关命令，为了区分，onenet采用命令结构体+命令值
的方式，格式为：命令结构+冒号+命令值，如：LED:1
**************************************************************/
void onenet_edp_receive_process(unsigned char *data)
{
	
	EDP_PACKET_STRUCTURE edpPacket = {NULL, 0, 0, 0};	/* 协议包 */
	char *cmdid_devid = NULL;
	unsigned short cmdid_len = 0;
	char *req = NULL;
	unsigned int req_len = 0;
	
	switch(EDP_UnPacketRecv(data))
	{
		case CONNRESP:
		{
			switch(EDP_UnPacketConnectRsp(data))
			{
				case 0:
				{
					rt_kprintf("Tips:	连接成功\r\n");
					if(send_heart_timer != RT_NULL)
					{
						rt_kprintf("[EDP]start send_heart_timer\r\n");
						rt_timer_start(send_heart_timer);/* 启动软件定时器定时发送心跳 */
					}
					if(send_humi_temp_timer != RT_NULL)
					{
						rt_kprintf("[EDP]start send_humi_temp_timer\r\n");
						rt_timer_start(send_humi_temp_timer);/* 启动软件定时器定时上报数据 */
					}
					break;
				}
				case 1:rt_kprintf("WARN:	连接失败：协议错误\r\n");break;
				case 2:rt_kprintf("WARN:	连接失败：设备ID鉴权失败\r\n");break;
				case 3:rt_kprintf("WARN:	连接失败：服务器失败\r\n");break;
				case 4:rt_kprintf("WARN:	连接失败：用户ID鉴权失败\r\n");break;
				case 5:rt_kprintf("WARN:	连接失败：未授权\r\n");break;
				case 6:rt_kprintf("WARN:	连接失败：授权码无效\r\n");break;
				case 7:rt_kprintf("WARN:	连接失败：激活码未分配\r\n");break;
				case 8:rt_kprintf("WARN:	连接失败：该设备已被激活\r\n");break;
				case 9:rt_kprintf("WARN:	连接失败：重复发送连接请求包\r\n");break;
				default:rt_kprintf("ERR:	连接失败：未知错误\r\n");break;
			}
			break;
		}
		case DISCONNECT:
		{
			rt_kprintf("WARN:连接断开,准备重连\r\n");
			if(send_heart_timer != RT_NULL)
			{
				rt_kprintf("[EDP]stop send_heart_timer\r\n");
				rt_timer_stop(send_heart_timer);/* 关闭软件定时器 */
			}
			if(send_humi_temp_timer != RT_NULL)
			{
				rt_kprintf("[EDP]stop send_humi_temp_timer\r\n");
				rt_timer_stop(send_humi_temp_timer);/* 关闭软件定时器 */
			}
			if(link_onenet_sem != RT_NULL)
			{
				rt_sem_release(link_onenet_sem);
			}
			break;
		}
		case PINGRESP:
		{
			rt_kprintf("Tips:	HeartBeat OK\r\n");
			break;
		}
		case PUSHDATA:/* 解pushdata包，用于设备与设备之间的数据传输 */
		{
			if(EDP_UnPacketPushData(data, &cmdid_devid, &req, &req_len) == 0)
			{
				rt_kprintf("src_devid: %s, req: %s, req_len: %d\r\n", cmdid_devid, req, req_len);
				
				/* 执行命令回调 */
				
				edp_free_buffer(cmdid_devid);	/* 释放内存 */
				edp_free_buffer(req);
			}
			break;
		}
		case CMDREQ:/* 解命令包 */
		{
			if(EDP_UnPacketCmd(data, &cmdid_devid, &cmdid_len, &req, &req_len) == 0)
			{
				/* 命令回复组包 */
				EDP_PacketCmdResp(cmdid_devid, cmdid_len, req, req_len, &edpPacket);
				rt_kprintf("cmdid: %s, req: %s, req_len: %d\r\n", cmdid_devid, req, req_len);
				
				/* 执行命令回调 */
				onenet_edp_command_callback(req);
				edp_free_buffer(cmdid_devid); /* 释放内存 */
				edp_free_buffer(req);
				
				/* 回复命令 */
				socket_send(g_socket_id, edpPacket._data, edpPacket._len);/* 上传平台 */
				EDP_DeleteBuffer(&edpPacket);	/* 删包 */
			}
			break;
		}	
		case SAVEACK:
		{
			if(data[3] == MSG_ID_HIGH && data[4] == MSG_ID_LOW)
			{
				
				rt_kprintf("Tips:	Send %s\r\n", data[5] ? "Err" : "Ok");
			}
			else
			{
				rt_kprintf("Tips:	Message ID Err\r\n");
			}
			break;
		}
		default:break;
	}

}


/**************************************************************
函数名称:onenet_edp_link_device_thread
函数功能:连接OneNET平台设备线程
输入参数:parameter:线程入口参数
返 回 值:无
备     注:WiFi注册上网了后连接平台，连接成功则启动定时器发送心跳
**************************************************************/
void onenet_edp_link_device_thread(void *parameter)
{
	rt_err_t result = RT_ERROR;
	
	while(1)
	{
		result = rt_sem_take(link_onenet_sem, RT_WAITING_FOREVER); /* 等待时间：一直等 */
		if(RT_EOK == result)
		{
			#ifdef USING_DEVICEID_APIKEY_LINK
			onenet_edp_link_device(ONENET_EDP_SRV_ADDR, ONENET_EDP_SRV_PORT, ONENET_EDP_DEVICE_ID, ONENET_EDP_PRODUCT_APIKEY);
			#else
			onenet_edp_link_device(ONENET_EDP_SRV_ADDR, ONENET_EDP_SRV_PORT, ONENET_EDP_PRODUCT_ID, ONENET_EDP_DEVICE_AUTHKEY);
			#endif
		}
		rt_thread_mdelay(1);
		rt_thread_yield();/* 放弃剩余时间片，进行一次线程切换 */
	}
}


/**************************************************************
函数名称:onenet_edp_receive_process
函数功能:平台下发命令或结果处理线程
输入参数:parameter:线程入口参数
返 回 值:无
备     注:无
**************************************************************/
void onenet_edp_receive_process_thread(void *parameter)
{
	char data_ptr[128];
	memset(data_ptr, 0, sizeof(data_ptr));
	
	while(1)
	{
		if(socket_receive(g_socket_id, data_ptr, 128) > 0)
		{
			rt_kprintf("socket receive data\r\n");
			onenet_edp_receive_process((unsigned char *)data_ptr);
			memset(data_ptr, 0, sizeof(data_ptr));
		}	
		rt_thread_mdelay(1);
		rt_thread_yield();/* 放弃剩余时间片，进行一次线程切换 */
	}
}

/**************************************************************
函数名称:onenet_edp_send_heart_thread
函数功能:发送心跳线程
输入参数:parameter:线程入口参数
返 回 值:无
备     注:无
**************************************************************/
void onenet_edp_send_heart_thread(void *parameter)
{
	rt_err_t result = RT_ERROR;
	
	while(1)
	{
		result = rt_sem_take(send_heart_sem, RT_WAITING_FOREVER); /* 等待时间：一直等 */
		if(RT_EOK == result)
		{
			onenet_edp_send_heart();
		}
		rt_thread_mdelay(1);
		rt_thread_yield();/* 放弃剩余时间片，进行一次线程切换 */
	}
}

/**************************************************************
函数名称:onenet_edp_get_humi_temp_thread
函数功能:获取温湿度数据线程
输入参数:parameter:线程入口参数
返 回 值:无
备     注:无
**************************************************************/
void onenet_edp_get_humi_temp_thread(void *parameter)
{
	rt_err_t result = RT_ERROR;
	
	while(1)
	{
		result = rt_sem_take(send_humi_temp_sem, RT_WAITING_FOREVER); /* 等待时间：一直等 */
		if(RT_EOK == result)
		{
			result = rt_sem_take(humi_temp_sem, RT_WAITING_FOREVER); /* 等待时间：一直等 */
			if(RT_EOK == result)
			{
				aht10_read_data(&g_temperature, &g_humidity);
			}
			rt_sem_release(humi_temp_sem);  /* 释放信号量 */
		}
		rt_thread_mdelay(1);
		rt_thread_yield();/* 放弃剩余时间片，进行一次线程切换 */
	}
}


/**************************************************************
函数名称:onenet_edp_send_humi_temp_thread
函数功能:发送温湿度数据线程
输入参数:parameter:线程入口参数
返 回 值:无
备     注:无
**************************************************************/
void onenet_edp_send_humi_temp_thread(void *parameter)
{
	rt_err_t result = RT_ERROR;
	
	while(1)
	{
		result = rt_sem_take(send_humi_temp_sem, RT_WAITING_FOREVER); /* 等待时间：一直等 */
		if(RT_EOK == result)
		{
			result = rt_sem_take(humi_temp_sem, RT_WAITING_FOREVER); /* 等待时间：一直等 */
			if(RT_EOK == result)
			{
				/* 上传温湿度数据到平台 */
				if(RT_EOK == onenet_edp_send_data(FORMAT_TYPE3, ONENET_EDP_DEVICE_ID, ONENET_EDP_PRODUCT_APIKEY, humi_temp_data_stream, humi_temp_data_stream_cnt))
				{
					rt_kprintf("Humi_Temp Send success\n");
				}
				else
				{
					rt_kprintf("Humi_Temp Status Send failed\n");
				}
			}
			rt_sem_release(humi_temp_sem);  /* 释放信号量 */
		}
		rt_thread_mdelay(1);
		rt_thread_yield();/* 放弃剩余时间片，进行一次线程切换 */
	}
}

/**************************************************************
函数名称:send_heart_timer_callback
函数功能:软件定时器回调函数，用于定时发送信号量来触发发送心跳包
输入参数:parameter:入口参数
返 回 值:无
备     注:无
**************************************************************/
void send_heart_timer_callback(void *parameter)
{
	if(send_heart_timer != RT_NULL)
	{
		rt_sem_release(send_heart_sem);
	}
}

/**************************************************************
函数名称:send_humi_temp_timer_callback
函数功能:软件定时器回调函数，用于定时发送信号量来触发上传温湿度
输入参数:parameter:入口参数
返 回 值:无
备     注:无
**************************************************************/
void send_humi_temp_timer_callback(void *parameter)
{
	if(send_humi_temp_timer != RT_NULL)
	{
		rt_sem_release(send_humi_temp_sem);
	}
}

rt_err_t onenet_edp_sample(void)
{
	/* 连接OneNET信号量 */
	link_onenet_sem = rt_sem_create("link_onenet_sem", 0, RT_IPC_FLAG_FIFO);
	if(link_onenet_sem == RT_NULL)
	{
		rt_kprintf("[EDP]link_onenet_sem create failed\r\n");
		return RT_ERROR;
	}

	/* 发送心跳信号量 */
	send_heart_sem = rt_sem_create("send_heart_sem", 0, RT_IPC_FLAG_FIFO);
	if(send_heart_sem == RT_NULL)
	{
		rt_kprintf("[EDP]send_heart_sem create failed\r\n");
		return RT_ERROR;
	}
	
	/* 用于获取温湿度和发送温湿度信号量 */
	humi_temp_sem = rt_sem_create("humi_temp_sem", 1, RT_IPC_FLAG_FIFO);
	if(humi_temp_sem == RT_NULL)
	{
		rt_kprintf("[EDP]humi_temp_sem create failed\r\n");
		return RT_ERROR;
	}

	/* 用于定时触发上报温湿度数据信号量 */
	send_humi_temp_sem = rt_sem_create("send_humi_temp_sem", 0, RT_IPC_FLAG_FIFO);
	if(send_humi_temp_sem == RT_NULL)
	{
		rt_kprintf("[EDP]send_humi_temp_sem create failed\r\n");
		return RT_ERROR;
	}
	
	/* 软件定时器，周期执行，100*RT_TICK_PER_SECOND */
	send_heart_timer = rt_timer_create("send_heart_timer", send_heart_timer_callback, RT_NULL, (100*RT_TICK_PER_SECOND), RT_TIMER_FLAG_SOFT_TIMER | RT_TIMER_FLAG_PERIODIC);
	if(send_heart_timer == RT_NULL)
	{
		rt_kprintf("[EDP]send_heart_timer create failed\r\n");
		return RT_ERROR;
	}
	
	/* 软件定时器，周期执行，300秒 */
	send_humi_temp_timer = rt_timer_create("send_humi_temp_timer", send_humi_temp_timer_callback, RT_NULL, (300*RT_TICK_PER_SECOND), RT_TIMER_FLAG_SOFT_TIMER | RT_TIMER_FLAG_PERIODIC);
	if(send_humi_temp_timer == RT_NULL)
	{
			rt_kprintf("[EDP]send_humi_temp_timer create failed\r\n");
			return RT_ERROR;
	}
		
	/* 连接设备线程 */
	link_dev_thread = rt_thread_create("link_dev_thread", onenet_edp_link_device_thread, RT_NULL, 2048, 4, 200);
    if(link_dev_thread == RT_NULL)
    {
    	rt_kprintf("[EDP]link_dev_thread create failed\r\n");
       	return RT_ERROR;
    }
	rt_thread_startup(link_dev_thread);

	/* 发送心跳线程 */
	send_heart_thread = rt_thread_create("send_heart_thread", onenet_edp_send_heart_thread, RT_NULL, 1024, 4, 50);
    if(send_heart_thread == RT_NULL)
    {
    	rt_kprintf("[EDP]send_heart_thread create failed\r\n");
       	return RT_ERROR;
    }
	rt_thread_startup(send_heart_thread);
	
	/* 平台下发命令或结果处理线程 */
	recv_data_pro_thread = rt_thread_create("recv_data_pro_thread", onenet_edp_receive_process_thread, RT_NULL, 4096, 4, 100);
    if(recv_data_pro_thread == RT_NULL)
    {
    	rt_kprintf("[EDP]recv_data_pro_thread create failed\r\n");
       	return RT_ERROR;
    }
	rt_thread_startup(recv_data_pro_thread);

	/* 获取温湿度数据线程 */
	get_humi_temp_thread = rt_thread_create("get_humi_temp_thread", onenet_edp_get_humi_temp_thread, RT_NULL, 4096, 4, 50);
    if(get_humi_temp_thread == RT_NULL)
    {
    	rt_kprintf("[EDP]get_humi_temp_thread create failed\r\n");
       	return RT_ERROR;
    }
	rt_thread_startup(get_humi_temp_thread);

	/* 上报温湿度数据到平台线程 */
	send_humi_temp_thread = rt_thread_create("send_humi_temp_thread", onenet_edp_send_humi_temp_thread, RT_NULL, 4096, 4, 100);
    if(send_humi_temp_thread == RT_NULL)
    {
    	rt_kprintf("[EDP]send_humi_temp_thread create failed\r\n");
       	return RT_ERROR;
    }
	rt_thread_startup(send_humi_temp_thread);
	
	return RT_EOK;	
}





