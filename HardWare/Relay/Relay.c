#include "stm32l4xx.h"                  // Device header
#include "Relay.h"

/* 继电器 RLY1 = PC7，经 S8050 驱动，高电平吸合 */
void Relay_On(void)
{
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET);
}

void Relay_Off(void)
{
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);
}
