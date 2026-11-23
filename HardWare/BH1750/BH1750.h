#ifndef __BH1750_H
#define __BH1750_H
#include "main.h"

uint8_t BH1750_Init(void);              /* 发一次连续测量指令，成功返回 0 */
uint8_t BH1750_Read(uint16_t *lux);     /* 读光照值（勒克斯），成功返回 0 */

#endif
