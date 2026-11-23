#ifndef __BEEP_H
#define __BEEP_H
#include "main.h"

void    Beep_On(void);          /* 响 */
void    Beep_Off(void);         /* 停 */
void    Beep_Turn(void);        /* 翻转当前状态 */
uint8_t Beep_IsOn(void);        /* 返回 1 = 正在响 */

#endif
