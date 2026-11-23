# smart_home_firmware

![STM32](https://img.shields.io/badge/MCU-STM32L431VCTx-075fba)
![Keil](https://img.shields.io/badge/IDE-Keil%20MDK--ARM-5b8f32)
![ESP8285](https://img.shields.io/badge/WiFi-ESP8285-orange)
![MQTT](https://img.shields.io/badge/Protocol-MQTT-blue)

智慧家居项目的STM32固件。芯片为 STM32L431VCTx，通过 ESP8285 WiFi 模块以 MQTT 接入华为云 IoTDA，与 HarmonyOS App、Spring Boot 后端配合使用。配套的 App 与后端见 `smarthome_project`。

## 功能

- 传感器：BH1750 光照、SHT30 温湿度、烟雾传感器、PIR 人体感应。
- 执行器：LED、风扇、继电器、舵机、蜂鸣器。
- 按键：本地手动控制；KEY4 切换自动 / 手动模式。
- 上报：周期性向 IoTDA 上报设备状态，云端下发命令控制外设。
- 云侧主题：`$oc/devices/{设备ID}/sys/commands/#`、`$oc/devices/{设备ID}/sys/messages/down`、`$oc/devices/{设备ID}/sys/properties/report`。
- 命令名：`Light_control`、`Motor_control`、`Servo_control`、`Beep_control`、`Mode_control`、`Smoke_control`、`PIR_control`，参数需与华为云产品模型一致。

## 连接参数配置

WiFi 账号、华为云设备 ID、设备密钥和接入域名都不放在仓库里。首次使用按下面步骤操作：

1. 复制 `Inc/user_config_example.h` 为同目录下的 `Inc/user_config.h`。
2. 在 `Inc/user_config.h` 里填入自己的 WiFi 名称与密码、华为云设备 ID、设备密钥、接入域名。`HUAWEI_MQTT_PORT` 默认为 1883。
3. `Inc/user_config.h` 已被 `.gitignore` 排除，不会提交。换 WiFi 或换设备时只改这个文件。

`Utils/mqtt.c` 和 `Src/main.c` 都引用这些宏，不需要再改源码。

## 编译与烧录

1. 用 Keil MDK-ARM 打开 `MDK-ARM/Smart_Home_Demo.uvprojx`。
2. 确认已配置 `Inc/user_config.h`，否则编译会提示缺少宏定义。
3. 编译后通过 ST-Link 或其他调试器下载到板子。串口 1（PA9/PA10，115200）连接 ESP8285，串口 2 用于日志输出。

外设初始化由 STM32CubeMX 生成，配置文件是 `Smart_Home_Demo.ioc`；修改引脚后需要用 CubeMX 重新生成并保留 `Src/` 与 `Inc/` 下的用户代码。

## 目录

```text
Smart_Home_Demo/
├─ Smart_Home_Demo.ioc    CubeMX 配置
├─ Src/                   主循环、GPIO / I2C / TIM / USART 初始化与中断
├─ Inc/                   头文件；user_config.h 为本地私有配置
├─ HardWare/              传感器与执行器驱动，每个外设一个目录
├─ Utils/                 ESP8285 AT 指令、MQTT 客户端、HMAC-SHA256
├─ Drivers/               STM32L4 HAL 库与 CMSIS
└─ MDK-ARM/               Keil 工程与构建输出（输出目录已忽略）
```

## 说明

这是课程项目的固件部分。设备密钥仅存放在本地配置文件中，推送前请确认 `Inc/user_config.h` 没有被加入版本控制。
