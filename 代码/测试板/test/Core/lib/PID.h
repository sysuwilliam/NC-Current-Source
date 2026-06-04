//
// Created by lin on 2026/5/31.
//

#ifndef TEST_PID_H
#define TEST_PID_H

#include "main.h"

// PID 参数与控制状态结构体
typedef struct {
    float Kp;               // 比例系数
    float Ki;               // 积分系数
    float Kd;               // 微分系数

    int32_t target;         // 控制目标值 (对应你的 I_target, 单位 mA)
    int32_t actual;         // 当前反馈真实值 (对应你的 I_actual, 单位 mA)

    int32_t err_curr;       // 当前采样周期的误差 E(n)
    int32_t err_next;       // 上一个采样周期的误差 E(n-1)
    int32_t err_last;       // 上上一个采样周期的误差 E(n-2)

    float out_delta;        // 本次 PID 计算出的控制量增量
} PID_Controller_t;

// 符号广播
extern PID_Controller_t pid_current;

// 函数声明
void BUCK_PID_Init(void);
void BUCK_PID_Current_Loop(void);

#endif //TEST_PID_H