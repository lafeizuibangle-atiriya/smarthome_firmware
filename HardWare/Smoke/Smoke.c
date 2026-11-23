#include "stm32l4xx.h"                  // Device header
#include "Smoke.h"

/* 烟感 MQ-2 = PD15，板载 LM393 比较器 + 电位器调阈值，绿色 LED 指示
 * 上电后 MQ-2 需预热 1~5 分钟，期间读数不稳属正常现象
 * 极性说明：LM393 开集输出 + 上拉，静态为高；检测到烟雾时被拉低
 * （若实测相反：干净时打印 Smoke=0、有烟时 Smoke=1 才是对的） */
void Smoke_Init(void)
{
    GPIO_InitTypeDef g = {0};
    g.Pin  = GPIO_PIN_15;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;   /* 内部上拉：模块不动这条线时读高电平，不误报 */
    HAL_GPIO_Init(GPIOD, &g);
}

uint8_t Smoke_Detected(void)
{
    return (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_15) == GPIO_PIN_RESET) ? 1 : 0;
}
