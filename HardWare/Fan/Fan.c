#include "stm32l4xx.h"                  // Device header
#include "Fan.h"
#include "tim.h"

/* 风扇 = PA8，TIM1_CH1 PWM，经 S8050 + AO3415A 驱动（图2-3 那个电路）
 *
 * 现在只做开 / 关，不做调速：
 *   TIM1 的 PSC/ARR 被舵机1 占用，整条定时器跑到 50Hz。
 *   50Hz 下"占空比 100%"就是常高电平，风扇全速；"0%"就是常低，停转。
 *   中间那些占空比对直流电机没有意义（50Hz 会抖、低速起不来），所以干脆不给。
 *
 * 想恢复调速的话：把 TIM1 改回 2kHz，舵机1 就得换成 TIM16 软件脉冲
 * （那套代码在 11-05 的版本里，Servo.c 有记录）。 */

#define FAN_FULL_CCR   2000     /* ARR=1999，写 2000 就是整周期都是高电平 */

static uint8_t s_on = 0;

void Fan_Init(void)
{
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    Fan_Off();
}

void Fan_On(void)
{
    s_on = 1;
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, FAN_FULL_CCR);
}

void Fan_Off(void)
{
    s_on = 0;
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
}

uint8_t Fan_IsOn(void)
{
    return s_on;
}
