#ifndef __LIGHT_H
#define __LIGHT_H
#include "main.h"

void    Light_On(void);         /* 点亮 */
void    Light_Off(void);        /* 熄灭 */
void    Light_Turn(void);       /* 按当前状态翻转 */
uint8_t Light_IsOn(void);       /* 返回 1 = 当前是亮 */

#endif
