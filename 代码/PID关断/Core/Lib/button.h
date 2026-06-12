//
// Created by lin on 2026/6/2.
//

#ifndef CI_SOURCE_FINISHED_BUTTON_H
#define CI_SOURCE_FINISHED_BUTTON_H
#include <stdint.h>
#include "stm32f1xx.h"
// 按键内部状态机枚举
typedef enum {
    BTN_STATE_IDLE = 0, // 空闲态
    BTN_STATE_DOWN,     // 确认按下消抖态
    BTN_STATE_UP        // 等待抬手释放态
} ButtonState_t;

// 按键事件回调函数指针定义
typedef void (*ButtonCallback_t)(void);

// 按键对象结构体（面向对象封装）
typedef struct {
    GPIO_TypeDef* GPIOx;            // 物理端口 (如 GPIOB)
    uint16_t           GPIO_Pin;    // 物理引脚 (如 KEY_1_Pin)
    ButtonState_t      state;       // 该按键独立的状态机位置
    uint32_t           down_tick;   // 独立的按下时间戳
    ButtonCallback_t   OnPress;     // 短按下触发的功能函数指针
    ButtonCallback_t   OnLongPress; // 长按事件处理
    ButtonCallback_t   OnRelease;   // 释放触发的功能函数指针（不需要可填NULL）
} Button_t;

// 声明全局按键枚举，用于索引
typedef enum {
    KEY_INDEX_1 = 0,
    KEY_INDEX_2,
    KEY_INDEX_3,
    ENC_SW,
    BTN_NUM
} ButtonIndex_t;

void Button_Init(void);
void Button_Process(void);
void Button_EXTI_Callback(uint16_t GPIO_Pin);
void Button_RegisterCallback(ButtonIndex_t index, ButtonCallback_t onPress, ButtonCallback_t onLongPress);
#endif //CI_SOURCE_FINISHED_BUTTON_H