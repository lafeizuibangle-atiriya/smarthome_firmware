#include "stm32l4xx.h"                  // Device header
#include "Light.h"

/* 灯：PB15，经 S8050 + AO3415A 驱动，高电平点亮。
 * 如果换到别的板子上灯不亮，把下面的引脚改成 PB11 试一次
 * （原理图上标的是 PB11，实际这块板接的是 PB15）。
 * 内部记一个状态量，Light_On / Light_Off / Light_Turn 都改同一个状态，
 * 避免自动规则直接写引脚、按键却按自己记的状态翻转，导致状态错乱 */

#define LIGHT_PORT      GPIOB
#define LIGHT_PIN       GPIO_PIN_15

static uint8_t s_on = 0;        /* 0 = 关，1 = 开 */

void Light_On(void)
{
    s_on = 1;
    HAL_GPIO_WritePin(LIGHT_PORT, LIGHT_PIN, GPIO_PIN_SET);
}

void Light_Off(void)
{
    s_on = 0;
    HAL_GPIO_WritePin(LIGHT_PORT, LIGHT_PIN, GPIO_PIN_RESET);
}

void Light_Turn(void)
{
    if (s_on) Light_Off();
    else      Light_On();
}

uint8_t Light_IsOn(void)
{
    return s_on;
}
