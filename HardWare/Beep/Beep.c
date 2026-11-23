#include "stm32l4xx.h"                  // Device header
#include "Beep.h"

/* 蜂鸣器 = PE8，高电平响（有源蜂鸣器，给电就叫）
 * 记一个状态量，Beep_Turn 才知道当前是开还是关 */

static uint8_t s_on = 0;

void Beep_On(void)
{
    s_on = 1;
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
}

void Beep_Off(void)
{
    s_on = 0;
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8, GPIO_PIN_RESET);
}

void Beep_Turn(void)
{
    if (s_on) Beep_Off();
    else      Beep_On();
}

uint8_t Beep_IsOn(void)
{
    return s_on;
}
