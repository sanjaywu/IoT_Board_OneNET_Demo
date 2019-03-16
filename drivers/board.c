/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2009-01-05     Bernard      first implementation
 */

#include <stdint.h>
#include <rthw.h>
#include <rtthread.h>

#include <stm32l4xx_hal.h>

#include "board.h"
#ifdef RT_USING_SERIAL
#include "drv_usart.h"
#endif
#ifdef BSP_USING_GPIO
#include "drv_gpio.h"
#endif

#if defined(RT_USING_MEMHEAP) && defined(RT_USING_MEMHEAP_AS_HEAP)
static struct rt_memheap system_heap;
#endif

/**
 *  HAL adaptation
 */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    /* Return function status */
    return HAL_OK;
}

uint32_t HAL_GetTick(void)
{
    return rt_tick_get() * 1000 / RT_TICK_PER_SECOND;
}

void HAL_SuspendTick(void)
{
    return ;
}

void HAL_ResumeTick(void)
{
    return ;
}

void HAL_Delay(__IO uint32_t Delay)
{
    return ;
}

/*  
其中入口参数 us 指示出需要延时的微秒数目，
这个函数只能支持低于 1 OS Tick 的延时，否则 SysTick
会出现溢出而不能够获得指定的延时时间
*/
void rt_hw_us_delay(rt_uint32_t us)
{
	rt_uint32_t delta;
	/* 获 得 延 时 经 过 的 tick 数 */
	us = us * (SysTick->LOAD/(1000000/RT_TICK_PER_SECOND));
	/* 获 得 当 前 时 间 */
	delta = SysTick->VAL;
	/* 循 环 获 得 当 前 时 间， 直 到 达 到 指 定 的 时 间 后 退 出 循环 */
	while (delta - SysTick->VAL< us);
}

void rt_hw_ms_delay(rt_uint32_t ms)
{
	rt_uint32_t i;

	for(i = 0; i < ms; i++)
	{
		rt_hw_us_delay(500);
		rt_hw_us_delay(500);
	}
}

/*===================================================*/

/**
 * This is the timer interrupt service routine.
 *
 */
void SysTick_Handler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    rt_tick_increase();

    /* leave interrupt */
    rt_interrupt_leave();
}

/**
 * This function will initial STM32 board.
 */
void rt_hw_board_init()
{
    /* NVIC Configuration */
#define NVIC_VTOR_MASK              0x3FFFFF80
#ifdef  VECT_TAB_RAM
    /* Set the Vector Table base location at 0x10000000 */
    SCB->VTOR  = (0x10000000 & NVIC_VTOR_MASK);
#else  /* VECT_TAB_FLASH  */
    /* Set the Vector Table base location at 0x08000000 */
    SCB->VTOR  = (0x08000000 & NVIC_VTOR_MASK);
#endif

    /* HAL_Init() function is called at the beginning of program after reset and before
     * the clock configuration.
     */
    HAL_Init();

    SystemClock_Config();
		
    /* First initialize pin port,so we can use pin driver in other bsp driver */
#ifdef BSP_USING_GPIO
    rt_hw_pin_init();
#endif

    /* Second initialize the serial port, so we can use rt_kprintf right away */
#ifdef RT_USING_SERIAL
    stm32_hw_usart_init();
#endif

#ifdef RT_USING_CONSOLE
    rt_console_set_device(RT_CONSOLE_DEVICE_NAME);
#endif

#if defined(RT_USING_MEMHEAP) && defined(RT_USING_MEMHEAP_AS_HEAP)
    rt_system_heap_init((void *)HEAP_BEGIN, (void *)HEAP_END);
    rt_memheap_init(&system_heap, "sram2", (void *)STM32_SRAM2_BEGIN, STM32_SRAM2_HEAP_SIZE);
#else
    rt_system_heap_init((void *)HEAP_BEGIN, (void *)HEAP_END);
#endif

#ifdef RT_USING_COMPONENTS_INIT
    rt_components_board_init();
#endif
}

/**
 * This function will get STM32 uid.
 */
void rt_get_cpu_id(rt_uint32_t cpuid[3])
{
    /* get stm32 uid */
    cpuid[0] = *(volatile rt_uint32_t *)(0x1FFF7590);
    cpuid[1] = *(volatile rt_uint32_t *)(0x1FFF7590 + 4);
    cpuid[2] = *(volatile rt_uint32_t *)(0x1FFF7590 + 8);
}

#ifdef RT_USING_FINSH
#include <finsh.h>
static void reboot(uint8_t argc, char **argv)
{
    rt_hw_cpu_reset();
}
FINSH_FUNCTION_EXPORT_ALIAS(reboot, __cmd_reboot, Reboot System);
#endif /* RT_USING_FINSH */
