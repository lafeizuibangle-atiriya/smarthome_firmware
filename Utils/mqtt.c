#include "mqtt.h"
#include "esp8285.h"
#include "hmac_sha256.h"
#include <stdio.h>
#include <string.h>

/*
 * 华为云 IoTDA 设备接入。标准版、明文 MQTT，端口 1883。
 * 接入点：上海一 cn-east-3，实例地址见 user_config.h。
 *
 * 登录不是普通的用户名加密码，规则固定为：
 *   ClientId = 设备ID_0_0_UTC小时
 *   用户名   = 设备ID
 *   密码     = HMAC-SHA256(密钥=UTC小时, 内容=设备密钥) 的 64 位小写十六进制
 * UTC小时写成 2026110611，表示 2026-11-06 11 点（世界时，不是北京时间）。
 * 所以连接前要先让 ESP8285 对一次网。
 *
 * 用到的主题：
 *   上报数据  $oc/devices/设备ID/sys/properties/report
 *   订阅命令  $oc/devices/设备ID/sys/commands/#
 *   回复命令  $oc/devices/设备ID/sys/commands/response/request_id=命令编号
 *   订阅消息  $oc/devices/设备ID/sys/messages/down
 * 命令名和参数必须和华为云产品模型一致：
 *   Light_control(Light) Motor_control(Motor) Servo_control(Serve)
 *   Beep_control(Beep) Mode_control(Mode) Smoke_control PIR_control
 */

/* 连接参数集中在 Inc/user_config.h（不入库），模板见 Inc/user_config_example.h */
#include "user_config.h"

/* 1 表示已经登录华为云 */
static int g_mqtt_ok = 0;
/* 1 表示 ESP 支持 AT+MQTTPUBRAW，长报文用它发。不支持就改用 AT+MQTTPUB */
static int g_raw_pub = 1;

/* 把 ESP 对时结果里的英文月份换成数字。Jan 是 1 月 */
static int month_number(const char *text)
{
  static const char names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  int i;
  for (i = 0; i < 12; i++) {
    if (text[0] == names[i * 3] && text[1] == names[i * 3 + 1] && text[2] == names[i * 3 + 2]) {
      return i + 1;
    }
  }
  return 0;
}

/*
 * 把 +CIPSNTPTIME:Thu Nov 06 03:56:12 2026
 * 收成华为云要的 2026110603。小时必须是 UTC。
 * 年份太早（还是 1970）说明还没对上时，返回 0。
 */
static int parse_stamp(const char *line, char out[12])
{
  const char *p = strstr(line, "+CIPSNTPTIME:");
  int month;
  int day = 0;
  int hour;
  int year = 0;

  if (p == 0) {
    return 0;
  }
  p += 13;
  while (*p == ' ') {
    p++;
  }
  while (*p != '\0' && *p != ' ') {
    p++;
  }
  while (*p == ' ') {
    p++;
  }
  month = month_number(p);
  if (month == 0) {
    return 0;
  }
  p += 3;
  while (*p == ' ') {
    p++;
  }
  while (*p >= '0' && *p <= '9') {
    day = day * 10 + (*p - '0');
    p++;
  }
  while (*p == ' ') {
    p++;
  }
  if (p[0] < '0' || p[0] > '9' || p[1] < '0' || p[1] > '9') {
    return 0;
  }
  hour = (p[0] - '0') * 10 + (p[1] - '0');
  while (*p != '\0' && *p != ' ') {
    p++;
  }
  while (*p == ' ') {
    p++;
  }
  while (*p >= '0' && *p <= '9') {
    year = year * 10 + (*p - '0');
    p++;
  }
  if (year < 2024 || day < 1 || day > 31 || hour > 23) {
    return 0;
  }
  sprintf(out, "%04d%02d%02d%02d", year, month, day, hour);
  return 1;
}

/*
 * 从云端下来的那一行里取出 request_id。
 * 回复命令时必须带回同一个编号，平台才认为这条命令执行完了。
 */
static int copy_request_id(const char *line, char *out, int max)
{
  const char *p = strstr(line, "request_id=");
  int i = 0;

  if (p != 0) {
    p += 11;
  } else {
    p = strstr(line, "\"request_id\"");
    if (p == 0) {
      return 0;
    }
    p = strchr(p, ':');
    if (p == 0) {
      return 0;
    }
    p++;
    while (*p == ' ' || *p == '"') {
      p++;
    }
  }
  while (*p != '\0' && *p != '"' && *p != '\r' && *p != '\n' && *p != ',' && *p != '}' && i < max - 1) {
    out[i++] = *p++;
  }
  out[i] = '\0';
  return i > 0;
}

/*
 * 用 AT+MQTTPUB 发一条报文。
 * ESP 的这句 AT 要求引号、逗号、反斜杠前面再加一个反斜杠。
 * 整行大约不能超过 250 字节，所以只适合较短的报文。
 */
static int publish_escaped(const char *topic, const char *json)
{
  static char cmd[420];
  static char esc[280];
  int i;
  int j = 0;
  int n;

  for (i = 0; json[i] != '\0' && j < (int)sizeof(esc) - 3; i++) {
    if (json[i] == '"' || json[i] == ',' || json[i] == '\\') {
      esc[j++] = '\\';
    }
    esc[j++] = json[i];
  }
  esc[j] = '\0';
  n = sprintf(cmd, "AT+MQTTPUB=0,\"%s\",\"%s\",0,0\r\n", topic, esc);
  if (n <= 0 || n > 250) {
    printf("[FAIL] MQTT pub length %d\r\n", n);
    return 0;
  }
  return ESP_SendCmd(cmd, "OK", 3000);
}

/*
 * 先试 AT+MQTTPUBRAW，报文可以更长，能带上全部状态。
 * 模块回 ERROR 或不支持时，记住以后改用上面的 AT+MQTTPUB。
 */
static int cloud_publish(const char *topic, const char *json)
{
  int ret;

  if (g_raw_pub) {
    ret = ESP_PublishRaw(topic, json);
    if (ret == 1) {
      return 1;
    }
    g_raw_pub = 0;
  }
  return publish_escaped(topic, json);
}

/* 拿到时间后清掉标志，避免迟到的 OK 被下一条 AT 当成自己的应答 */
static void accept_stamp(const char *stamp)
{
  HAL_Delay(200);
  ESP_ClearAck();
  printf("Huawei UTC %s\r\n", stamp);
}

/*
 * 时区参数 0 表示 UTC。华为云核对密码时用的就是这个小时。
 * 模块经常 3 秒左右才回 +CIPSNTPTIME，旧的 2 秒超时会先返回，
 * 下一轮又把已经到达的时间清掉，所以这里等够，并且超时后仍认这行时间。
 */
static int read_utc_stamp(char stamp[12])
{
  int i;

  ESP_SendCmd("AT+CIPSNTPCFG=1,0,\"ntp.aliyun.com\"\r\n", "OK", 8000);
  for (i = 0; i < 6; i++) {
    uint32_t t0;

    HAL_NVIC_DisableIRQ(USART1_IRQn);
    sntp_got = 0;
    sntp_line[0] = '\0';
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    ESP_SendCmd("AT+CIPSNTPTIME?\r\n", "OK", 5000);
    t0 = HAL_GetTick();
    while (!sntp_got && (HAL_GetTick() - t0) < 2000) {
      ESP_PollLog();
      HAL_Delay(50);
    }
    if (sntp_got && parse_stamp(sntp_line, stamp)) {
      accept_stamp(stamp);
      return 1;
    }
    HAL_Delay(500);
  }
  printf("SNTP time failed\r\n");
  return 0;
}

int MQTT_IsConnected(void)
{
  return g_mqtt_ok;
}

/* 安信可 2.2.0 不认识 MQTTLONG*。客户端 ID、用户名、密码各用一条短指令 */
static int mqtt_set_field(const char *at_name, const char *value)
{
  char cmd[160];
  int n;

  n = sprintf(cmd, "%s=0,\"%s\"\r\n", at_name, value);
  if (n <= 0 || n >= (int)sizeof(cmd)) {
    printf("%s too long\r\n", at_name);
    return 0;
  }
  if (!ESP_SendCmd(cmd, "OK", 3000)) {
    printf("%s failed\r\n", at_name);
    return 0;
  }
  return 1;
}

/* 对时、算密码、连接、订阅命令和消息。任一步失败返回 0 */
int MQTT_ConnectHuawei(void)
{
  static char stamp[12];
  static char client_id[80];
  static char password[68];
  static char cmd[300];
  static char topic[140];

  g_mqtt_ok = 0;
  if (!hmac_self_test()) {
    return 0;
  }
  if (!read_utc_stamp(stamp)) {
    return 0;
  }
  /* 华为云规定：时间戳是密钥，设备密钥是被加密的内容。两个参数反了平台会直接断开 */
  hmac_sha256_hex(stamp, HUAWEI_DEVICE_PWD, password);
  sprintf(client_id, "%s_0_0_%s", HUAWEI_DEVICE_ID, stamp);

  ESP_SendCmd("AT+MQTTCLEAN=0\r\n", "OK", 1000);
  /*
   * 三条凭证拼成一条 AT 有 190+ 字节，这块安信可固件收不完。
   * 它也没有 MQTTLONG* 指令，所以先写短配置，再分成三条旧指令。
   */
  if (!ESP_SendCmd("AT+MQTTUSERCFG=0,1,\"c\",\"u\",\"p\",0,0,\"\"\r\n", "OK", 10000)) {
    printf("MQTT user config failed\r\n");
    return 0;
  }
  if (!mqtt_set_field("AT+MQTTCLIENTID", client_id) ||
      !mqtt_set_field("AT+MQTTUSERNAME", HUAWEI_DEVICE_ID) ||
      !mqtt_set_field("AT+MQTTPASSWORD", password)) {
    return 0;
  }
  ESP_SendCmd("AT+MQTTCONNCFG=0,120,0,\"\",\"\",0,0\r\n", "OK", 1000);
  sprintf(cmd, "AT+MQTTCONN=0,\"%s\",%d,0\r\n", HUAWEI_MQTT_HOST, HUAWEI_MQTT_PORT);
  if (!ESP_SendCmd(cmd, "OK", 15000)) {
    printf("Huawei MQTT connect failed\r\n");
    return 0;
  }

  /* 订阅云端命令。灯、风扇、舵机、蜂鸣器都从这里下来 */
  sprintf(topic, "$oc/devices/%s/sys/commands/#", HUAWEI_DEVICE_ID);
  sprintf(cmd, "AT+MQTTSUB=0,\"%s\",1\r\n", topic);
  if (!ESP_SendCmd(cmd, "OK", 3000)) {
    printf("subscribe commands failed\r\n");
    return 0;
  }

  /* 订阅普通消息。smart_auto、smart_hand 走这里 */
  sprintf(topic, "$oc/devices/%s/sys/messages/down", HUAWEI_DEVICE_ID);
  sprintf(cmd, "AT+MQTTSUB=0,\"%s\",1\r\n", topic);
  ESP_SendCmd(cmd, "OK", 3000);

  g_mqtt_ok = 1;
  printf("Huawei MQTT connected\r\n");
  return 1;
}

/*
 * 处理一条已经收到的云端命令。
 * 串口中断里只负责把原文拷进 hw_cmd_buf，解析和回复放到这里，
 * 避免在中断里再发 AT 指令。
 * result_code：0 表示认出了命令并已置标志，1 表示命令名或参数不对。
 */
void MQTT_PollCommand(void)
{
  static char local[640];
  static char topic[180];
  static char req[48];
  static char body[32];
  int code;

  if (mqtt_link_down) {
    mqtt_link_down = 0;
    g_mqtt_ok = 0;
  }
  if (!hw_cmd_pending) {
    return;
  }

  __disable_irq();
  memcpy(local, hw_cmd_buf, sizeof(local));
  hw_cmd_pending = 0;
  __enable_irq();
  local[sizeof(local) - 1] = '\0';

  code = HW_ApplyCommand(local);
  if (!copy_request_id(local, req, (int)sizeof(req))) {
    return;
  }
  sprintf(topic, "$oc/devices/%s/sys/commands/response/request_id=%s", HUAWEI_DEVICE_ID, req);
  sprintf(body, "{\"result_code\":%d}", code);
  if (!cloud_publish(topic, body)) {
    g_mqtt_ok = 0;
  }
}

/*
 * 每 3 秒由 main.c 调用一次。
 * JSON 里的 service_id、属性名必须和华为云产品模型一字不差：
 * 服务 smart_home，属性 Temperature、Humidity、Lux 是数值，
 * LightStatus、MotorStatus、ServoStatus、BeepStatus、ModeStatus、
 * SmokeStatus、PIR_Status 是字符串。
 */
void MQTT_ReportState(const Cloud_Report *r)
{
  static char topic[120];
  static char json[400];

  if (!g_mqtt_ok) {
    return;
  }
  sprintf(topic, "$oc/devices/%s/sys/properties/report", HUAWEI_DEVICE_ID);
  sprintf(json,
          "{\"services\":[{\"service_id\":\"smart_home\",\"properties\":{\"Temperature\":%.1f,\"Humidity\":%.1f,\"Lux\":%d,"
          "\"LightStatus\":\"%s\",\"MotorStatus\":\"%s\",\"ServoStatus\":\"%s\",\"BeepStatus\":\"%s\","
          "\"ModeStatus\":\"%s\",\"SmokeStatus\":\"%s\",\"PIR_Status\":\"%s\"}}]}",
          r->temp, r->humi, r->lux,
          r->light ? "ON" : "OFF",
          r->motor ? "ON" : "OFF",
          r->servo ? "ON" : "OFF",
          r->beep ? "ON" : "OFF",
          r->auto_mode ? "AUTO" : "MANUAL",
          r->smoke ? "ON" : "OFF",
          r->pir ? "ON" : "OFF");
  if (g_raw_pub) {
    int ret = ESP_PublishRaw(topic, json);
    if (ret == 1) {
      return;
    }
    if (ret < 0) {
      g_raw_pub = 0;
    } else {
      return;
    }
  }
  publish_escaped(topic, json);
}
