#include "stm32l4xx.h"                  // Device header
#include "PIR.h"

/* 人体红外 HC-SR501 = PC8，OUT 经 100R 进 MCU
 * 检测到人输出高电平（3.3V），无触发为低
 * 注意：HC-SR501 上电需约 1 分钟稳定，期间可能误触发 */
uint8_t PIR_Detected(void)
{
    return (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_8) == GPIO_PIN_SET) ? 1 : 0;
}
