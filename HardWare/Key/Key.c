#include "stm32l4xx.h"                  // Device header
#include "Key.h"

/* 4 个按键：上拉输入，按下为低（GPIOD 5/4/3/2）
 * 边沿检测写法：static 变量记住上一帧电平，
 * 只在 "1 -> 0"（松开变按下）跳变的那一次返回 1
 * 另外每个键加 150ms 抑制窗口：机械抖动可能在几十毫秒内产生两个下降沿，
 * 不拦的话一次按键会被当成两次，翻转类动作（开灯）就会"看起来没反应" */

#define KEY_HOLDOFF_MS 150

uint8_t Key1_Pressed(void)
{
    static uint8_t  last = 1;       /* 1 = 松开 */
    static uint32_t t_last = 0;     /* 上次被接受的事件时刻 */
    uint8_t now = (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_5) == GPIO_PIN_RESET) ? 0 : 1;
    uint8_t evt = 0;

    if (last == 1 && now == 0 && (HAL_GetTick() - t_last) > KEY_HOLDOFF_MS)
    {
        evt = 1;
        t_last = HAL_GetTick();
    }
    last = now;
    return evt;
}

uint8_t Key2_Pressed(void)
{
    static uint8_t  last = 1;
    static uint32_t t_last = 0;
    uint8_t now = (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_4) == GPIO_PIN_RESET) ? 0 : 1;
    uint8_t evt = 0;

    if (last == 1 && now == 0 && (HAL_GetTick() - t_last) > KEY_HOLDOFF_MS)
    {
        evt = 1;
        t_last = HAL_GetTick();
    }
    last = now;
    return evt;
}

uint8_t Key3_Pressed(void)
{
    static uint8_t  last = 1;
    static uint32_t t_last = 0;
    uint8_t now = (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_3) == GPIO_PIN_RESET) ? 0 : 1;
    uint8_t evt = 0;

    if (last == 1 && now == 0 && (HAL_GetTick() - t_last) > KEY_HOLDOFF_MS)
    {
        evt = 1;
        t_last = HAL_GetTick();
    }
    last = now;
    return evt;
}

uint8_t Key4_Pressed(void)
{
    static uint8_t  last = 1;
    static uint32_t t_last = 0;
    uint8_t now = (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_2) == GPIO_PIN_RESET) ? 0 : 1;
    uint8_t evt = 0;

    if (last == 1 && now == 0 && (HAL_GetTick() - t_last) > KEY_HOLDOFF_MS)
    {
        evt = 1;
        t_last = HAL_GetTick();
    }
    last = now;
    return evt;
}
