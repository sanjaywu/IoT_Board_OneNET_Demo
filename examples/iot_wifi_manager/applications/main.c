/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-09-01     ZeroFree     first implementation
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <msh.h>

#include "drv_wlan.h"
#include "wifi_config.h"

#include <stdio.h>
#include <stdlib.h>

#include "onenet_edp.h"
extern rt_sem_t link_onenet_sem;
extern rt_timer_t send_heart_timer;
extern rt_timer_t send_humi_temp_timer;

#define WLAN_SSID               "MY_WiFi"
#define WLAN_PASSWORD           "1234567890"


void wlan_ready_handler(int event, struct rt_wlan_buff *buff, void *parameter)
{
	rt_kprintf("wlan ready\r\n");
	if(link_onenet_sem != RT_NULL)
	{
			rt_sem_release(link_onenet_sem);
	}
}

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


int main(void)
{
	onenet_edp_sample();
	
    /* register network ready event callback */
    rt_wlan_register_event_handler(RT_WLAN_EVT_READY, wlan_ready_handler, RT_NULL);
    /* register wlan disconnect event callback */
    rt_wlan_register_event_handler(RT_WLAN_EVT_STA_DISCONNECTED, wlan_station_disconnect_handler, RT_NULL);
	rt_wlan_connect(WLAN_SSID, WLAN_PASSWORD);

	rt_thread_mdelay(1000);
	
    wlan_autoconnect_init();
    /* enable wlan auto connect */
    rt_wlan_config_autoreconnect(RT_TRUE);
	
    return 0;

}



