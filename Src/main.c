/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "Beep.h"
#include "Light.h"
#include "Fan.h"
#include "Servo.h"
#include "Relay.h"
#include "Smoke.h"
#include "PIR.h"
#include "Key.h"
#include "Led.h"
#include "SHT30.h"
#include "BH1750.h"
#include "esp8285.h"
#include "mqtt.h"
#include "user_config.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* ---- 自动控制阈值（带回差，防临界抖动） ---- */
#define LUX_LIGHT_ON     10      /* 光照低于此值开补光灯 */
#define LUX_LIGHT_OFF    50      /* 高于此值关灯（与开灯值构成回差） */
#define TEMP_FAN_ON      30.0f   /* 温度高于此值风扇转 */
#define TEMP_FAN_OFF     28.0f   /* 低于此值风扇停（回差 2 度） */
#define TEMP_RELAY_ON    35.0f   /* 温度过高继电器吸合（二级保护） */
#define TEMP_RELAY_OFF   32.0f   /* 回差 3 度后释放 */
#define PIR_GATE_MS      5000    /* PIR 持续无人多久后落杆 */
#define PRINT_MS         1000    /* 串口状态打印周期 */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static uint8_t  g_auto = 1;        /* 1 = AUTO 自动模式，0 = MANUAL 手动 */
static uint8_t  g_fan_on = 0;      /* KEY2 手动风扇开关：1 = 开 */
static uint8_t  g_gate_open = 0;   /* 道闸当前状态（软件记账） */
static uint32_t g_pir_last = 0;    /* 最后一次检测到人的时刻 */
static uint32_t g_print_last = 0;
static uint8_t  g_beep_manual = 0;    /* 1 = 蜂鸣器被 KEY1 手动接管 */
static uint8_t  g_smoke_cnt = 0;      /* 烟感连续确认计数 */
static uint32_t g_next_conn = 0;      /* 下一次尝试登录华为云的时刻 */
static uint32_t g_last_cloud = 0;     /* 上一次云端上报的时刻 */
static int8_t   g_lux_zone = -1;   /* 光照区：-1 未定 / 0 暗 / 1 亮 / 2 中间带 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* 扫描 I2C 总线：把 0x08~0x77 每个地址问一遍，谁应答就打印谁。
 * 用来排查"传感器找不到"：有地址 = 接线通；一个都没有 = 线/供电有问题 */
static void I2C_Scan(I2C_HandleTypeDef *hi, const char *name)
{
    printf("%s scan:", name);
    for (uint8_t a = 0x08; a < 0x78; a++)
    {
        if (HAL_I2C_IsDeviceReady(hi, (uint16_t)(a << 1), 2, 20) == HAL_OK)
            printf(" 0x%02X", a);
    }
    printf(" done\r\n");
}
/* printf 重定向到 USART2（需勾选 MicroLIB） */
int fputc(int ch, FILE *f)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF);
    return ch;
}

/* 道闸统一入口：只在新旧状态不同时才动作，避免反复写 CCR */
static void Gate_Set(uint8_t open)
{
    if (open && !g_gate_open)  { Gate_Open();  g_gate_open = 1; g_pir_last = HAL_GetTick(); }
    if (!open && g_gate_open)  { Gate_Close(); g_gate_open = 0; }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_I2C2_Init();
  MX_USART2_UART_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  printf("\r\n===== Smart Home Demo (STM32L431) =====\r\n");
  Fan_Init();          /* 启动风扇 PWM，0% */
  Servo_Init();        /* 启动舵机 PWM，0 度关闸 */
  g_gate_open = 0;
  if (BH1750_Init() != 0)
  {
      printf("BH1750 not found, Lux disabled\r\n");
      I2C_Scan(&hi2c2, "I2C2");   /* 应该同时看到 0x44(SHT30) 和 0x5C(BH1750) */
  }
  Smoke_Init();        /* PD15 上拉输入，防浮空误报 */
  Led_All(0);
  Relay_Off();
  Beep_Off();
  Light_Off();

  /*
   * 连 WiFi 并登录华为云（IoTDA，上海一）。
   * WiFi 与华为云连接参数在 Inc/user_config.h（模板见 Inc/user_config_example.h）。
   * ESP_Init 里 WiFi 连接失败不会死等，MQTT 登录失败由主循环 15 秒后重试。
   */
  ESP_Init(WIFI_SSID, WIFI_PASSWORD);
  if (!MQTT_ConnectHuawei())
  {
      printf("retry Huawei in 15s\r\n");
      g_next_conn = HAL_GetTick() + 15000;
  }
  printf("init done. KEY4 = switch AUTO/MANUAL\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* ============ 0. 云端层：收日志、执行云端命令、掉线重连 ============ */
    ESP_PollLog();
    MQTT_PollCommand();
    if (cloud_light_req >= 0) { if (cloud_light_req) Light_On(); else Light_Off(); cloud_light_req = -1; }
    if (cloud_motor_req >= 0) { g_fan_on = (cloud_motor_req != 0); if (g_fan_on) Fan_On(); else Fan_Off(); cloud_motor_req = -1; }
    if (cloud_servo_req >= 0) { Gate_Set(cloud_servo_req ? 1 : 0); cloud_servo_req = -1; }
    if (cloud_beep_req  >= 0) { g_beep_manual = 1; if (cloud_beep_req) Beep_On(); else Beep_Off(); cloud_beep_req = -1; }
    if (cloud_mode_req  >= 0)
    {
        g_auto = (cloud_mode_req != 0);
        g_beep_manual = 0;
        if (!g_auto) { g_fan_on = 0; Fan_Off(); }
        printf("[MODE] -> %s\r\n", g_auto ? "AUTO" : "MANUAL");
        cloud_mode_req = -1;
    }
    if (!MQTT_IsConnected() && HAL_GetTick() >= g_next_conn)
    {
        if (!MQTT_ConnectHuawei())
        {
            printf("retry Huawei in 15s\r\n");
            g_next_conn = HAL_GetTick() + 15000;
        }
    }

    /* ============ 1. 按键层（手动控制，两种模式下都响应） ============ */
    if (Key4_Pressed())
    {
        g_auto = !g_auto;
        g_beep_manual = 0;    /* 切模式时把手动接管还回去 */
        printf("[MODE] -> %s\r\n", g_auto ? "AUTO" : "MANUAL");
        if (!g_auto) { g_fan_on = 0; Fan_Off(); }   /* 进手动模式先把风扇停 */
    }
    if (Key1_Pressed())
    {
        g_beep_manual = 1;    /* 手动接管蜂鸣器，烟雾逻辑不再每圈覆盖 */
        Beep_Turn();
        printf("[KEY1] beep %s\r\n", Beep_IsOn() ? "ON" : "OFF");
    }
    if (Key2_Pressed())
    {
        g_fan_on = !g_fan_on;
        if (g_fan_on) Fan_On(); else Fan_Off();
        printf("[KEY2] fan %s\r\n", g_fan_on ? "ON" : "OFF");
    }
    if (Key3_Pressed())
    {
        Gate_Set(!g_gate_open);
        printf("[KEY3] gate %s\r\n", g_gate_open ? "OPEN" : "CLOSE");
    }

    /* ============ 2. 传感器采集 ============ */
    float t = 0.0f, h = 0.0f;
    uint16_t lux = 0;
    uint8_t ok_t = SHT30_Read(&t, &h);      /* 0 = 成功 */
    uint8_t ok_l = BH1750_Read(&lux);       /* 0 = 成功 */
    uint8_t smoke_raw = Smoke_Detected();
    uint8_t smoke = 0;
    uint8_t pir   = PIR_Detected();

    /* 烟感去抖：连续 2 圈（约 60ms）都报有气才算，滤掉 LM393 上的一次毛刺 */
    if (smoke_raw) { if (g_smoke_cnt < 3) g_smoke_cnt++; }
    else           { g_smoke_cnt = 0; }
    smoke = (g_smoke_cnt >= 2) ? 1 : 0;
    /* ============ 3. 烟雾联动（最高优先级，两种模式都生效） ============ */
    /* 烟感触发：蜂鸣器报警 + 风扇全速排风 + 道闸打开放行 */
    if (smoke)
    {
        g_beep_manual = 0;    /* 报警优先，清掉手动状态 */
        Beep_On();
        Fan_On();
    }
    else
    {
        if (!g_beep_manual) Beep_Off();   /* 手动开的就一直响 */
        if (!g_auto) { if (g_fan_on) Fan_On(); else Fan_Off(); }  /* 手动模式听 KEY2 的 */
    }

    /* ============ 4. 自动层（仅 AUTO 模式） ============ */
    if (g_auto)
    {
        /* 温度 -> 风扇调速（回差：>=30 开 60%，<=28 关） */
        if (ok_t == 0)
        {
            if (t >= TEMP_FAN_ON)       Fan_On();
            else if (t <= TEMP_FAN_OFF) Fan_Off();
            /* 28~30 之间：保持现状，防止临界点来回开关 */
        }
        /* 温度 -> 继电器（二级保护：>=35 吸合，<=32 释放） */
        if (ok_t == 0)
        {
            if (t >= TEMP_RELAY_ON)       Relay_On();
            else if (t <= TEMP_RELAY_OFF) Relay_Off();
        }
        /* 光照 -> 补光灯：只在"跨过阈值"的那一刻动作，之后不再每圈强写
         * 这样按 KEY1 手动开关后不会被自动规则立刻改回去 */
        if (ok_l == 0)
        {
            int8_t zone;
            if      (lux <= LUX_LIGHT_ON)  zone = 0;    /* 暗 */
            else if (lux >= LUX_LIGHT_OFF) zone = 1;    /* 亮 */
            else                           zone = 2;    /* 中间带：保持现状 */

            if (zone != g_lux_zone)
            {
                if      (zone == 0) Light_On();
                else if (zone == 1) Light_Off();
                g_lux_zone = zone;
            }
        }
        /* PIR -> 门禁道闸：有人抬杆，持续 5 秒无人落杆 */
        if (pir)
        {
            g_pir_last = HAL_GetTick();
            Gate_Set(1);
        }
        else if (g_gate_open && (HAL_GetTick() - g_pir_last > PIR_GATE_MS))
        {
            Gate_Set(0);
        }
    }

    /* ============ 5. LED 状态指示 ============ */
    if (g_auto) Led_Off(1); else Led_On(1);   /* LED1：亮 = MANUAL */
    if (smoke)  Led_On(2);  else Led_Off(2);  /* LED2：烟雾报警 */
    if (pir)    Led_On(3);  else Led_Off(3);  /* LED3：检测到人 */
    /* LED4 空闲，留给以后扩展 */

    /* ============ 6. 串口状态打印（每 1 秒一行） ============ */
    if (HAL_GetTick() - g_print_last >= PRINT_MS)
    {
        g_print_last = HAL_GetTick();
        if (ok_t == 0 && ok_l == 0)
            printf("T=%.1fC H=%.1f%% Lux=%4u Smoke=%d PIR=%d %s Fan=%s Gate=%s up=%lus\r\n",
                   t, h, lux, smoke, pir,
                   g_auto ? "AUTO" : "MANUAL",
                   Fan_IsOn() ? "ON" : "OFF",
                   g_gate_open ? "OPEN" : "CLOSE", (unsigned long)(HAL_GetTick() / 1000));
        else if (ok_t == 0)
            printf("T=%.1fC H=%.1f%% Lux=---- Smoke=%d PIR=%d %s Fan=%s Gate=%s up=%lus\r\n",
                   t, h, smoke, pir,
                   g_auto ? "AUTO" : "MANUAL",
                   Fan_IsOn() ? "ON" : "OFF",
                   g_gate_open ? "OPEN" : "CLOSE", (unsigned long)(HAL_GetTick() / 1000));
        else
            printf("sensor err t=%d lux=%d Smoke=%d PIR=%d %s\r\n",
                   ok_t, ok_l, smoke, pir, g_auto ? "AUTO" : "MANUAL");
    }

    /* ============ 7. 云端上报（每 3 秒一次，查询类命令会触发立刻上报） ============ */
    if (MQTT_IsConnected() && (HAL_GetTick() - g_last_cloud >= 500 || cloud_report_req))
    {
        Cloud_Report r;

        cloud_report_req = 0;
        r.temp = (ok_t == 0) ? t : 0.0f;
        r.humi = (ok_t == 0) ? h : 0.0f;
        r.lux  = (ok_l == 0) ? (int)lux : 0;
        r.light = Light_IsOn();
        r.motor = Fan_IsOn();
        r.servo = g_gate_open;
        r.beep = Beep_IsOn();
        r.auto_mode = g_auto;
        r.smoke = smoke;
        r.pir = pir;
        MQTT_ReportState(&r);
        g_last_cloud = HAL_GetTick();
    }

    HAL_Delay(10);   /* 主循环节拍约 30~40ms（SHT30 内部有 20ms 等待） */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 20;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2|RCC_PERIPHCLK_I2C2;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  PeriphClkInit.I2c2ClockSelection = RCC_I2C2CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure the main internal regulator output voltage 
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* 蜂鸣器哒哒响 = 初始化失败（最常见：HSE 时钟没起来）。死循环停在这里 */
  while (1)
  {
    HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_8);
    HAL_Delay(200);
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(char *file, uint32_t line)
{ 
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
