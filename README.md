# RT-Thread学习的综合应用——使用AP6181 WiFi模组对接OneNET应用示例

>学习了RT-Thread的内核也有一段时间了，由于各种各样的琐事自己没有去做一个综合应用示例，刚最近有点时间，做了一个对接OneNET的历程，采用的是OneNET的EDP协议，关于OneNET的EDP协议可以[点击跳转至OneNET EDP协议讲解与应用](https://blog.csdn.net/Sanjay_Wu/article/details/84800189)这篇博客看一下，这篇博客会比较详细的介绍对接EDP协议的思路。本篇博客会根据着重的将RT-Thread的应用思路，在本应用示例也只是用来软件定时器、线程管理、信号量而已，因为我用的是潘多拉开发板进行实验，所以AP6181驱动就直接用RT-Thread的，自己也只是在这个基础上做了应用。
>使用AP6181 WiFi模组对接OneNET应用示例：STM32+AP6181（SDIO WiFi模组）对接OneNET，实现温湿度定时上报，控制上报周期、控制设备LED灯、蜂鸣器以及电机，掉线自动连接、WiFi自动连接。

## 一、移植需要修改的地方

在移植时，主要也就是修改几个地方，代码如下：
<pre name="code" class="c">
#define ONENET_EDP_PRODUCT_ID		"190254"	/* 产品id */
#define ONENET_EDP_PRODUCT_APIKEY	"cWPICK6PDU6cOHP=T0SqMcXWRc4="	/* api key */
#define ONENET_EDP_DEVICE_ID		"504890772"	/* 设备id */
#define ONENET_EDP_DEVICE_AUTHKEY	"edp20181122"	/* 设备鉴权信息 */
</pre>
需要修改的为产品ID、APIKEY、设备ID、设备鉴权信息，将这些信息修改为你自己在OneNET开发者中心的产品信息或设备信息。

## 二、连接OneNET思路

1、开机后，RT-Thread完成一系列的初始化后进入main线程，首先进入main函数会进行执行`onenet_edp_sample();`函数进行信号量创建、软件定时器创建以及线程创建：
<pre name="code" class="c">
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
	
	/* 软件定时器，周期执行，120秒 */
	send_heart_timer = rt_timer_create("send_heart_timer", send_heart_timer_callback, RT_NULL, 120000, RT_TIMER_FLAG_SOFT_TIMER | RT_TIMER_FLAG_PERIODIC);
	if(send_heart_timer == RT_NULL)
	{
		rt_kprintf("[EDP]send_heart_timer create failed\r\n");
		return RT_ERROR;
	}
	
	/* 软件定时器，周期执行，300秒 */
	send_humi_temp_timer = rt_timer_create("send_humi_temp_timer", send_humi_temp_timer_callback, RT_NULL, 30000, RT_TIMER_FLAG_SOFT_TIMER | RT_TIMER_FLAG_PERIODIC);
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
	recv_data_pro_thread = rt_thread_create("recv_data_pro_thread",onenet_edp_receive_process_thread, RT_NULL, 4096, 4, 100);
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

</pre>


2、完成信号量创建、软件定时器的线程创建后，会进行注册网络就绪事件回调、网络断开事件回调、以及连接网络、开启自动连接网络：
<pre name="code" class="c">
/* 注册网络就绪回调、 */
rt_wlan_register_event_handler(RT_WLAN_EVT_READY, wlan_ready_handler, RT_NULL);
/* 注册网络断开事件回调 */
rt_wlan_register_event_handler(RT_WLAN_EVT_STA_DISCONNECTED, wlan_station_disconnect_handler, RT_NULL);
/* 连接路由器 */
rt_wlan_connect(WLAN_SSID, WLAN_PASSWORD);
rt_thread_mdelay(1000);
/* 开启自动连接 */
wlan_autoconnect_init();
rt_wlan_config_autoreconnect(RT_TRUE);
</pre>

3、网络就绪回调函数用于释放连接OneNET信号量，告诉系统已经有网络了，可以去连接OneNET了：
<pre name="code" class="c">
void wlan_ready_handler(int event, struct rt_wlan_buff *buff, void *parameter)
{
	rt_kprintf("wlan ready\r\n");
	if(link_onenet_sem != RT_NULL)
	{
		rt_sem_release(link_onenet_sem);
	}
}
</pre>
而另一方面，连接OneNET线程会移植挂起等待连接OneNET的信号量，指导获取到这个信号量才开始去连接OneNET：
<pre name="code" class="c">
/**************************************************************
函数名称:onenet_edp_link_device_thread
函数功能:连接OneNET平台设备线程
输入参数:parameter:线程入口参数
返 回 值:无
备    注:WiFi注册上网了后连接平台，连接成功则启动定时器发送心跳
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
</pre>

## 三、数据上报和心跳发送思路

1、连接上OneNET之后，处理平台下发命令或结果的线程会得到连接成功返回结果并进行处理，启动软件定时器进行发送心跳和上报数据：
<pre name="code" class="c">
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
</pre>

2、软件定时器回调函数用于释放发送心跳信号量：
<pre name="code" class="c">
/**************************************************************
函数名称:send_heart_timer_callback
函数功能:软件定时器回调函数，用于定时发送信号量来触发发送心跳包
输入参数:parameter:入口参数
返 回 值:无
备    注:无
**************************************************************/
void send_heart_timer_callback(void *parameter)
{
	if(send_heart_timer != RT_NULL)
	{
		rt_sem_release(send_heart_sem);
	}
}
</pre>

3、发送心跳线程获取到信号量之后执行发送心跳操作：
<pre name="code" class="c">
/**************************************************************
函数名称:onenet_edp_send_heart_thread
函数功能:发送心跳线程
输入参数:parameter:线程入口参数
返 回 值:无
备    注:无
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
</pre>

4、同理，上报数据也是等待信号量，一旦获取到就上报数据，为保证数据上报的同步，获取数据与上报数据用来二值信号量，实现代码：
<pre name="code" class="c">
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
</pre>

## 四、设备断开与网络断开处理

1、如果在有网络的情况下设备突然断开，那么就需要重新连接OneNET了，首先会停止所有定时器、接着重新发送连接OneNET信号量，处理如下：
<pre name="code" class="c">
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
</pre>

2、如果是网络断开，那么就需要停止软件定时器，处理如下：
<pre name="code" class="c">
void wlan_station_disconnect_handler(int event, struct rt_wlan_buff *buff, void *parameter)
{
	rt_kprintf("disconnect from the network!\n");
	
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
}
</pre>

## 五、在OneNET实现的应用示例

[点击跳转至在OneNET实现的应用](https://open.iot.10086.cn/iotbox/appsquare/appview?openid=8da9012c0dcc77af73399b63a29662d8)
![在这里插入图片描述](https://img-blog.csdnimg.cn/20190316115745161.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L1NhbmpheV9XdQ==,size_16,color_FFFFFF,t_70)

## 六、图片示例

1、连接成功

![在这里插入图片描述](https://img-blog.csdnimg.cn/20190316120049678.png)


2、发送心跳成功

![在这里插入图片描述](https://img-blog.csdnimg.cn/20190316120220575.png)

3、上报数据成功

![在这里插入图片描述](https://img-blog.csdnimg.cn/20190316120324545.png)

4、接收命令执行操作以及上报状态成功：

![在这里插入图片描述](https://img-blog.csdnimg.cn/20190316120500474.png)



## 七、FinSH抓取连接OneNET、发送心跳、上报数据控制设备的信息

1、信息如下：
<pre name="code" class="c">
 \ | /
- RT -     Thread Operating System
 / | \     4.0.0 build Mar 15 2019
 2006 - 2018 Copyright by rt-thread team
lwIP-2.0.2 initialized!
[SFUD] Find a Winbond flash chip. Size is 16777216 bytes.
[SFUD] w25q128 flash device is initialize success.
msh />[I/FAL] RT-Thread Flash Abstraction Layer (V0.2.0) initialize success.
[I/OTA] RT-Thread OTA package(V0.1.3) initialize success.
[I/OTA] Verify 'wifi_image' partition(fw ver: 1.0, timestamp: 1529386280) success.
[I/WICED] wifi initialize done. wiced version 3.3.1
[I/WLAN.dev] wlan init success
[I/WLAN.lwip] eth device init ok name:w0
join ssid:MY_WiFi
[I/WLAN.mgnt] wifi connect success ssid:MY_WiFi
[Flash] EasyFlash V3.2.1 is initialize success.
[Flash] You can get the latest version on https://github.com/armink/EasyFlash .
wlan ready
socket create success!!!, socket_id:0
[I/WLAN.lwip] Got IP address : 192.168.43.5
socket connect success!!!
[EDP]EDP_PacketConnect send success
socket receive data
Tips:   连接成功
[EDP]start send_heart_timer
[EDP]start send_humi_temp_timer
[EDP]send heart
socket receive data
cmdid: 00a8d84c-d740-586c-94d1-4e6ac32dc883, req: LED:1, req_len: 5
Send 35 Bytes
socket receive data
Tips:   HeartBeat OK
socket receive data
socket receive data
Tips:   Send Ok
[EDP]send heart
socket receive data
Tips:   HeartBeat OK
socket receive data
cmdid: 4b3ffb43-4f0c-50f3-9370-1ec9179658d8, req: LED:0, req_len: 5
Send 35 Bytes
socket receive data
socket receive data
Tips:   Send Ok
socket receive data
cmdid: 3c039820-9487-5415-948d-f209550672e5, req: LED:1, req_len: 5
Send 35 Bytes
socket receive data
socket receive data
Tips:   Send Ok
socket receive data
cmdid: e8f37c5b-d136-5ff5-95f2-40a530d9d553, req: LED:0, req_len: 5
Send 35 Bytes
socket receive data
socket receive data
Tips:   Send Ok
socket receive data
cmdid: f38d85db-2b5b-51a9-94c7-6fda84de0ce1, req: LED:1, req_len: 5
Send 35 Bytes
socket receive data
socket receive data
Tips:   Send Ok
socket receive data
cmdid: f2d9c7cc-0b02-594d-84b6-2b8e344a621f, req: LED:0, req_len: 5
Send 35 Bytes
socket receive data
socket receive data
Tips:   Send Ok
[EDP]send heart
socket receive data
Tips:   HeartBeat OK
[EDP]send heart
socket receive data
Tips:   HeartBeat OK
[EDP]send heart
socket receive data
Tips:   HeartBeat OK
socket receive data
cmdid: 4fc4d0de-078e-5ce5-bae1-cc4fdff23ec8, req: LED:1, req_len: 5
Send 35 Bytes
socket receive data
socket receive data
Tips:   Send Ok
socket receive data
cmdid: 5f56d146-0313-5814-9c62-f5836221aebd, req: LED:0, req_len: 5
Send 35 Bytes
socket receive data
socket receive data
Tips:   Send Ok
[EDP]send heart
Send 65 Bytes
Humi_Temp Send success
socket receive data
Tips:   HeartBeat OK
socket receive data
socket receive data
Tips:   Send Ok
[EDP]send heart
socket receive data
Tips:   HeartBeat OK
[EDP]send heart
socket receive data
Tips:   HeartBeat OK
[EDP]send heart
socket receive data
Tips:   HeartBeat OK
[EDP]send heart
socket receive data
Tips:   HeartBeat OK
[EDP]send heart
socket receive data
Tips:   HeartBeat OK
[EDP]send heart
Send 65 Bytes
Humi_Temp Send success
socket receive data
Tips:   HeartBeat OK
socket receive data
socket receive data
Tips:   Send Ok
[EDP]send heart
socket receive data
Tips:   HeartBeat OK
socket receive data
cmdid: 6235c513-bee0-5a46-a5ef-9282377f352a, req: MOTOR:1, req_len: 7
Send 37 Bytes
socket receive data
socket receive data
Tips:   Send Ok
socket receive data
cmdid: fcbb4818-eeb7-5cac-b122-41b6329d2c99, req: MOTOR:0, req_len: 7
Send 37 Bytes
socket receive data
socket receive data
Tips:   Send Ok
</pre>
