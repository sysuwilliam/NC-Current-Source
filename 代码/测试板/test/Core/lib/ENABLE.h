//
// Created by lin on 2026/5/17.
//

#ifndef INC_2026_5_4_ENABLE_H
#define INC_2026_5_4_ENABLE_H
#include "main.h"
//==================按键状态=================//
#define KEY1_STATE_IDLE       0   // 空闲状态：等待中断触发
#define KEY1_STATE_DOWN       1   // 软件消抖确认按下
#define KEY1_STATE_UP         2   // 确认按键抬起，重新使能

#define KEY2_STATE_IDLE       3   // 空闲状态：等待中断触发
#define KEY2_STATE_DOWN       4   // 软件消抖确认按下
#define KEY2_STATE_UP         5   // 确认按键抬起，重新使能

#define SW_DAC_CHANNEL_1         0
#define SW_DAC_CHANNEL_2         1

#define DAC_VOL_MAX     3.30f   // 限制 BUCK 最大安全输出电压 (V)
#define DAC_VOL_MIN     0.00f   // 限制 BUCK 最小输出电压 (V)



void BUCK_Key1_Process(void);
void Safe_Off(void);
void Safe_Init (void);
void Encoder_Process(void);
void DAC_OUTPUT(void);
void ENC_SW(void);
void Key2_Process(void);
#endif //INC_2026_5_4_ENABLE_H