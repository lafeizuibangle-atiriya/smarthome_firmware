#ifndef __MQTT_H
#define __MQTT_H
#include "main.h"

/*
 * 板子连接华为云 IoTDA 的接口（MQTT，1883，上海一 cn-east-3）。
 * 手机 App 不连板子，只和华为云交互；命令沿板子这条 MQTT 链路送下来。
 *
 * 换设备、换账号只改 mqtt.c 顶部 4 个宏。
 */

/* 一次上报的完整状态。main.c 负责填，mqtt.c 负责拼 JSON 发出去 */
typedef struct {
    float   temp;       /* 温度，摄氏度 */
    float   humi;       /* 湿度，百分比 */
    int     lux;        /* 光照，lux */
    uint8_t light;      /* 1 = 补光灯亮 */
    uint8_t motor;      /* 1 = 风扇在转 */
    uint8_t servo;      /* 1 = 闸门打开 */
    uint8_t beep;       /* 1 = 蜂鸣器在响 */
    uint8_t auto_mode;  /* 1 = 自动模式 */
    uint8_t smoke;      /* 1 = 烟雾报警 */
    uint8_t pir;        /* 1 = 检测到人 */
} Cloud_Report;

/* 登录华为云。成功返回 1。调用前 WiFi 必须已经连上，见 main.c 的 ESP_Init */
int  MQTT_ConnectHuawei(void);

/* 1 表示 MQTT 还连着，0 表示掉线，main.c 会隔 15 秒重连 */
int  MQTT_IsConnected(void);

/*
 * 主循环里一直调用。
 * 云端有命令就解析并回复 result_code，动作由 main.c 按 cloud_*_req 标志执行。
 */
void MQTT_PollCommand(void);

/* 每 3 秒调用一次，把整份状态上报给华为云设备影子 */
void MQTT_ReportState(const Cloud_Report *r);

#endif
