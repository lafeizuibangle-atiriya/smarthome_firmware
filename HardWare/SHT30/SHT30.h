#ifndef __SHT30_H
#define __SHT30_H
#include "main.h"

uint8_t SHT30_Read(float *temp, float *hum);    /* 返回 0=成功 1/2/3=各阶段失败 */

#endif
