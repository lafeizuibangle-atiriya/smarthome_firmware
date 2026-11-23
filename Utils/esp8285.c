/*
 * ESP8285 串口驱动（USART1，PA9/PA10，115200）。
 * 结构与 samrtHome 例程一致：接收中断只暂存应答，主循环 ESP_PollLog 再打印。
 * 云端命令在这里只解析成 cloud_*_req 标志，具体动作由 main.c 执行。
 */
#include "esp8285.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>

uint8_t uart1_rx_byte;
uint8_t esp_rx_buffer[640];
uint16_t esp_rx_index = 0;
volatile uint8_t esp_rx_flag = 0;
volatile uint8_t esp_err_flag = 0;
volatile uint8_t esp_prompt = 0;

/* 云端命令请求标志，含义见 esp8285.h */
volatile int8_t  cloud_light_req = -1;
volatile int8_t  cloud_motor_req = -1;
volatile int8_t  cloud_servo_req = -1;
volatile int8_t  cloud_beep_req  = -1;
volatile int8_t  cloud_mode_req  = -1;
volatile uint8_t cloud_report_req = 0;
volatile uint8_t hw_cmd_pending = 0;
char hw_cmd_buf[640];
volatile uint8_t sntp_got = 0;
char sntp_line[64];
volatile uint8_t mqtt_link_down = 0;

/*
 * 模块应答不能在串口中断里 printf。
 * printf 走 USART2，一行要几毫秒，这期间 USART1 收不到下一个字节，OK 会被冲掉。
 * 中断只把整行放进队列，主循环再打印。
 */
/* 设为 1 可看到模块逐条 AT 回显（排错用），0 = 只打印关键事件 */
#define ESP_DEBUG 0

#if ESP_DEBUG
#define ESP_LOG(...) printf(__VA_ARGS__)
#else
#define ESP_LOG(...)
#endif

#define ESP_LOG_LINES 8
#define ESP_LOG_LEN   220
static char esp_log_q[ESP_LOG_LINES][ESP_LOG_LEN];
static volatile uint8_t esp_log_w = 0;
static volatile uint8_t esp_log_r = 0;

static void esp_log_push(const char *s)
{
  uint8_t next = (uint8_t)((esp_log_w + 1) % ESP_LOG_LINES);

  if (next == esp_log_r) {
    return;
  }
  strncpy(esp_log_q[esp_log_w], s, ESP_LOG_LEN - 1);
  esp_log_q[esp_log_w][ESP_LOG_LEN - 1] = '\0';
  esp_log_w = next;
}

void ESP_PollLog(void)
{
  char local[ESP_LOG_LEN];

  while (esp_log_r != esp_log_w) {
    HAL_NVIC_DisableIRQ(USART1_IRQn);
    memcpy(local, esp_log_q[esp_log_r], ESP_LOG_LEN);
    esp_log_r = (uint8_t)((esp_log_r + 1) % ESP_LOG_LINES);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    ESP_LOG("ESP: %s", local);
  }
}

void ESP_ClearAck(void)
{
  HAL_NVIC_DisableIRQ(USART1_IRQn);
  esp_rx_flag = 0;
  esp_err_flag = 0;
  HAL_NVIC_EnableIRQ(USART1_IRQn);
}

static void log_cmd(const char *prefix, const char *cmd)
{
  ESP_LOG("%s %s\r\n", prefix, cmd);
}

/*
 * 不能用 HAL_UART_Transmit 给 ESP 发数据。
 * 它在整段发送期间占着串口锁，而接收中断每收到一个字节都要重新上锁。
 * 指令一长，模块的回显会在发送还没结束时到达，后面的 OK 就全部丢了。
 * 这里直接写发送寄存器，接收中断可以同时工作。
 */
static int esp_send(const uint8_t *data, uint16_t len)
{
  uint16_t i;
  uint32_t t0 = HAL_GetTick();

  for (i = 0; i < len; i++) {
    while ((huart1.Instance->ISR & USART_ISR_TXE) == 0) {
      if (HAL_GetTick() - t0 > 1000) {
        return 0;
      }
    }
    huart1.Instance->TDR = data[i];
  }
  while ((huart1.Instance->ISR & USART_ISR_TC) == 0) {
    if (HAL_GetTick() - t0 > 1000) {
      return 0;
    }
  }
  return 1;
}

uint8_t ESP_SendCmd(char *cmd, char *ack, uint16_t timeout)
{
  uint32_t tickstart;
  size_t len = strlen(cmd);

  (void)ack;
  HAL_NVIC_DisableIRQ(USART1_IRQn);
  memset(esp_rx_buffer, 0, sizeof(esp_rx_buffer));
  esp_rx_index = 0;
  esp_rx_flag = 0;
  esp_err_flag = 0;
  HAL_NVIC_EnableIRQ(USART1_IRQn);

  HAL_UART_Receive_IT(&huart1, &uart1_rx_byte, 1);
  if (!esp_send((const uint8_t *)cmd, (uint16_t)len)) {
    printf("[FAIL] uart tx\r\n");
    return 0;
  }
  /* 调用方有的已经带了 \r\n。再补一次会变成空指令，模块回 ERROR，把真正的 OK 盖掉 */
  if (len == 0 || cmd[len - 1] != '\n') {
    esp_send((const uint8_t *)"\r\n", 2);
  }

  tickstart = HAL_GetTick();
  while (!esp_rx_flag) {
    ESP_PollLog();
    if (HAL_GetTick() - tickstart > timeout) {
      ESP_PollLog();
      log_cmd("[TIMEOUT]", cmd);
      return 0;
    }
  }
  ESP_PollLog();
  if (esp_err_flag) {
    log_cmd("[FAIL]", cmd);
    return 0;
  }
  log_cmd("[OK]", cmd);
  return 1;
}

int ESP_SendPayload(const char *cmd, const char *payload, unsigned int len)
{
  uint32_t tickstart;

  esp_prompt = 0;
  esp_err_flag = 0;
  esp_rx_flag = 0;
  HAL_UART_Receive_IT(&huart1, &uart1_rx_byte, 1);
  if (!esp_send((const uint8_t *)cmd, (uint16_t)strlen(cmd))) {
    printf("[FAIL] uart tx\r\n");
    return 0;
  }

  tickstart = HAL_GetTick();
  while (!esp_prompt && !esp_err_flag) {
    ESP_PollLog();
    if (HAL_GetTick() - tickstart > 5000) {
      ESP_PollLog();
      printf("[TIMEOUT] payload prompt\r\n");
      return 0;
    }
  }
  if (esp_err_flag) {
    ESP_PollLog();
    printf("[FAIL] payload prompt\r\n");
    return -1;
  }

  /* 前面的 OK 不能当成数据已经发完 */
  esp_rx_flag = 0;
  esp_err_flag = 0;
  if (!esp_send((const uint8_t *)payload, (uint16_t)len)) {
    printf("[FAIL] uart tx\r\n");
    return 0;
  }
  tickstart = HAL_GetTick();
  while (!esp_rx_flag) {
    ESP_PollLog();
    if (HAL_GetTick() - tickstart > 5000) {
      ESP_PollLog();
      printf("[TIMEOUT] payload ok\r\n");
      return 0;
    }
  }
  ESP_PollLog();
  if (esp_err_flag) {
    printf("[FAIL] payload data\r\n");
    return 0;
  }
  return 1;
}

int ESP_PublishRaw(const char *topic, const char *payload)
{
  static char cmd[220];
  unsigned int len = (unsigned int)strlen(payload);
  int ret;

  sprintf(cmd, "AT+MQTTPUBRAW=0,\"%s\",%u,0,0\r\n", topic, len);
  ret = ESP_SendPayload(cmd, payload, len);
  if (ret == 1) {
    ESP_LOG("[OK] PUBRAW\r\n");
  }
  return ret;
}

void ESP_Init(char *wifiname, char *wifipass)
{
  char cmd[128];

  /* 第一步不通就说明模块没接线或没供电，后面的 AT 全都会超时 */
  if (!ESP_SendCmd("AT", "OK", 1000)) {
    printf("[ESP] no AT response - check PA9/PA10 wiring\r\n");
    return;
  }
  printf("[ESP] AT ok\r\n");

  ESP_SendCmd("AT+RST", "ready", 1000);
  HAL_Delay(1500);                 /* 模块复位要等它把启动信息吐完 */
  ESP_SendCmd("AT+CWMODE=1", "OK", 1000);

  sprintf(cmd, "AT+CWJAP=\"%s\",\"%s\"\r\n", wifiname, wifipass);
  if (!ESP_SendCmd(cmd, "WIFI GOT IP", 15000)) {
    printf("[ESP] wifi join failed - check SSID/password\r\n");
    return;
  }
  printf("[ESP] wifi connected\r\n");
}

/* 在 key 后面 20 个字符里找 ON 或 OFF。找不到返回 -1，兼容 1/0 */
/* 大小写不敏感的子串查找：找到返回 1，没有返回 0。
 * 云端下发的枚举值大小写不固定（AUTO / Auto / auto 都见过），
 * strstr 只认一种写法，换个写法就解析失败 */
static int ci_contains(const char *text, const char *word)
{
  const char *h;
  const char *w;

  for (; *text != '\0'; text++) {
    h = text;
    w = word;
    while (*w != '\0' && *h != '\0') {
      char a = *h;
      char b = *w;
      if (a >= 'a' && a <= 'z') {
        a = (char)(a - 32);
      }
      if (b >= 'a' && b <= 'z') {
        b = (char)(b - 32);
      }
      if (a != b) {
        break;
      }
      h++;
      w++;
    }
    if (*w == '\0') {
      return 1;
    }
  }
  return 0;
}


static int para_on(const char *json, const char *key)
{
  const char *p = strstr(json, key);
  const char *end;

  if (p == 0) {
    return -1;
  }
  end = p + 20;
  while (p < end && *p != '\0') {
    if (p[0] == 'O' && p[1] == 'F' && p[2] == 'F') {
      return 0;
    }
    if (p[0] == 'O' && p[1] == 'N') {
      return 1;
    }
    if (p[0] == '1' && (p[1] == ',' || p[1] == '}' || p[1] == ' ')) {
      return 1;
    }
    if (p[0] == '0' && (p[1] == ',' || p[1] == '}' || p[1] == ' ')) {
      return 0;
    }
    /* 兼容小写 on / off */
    if (p[0] == 'o' && p[1] == 'n' && (p[2] == '"' || p[2] == ',' || p[2] == '}' || p[2] == ' ')) {
      return 1;
    }
    if (p[0] == 'o' && p[1] == 'f' && p[2] == 'f') {
      return 0;
    }
    p++;
  }
  return -1;
}

/*
 * 云端命令原文类似：
 *   {"command_name":"Light_control","service_id":"smart_home","paras":{"Light":"ON"}}
 * 这里只把意图记到 cloud_*_req 标志，开灯开闸等动作由 main.c 主循环执行。
 * Smoke_control、PIR_control 是查询类命令，收到就把最新状态立刻上报一次。
 * 返回 0 交给 mqtt.c 回复成功，返回 1 回复失败。
 */
int HW_ApplyCommand(const char *json)
{
  int on;

  if (strstr(json, "Light_control") != 0) {
    on = para_on(json, "\"Light\"");
    if (on < 0) {
      return 1;
    }
    cloud_light_req = (int8_t)on;
    printf("cloud Light %s\r\n", on ? "ON" : "OFF");
    return 0;
  }
  if (strstr(json, "Motor_control") != 0) {
    on = para_on(json, "\"Motor\"");
    if (on < 0) {
      return 1;
    }
    cloud_motor_req = (int8_t)on;
    printf("cloud Motor %s\r\n", on ? "ON" : "OFF");
    return 0;
  }
  if (strstr(json, "Servo_control") != 0) {
    on = para_on(json, "\"Servo\"");
    if (on < 0) {
      return 1;
    }
    cloud_servo_req = (int8_t)on;
    printf("cloud Servo %s\r\n", on ? "OPEN" : "CLOSE");
    return 0;
  }
  if (strstr(json, "Beep_control") != 0) {
    on = para_on(json, "\"Beep\"");
    if (on < 0) {
      return 1;
    }
    cloud_beep_req = (int8_t)on;
    printf("cloud Beep %s\r\n", on ? "ON" : "OFF");
    return 0;
  }
  if (ci_contains(json, "Mode_control")) {
    int m;

    /* 先看参数 Mode 的值：ON/OFF/1/0 都认（有的物模型把模式做成布尔） */
    m = para_on(json, "\"Mode\"");
    /* 再看关键字，大小写不敏感 */
    if (m < 0 && ci_contains(json, "AUTO")) {
      m = 1;
    }
    if (m < 0 && ci_contains(json, "MANUAL")) {
      m = 0;
    }
    if (m < 0 && ci_contains(json, "MANU")) {
      m = 0;
    }
    if (m < 0 && ci_contains(json, "HAND")) {
      m = 0;
    }
    if (m < 0) {
      printf("cloud Mode unparsed\r\n");
      return 1;
    }
    cloud_mode_req = (int8_t)m;
    printf("cloud Mode %s\r\n", m ? "AUTO" : "MANUAL");
    return 0;
  }
  if (strstr(json, "Smoke_control") != 0 || strstr(json, "PIR_control") != 0) {
    cloud_report_req = 1;
    printf("cloud report now\r\n");
    return 0;
  }
  printf("cloud cmd not recognized\r\n");
  return 1;
}

/* USART1 接收中断：逐字节收，凑成一行后按内容分发 */
void USART1_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart1);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance != USART1) {
    return;
  }

  if (uart1_rx_byte == '>') {
    esp_prompt = 1;
  }

  if (esp_rx_index < sizeof(esp_rx_buffer) - 1) {
    esp_rx_buffer[esp_rx_index++] = uart1_rx_byte;
    esp_rx_buffer[esp_rx_index] = '\0';
  } else {
    esp_rx_index = 0;
    esp_rx_buffer[0] = '\0';
  }

  if (uart1_rx_byte == '\n') {
    /*
     * 一行结束。华为云下发的内容以 +MQTTSUBRECV 开头。
     * 带 command_name 的是设备命令，先存起来，主循环再执行。
     * smart_auto、smart_hand 是手动和自动消息，直接记到模式请求。
     */
    if (strstr((char *)esp_rx_buffer, "+MQTTSUBRECV") != 0) {
      esp_log_push("MQTT down\r\n");
    } else {
      esp_log_push((char *)esp_rx_buffer);
    }

    if (strstr((char *)esp_rx_buffer, "+CIPSNTPTIME:") != 0) {
      strncpy(sntp_line, (char *)esp_rx_buffer, sizeof(sntp_line) - 1);
      sntp_line[sizeof(sntp_line) - 1] = '\0';
      sntp_got = 1;
    }
    if (strstr((char *)esp_rx_buffer, "+MQTTDISCONNECTED") != 0) {
      mqtt_link_down = 1;
    }
    if (strstr((char *)esp_rx_buffer, "smart_auto") != 0) {
      cloud_mode_req = 1;
      esp_log_push("mode auto\r\n");
    } else if (strstr((char *)esp_rx_buffer, "smart_hand") != 0) {
      cloud_mode_req = 0;
      esp_log_push("mode hand\r\n");
    }
    if (hw_cmd_pending == 0 && strstr((char *)esp_rx_buffer, "command_name") != 0) {
      strncpy(hw_cmd_buf, (char *)esp_rx_buffer, sizeof(hw_cmd_buf) - 1);
      hw_cmd_buf[sizeof(hw_cmd_buf) - 1] = '\0';
      hw_cmd_pending = 1;
    }

    if (strstr((char *)esp_rx_buffer, "+MQTTSUBRECV") == 0) {
      if (strstr((char *)esp_rx_buffer, "ERROR") != 0) {
        esp_err_flag = 1;
        esp_rx_flag = 1;
      } else if (strstr((char *)esp_rx_buffer, "OK") != 0 ||
                 strstr((char *)esp_rx_buffer, "ready") != 0 ||
                 strstr((char *)esp_rx_buffer, "WIFI GOT IP") != 0) {
        esp_err_flag = 0;
        esp_rx_flag = 1;
      }
    }

    esp_rx_index = 0;
    memset(esp_rx_buffer, 0, sizeof(esp_rx_buffer));
  }

  HAL_UART_Receive_IT(&huart1, &uart1_rx_byte, 1);
}
