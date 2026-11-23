#include "stm32l4xx.h"                  // Device header
#include "SHT30.h"
#include "i2c.h"

/* SHT30 温湿度，挂在 I2C2（PB13/PB14），ADDR 接地 -> 7 位地址 0x44
 * 流程：发 2 字节测量命令 -> 等 15ms 以上 -> 读 6 字节 -> CRC 校验 -> 换算 */

#define SHT30_ADDR (0x44 << 1)    /* 0x88：HAL 要 8 位格式（7 位地址左移 1 位） */

static uint8_t crc8(const uint8_t *d)      /* SHT30 的 CRC：多项式 0x31，初值 0xFF */
{
    uint8_t crc = 0xFF;
    for (int i = 0; i < 2; i++) {
        crc ^= d[i];
        for (int b = 0; b < 8; b++)
            crc = (crc & 0x80) ? (uint8_t)(crc << 1) ^ 0x31 : (uint8_t)(crc << 1);
    }
    return crc;
}

uint8_t SHT30_Read(float *temp, float *hum)
{
    uint8_t cmd[2] = {0x2C, 0x06};   /* 单次测量、高重复性 */
    uint8_t buf[6];

    if (HAL_I2C_Master_Transmit(&hi2c2, SHT30_ADDR, cmd, 2, 100) != HAL_OK) return 1;
    HAL_Delay(20);                    /* 高重复性测量最长 15ms，留裕量 */
    if (HAL_I2C_Master_Receive(&hi2c2, SHT30_ADDR, buf, 6, 100) != HAL_OK) return 2;

    if (crc8(buf) != buf[2] || crc8(buf + 3) != buf[5]) return 3;   /* 校验失败 */

    uint16_t raw_t = (buf[0] << 8) | buf[1];   /* 高字节在前 */
    uint16_t raw_h = (buf[3] << 8) | buf[4];
    *temp = -45.0f + 175.0f * raw_t / 65535.0f;
    *hum  = 100.0f * raw_h / 65535.0f;
    return 0;
}
