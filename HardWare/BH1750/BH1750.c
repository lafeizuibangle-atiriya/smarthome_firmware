#include "BH1750.h"
#include "i2c.h"

/* BH1750 光照传感器，和 SHT30 共用 I2C2（PB13=SCL，PB14=SDA）
 * 注意两点（来自传感器板原理图，U5 = BH1750FVI）：
 * 1. DVI（内部逻辑电源脚）接在 PD12 上，必须先拉高 PD12 芯片才工作，
 *    否则怎么读都没有应答；
 * 2. ADDR 接了 3.3V，所以 7 位地址是 0x5C（0x23 是 ADDR 接地的用法，也留着探测）
 * 没有 CRC，比 SHT30 简单：发 1 字节指令，再读 2 字节原始值
 * 原始值 / 1.2 = 勒克斯（lux），芯片手册规定的换算
 * 高分辨率模式：Init 发一次 0x10，之后每 180ms 以上才能读一次 */

static uint8_t g_addr = 0;      /* 探测到的 8 位地址；0 = 未探测到 */

uint8_t BH1750_Init(void)
{
    uint8_t cmd = 0x10;             /* 高分辨率模式，1 lux 分辨率 */
    /* ADDR 接高 = 0x5C，ADDR 接地 = 0x23，HAL 用 8 位格式（末位是读写位） */
    const uint8_t probe[2] = { 0x5C << 1, 0x23 << 1 };

    /* DVI 使能：PD12 推挽输出高，给 BH1750 的逻辑部分供电 */
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_12;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOD, &g);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);
    HAL_Delay(10);                  /* 等芯片内部电源稳定 */

    for (int i = 0; i < 2; i++)
    {
        if (HAL_I2C_Master_Transmit(&hi2c2, probe[i], &cmd, 1, 100) == HAL_OK)
        {
            g_addr = probe[i];
            HAL_Delay(180);         /* 首次测量要等约 180ms 才出结果 */
            return 0;
        }
    }
    g_addr = 0;
    return 1;                       /* 两个地址都没应答：模块没接或接线不对 */
}

uint8_t BH1750_Read(uint16_t *lux)
{
    uint8_t buf[2];
    if (g_addr == 0) return 1;      /* 初始化时没找到，直接返回，不再占 I2C */
    if (HAL_I2C_Master_Receive(&hi2c2, g_addr, buf, 2, 100) != HAL_OK) return 2;
    *lux = ((uint16_t)buf[0] << 8) | buf[1];    /* 高字节在前，和 SHT30 一样 */
    *lux = (uint16_t)(*lux / 1.2f);
    return 0;
}
