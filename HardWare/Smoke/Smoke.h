#ifndef __SMOKE_H
#define __SMOKE_H
#include "main.h"

void    Smoke_Init(void);       /* 把 PD15 配成内部上拉输入，防浮空误报 */
uint8_t Smoke_Detected(void);   /* 返回 1 = 烟雾超标 */

#endif
