#include "Led.h"

/* LED1~LED4 = PA4~PA7£¨µÕµÁ∆Ωµ„¡¡£®SET=√£¨RESET=¡¡£© */

static const uint16_t led_pin[4] = {GPIO_PIN_4, GPIO_PIN_5, GPIO_PIN_6, GPIO_PIN_7};

void Led_On(uint8_t n)
{
    if (n < 1 || n > 4) return;
    HAL_GPIO_WritePin(GPIOA, led_pin[n - 1], GPIO_PIN_RESET);
}

void Led_Off(uint8_t n)
{
    if (n < 1 || n > 4) return;
    HAL_GPIO_WritePin(GPIOA, led_pin[n - 1], GPIO_PIN_SET);
}

void Led_All(uint8_t on)
{
    uint16_t pins = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    HAL_GPIO_WritePin(GPIOA, pins, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}
