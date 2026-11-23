#ifndef __ESP8285_H
#define __ESP8285_H

#include "main.h"

/*
 * ESP8285 串口驱动。USART1（PA9/PA10，115200）接模块。
 * WiFi 名称和密码由 main.c 里的 ESP_Init 传入。
 */

void    ESP_Init(char *wifiname, char *wifipass);
uint8_t ESP_SendCmd(char *cmd, char *ack, uint16_t timeout);
int     ESP_SendPayload(const char *cmd, const char *payload, unsigned int len);
int     ESP_PublishRaw(const char *topic, const char *payload);
void    ESP_PollLog(void);
void    ESP_ClearAck(void);

/*
 * 云端命令落地的请求标志。
 * esp8285.c 只负责把命令解析出来置标志，真正开灯、开风扇、开闸
 * 放到 main.c 的主循环里执行，保证状态机（g_fan_on、g_gate_open 等）
 * 只有一处，按键和云端两条路都走同一套状态。
 */
extern volatile int8_t  cloud_light_req;   /* -1 无动作，0 关，1 开 */
extern volatile int8_t  cloud_motor_req;   /* -1 无动作，0 停，1 转 */
extern volatile int8_t  cloud_servo_req;   /* -1 无动作，0 关闸，1 开闸 */
extern volatile int8_t  cloud_beep_req;    /* -1 无动作，0 停，1 响 */
extern volatile int8_t  cloud_mode_req;    /* -1 无动作，0 手动，1 自动 */
extern volatile uint8_t cloud_report_req;  /* 1 = 收到查询类命令，马上上报一次 */

extern volatile uint8_t hw_cmd_pending;
extern volatile uint8_t mqtt_link_down;
extern volatile uint8_t sntp_got;
extern char hw_cmd_buf[640];
extern char sntp_line[64];

/* 云端命令原文交给这里解析，返回 0 = 认出并已置标志，1 = 命令不认识 */
int HW_ApplyCommand(const char *json);

/* USART1 接收中断，向量在本文件里实现 */
void USART1_IRQHandler(void);

#endif
