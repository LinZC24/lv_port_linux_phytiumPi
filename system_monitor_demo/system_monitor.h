#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#ifdef __cplusplus
extern "C" {
#endif

// 这里声明供 main.c 调用的初始化函数
void system_monitor_init(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // SYSTEM_MONITOR_H