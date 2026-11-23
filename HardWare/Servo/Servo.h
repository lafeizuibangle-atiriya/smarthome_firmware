#ifndef __SERVO_H
#define __SERVO_H
#include "main.h"

/* 道闸两个舵机的对外接口。
 * 上层（main.c）只认"开闸 / 关闸"，用哪个定时器、哪个通道都封在 Servo.c 里。
 * 以后换引脚或者加第三个舵机，改 Servo.c 就够。 */

void Servo_Init(void);                 /* 启动两路 PWM，先关闸 */

void Servo_SetAngle1(uint8_t deg);     /* 舵机1（上杆，PA11），0~180 */
void Servo_SetAngle2(uint8_t deg);     /* 舵机2（下杆，PA15），0~180 */

void Gate_Open(void);                  /* 两根杆同时抬起 */
void Gate_Close(void);                 /* 两根杆收回 90 度 */

#endif
