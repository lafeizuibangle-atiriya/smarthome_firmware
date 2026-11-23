#ifndef __KEY_H
#define __KEY_H
#include "main.h"

/* 非阻塞按键扫描：主循环里反复调用
 * 每个键只在"松开 -> 按下"的那一帧返回 1，按住不放只触发一次 */
uint8_t Key1_Pressed(void);     /* PD5 */
uint8_t Key2_Pressed(void);     /* PD4 */
uint8_t Key3_Pressed(void);     /* PD3 */
uint8_t Key4_Pressed(void);     /* PD2 */

#endif
