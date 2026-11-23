#ifndef HMAC_SHA256_H
#define HMAC_SHA256_H

/*
 * 给华为云登录算密码，mqtt.c 会调用。
 * 密钥是 UTC 小时，例如 "2026110602"；被加密的内容是设备密钥。
 * 结果是 64 个小写十六进制字符，填进 MQTT 的 Password。
 * 自己不要改算法。换设备只改 mqtt.c 里的 HUAWEI_DEVICE_PWD。
 */
int hmac_self_test(void);
void hmac_sha256_hex(const char *key, const char *msg, char out[65]);

#endif
