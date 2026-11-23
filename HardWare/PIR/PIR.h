#ifndef __PIR_H
#define __PIR_H
#include "main.h"

void    PIR_Init(void);       /* PC8 输入上拉，防悬空乱触发 */
uint8_t PIR_Detected(void);   /* 返回 1 = 检测到人 */     /* 返回 1 = 检测到人 */

#endif
