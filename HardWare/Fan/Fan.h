#ifndef __FAN_H
#define __FAN_H
#include "main.h"

void    Fan_Init(void);      /* 启动 PWM 通道，默认停转 */
void    Fan_On(void);        /* 全速转 */
void    Fan_Off(void);       /* 停 */
uint8_t Fan_IsOn(void);      /* 1 = 正在转 */

#endif
