//
// Created by lin on 2026/5/17.
//

#include "ENABLE.h"
#include "DAC.h"
#include "tim.h"
#include "main.h"
#include <stdio.h>
#include <string.h>
#include "usart.h"

static volatile uint8_t key_state = KEY1_STATE_IDLE; //volatile修饰在中断里被改写、在主循环里被读取的全局变量
static volatile uint32_t KEY1_EXIT_DOWN_TIME = 0;
static volatile uint8_t key2_state = KEY2_STATE_IDLE; //volatile修饰在中断里被改写、在主循环里被读取的全局变量
static volatile uint32_t KEY2_EXIT_DOWN_TIME = 0;

static uint16_t last_encoder_cnt = 0;
float DAC1_Target_voltage = 1.67f;
float DAC2_Target_voltage = 0.2f;

float VOL_STEP = 0.01f;
uint8_t VOL_STEP_flag = 1;

uint8_t DAC_CHANNEL_FLAG = SW_DAC_CHANNEL_1;


void Safe_Init (void) {
    HAL_GPIO_WritePin(SW_IN_GPIO_Port, SW_IN_Pin,GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin,GPIO_PIN_SET);
    HAL_GPIO_WritePin(BUCK_EN_GPIO_Port, BUCK_EN_Pin,GPIO_PIN_RESET);

    DAC_Set_Voltage(DAC_CHANNEL_1, DAC_GAIN_1X,0.0f);
    DAC_Set_Voltage(DAC_CHANNEL_2, DAC_GAIN_1X,0.0f);
}

/**
 * @brief  计算时间
 */
void Delay_us_Block(uint32_t us)
{
    uint32_t count = us * 8; // 根据主频粗略估算
    while(count--);
}

/**
 * @brief  关断
 */
// void Safe_Off(void) {
//     DAC_Set_Voltage(DAC_CHANNEL_2,1, 0);
//     Delay_us_Block(20000);
//     HAL_GPIO_WritePin(SW_IN_GPIO_Port, SW_IN_Pin,GPIO_PIN_RESET);
//     HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin,GPIO_PIN_SET);
//     HAL_GPIO_WritePin(BUCK_EN_GPIO_Port, BUCK_EN_Pin,GPIO_PIN_RESET);
// }

/**
 * @brief  外部中断回调函数
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == KEY_1_Pin)
    {
        // 当前状态机正处于空闲
        if (key_state == KEY1_STATE_IDLE)
        {
            // 关闭当前引脚的外部中断
            __HAL_GPIO_EXTI_CLEAR_IT(KEY_1_Pin);
            HAL_NVIC_DisableIRQ(EXTI15_10_IRQn);

            KEY1_EXIT_DOWN_TIME = HAL_GetTick();   // 记住按下时的时间
            key_state = KEY1_STATE_DOWN;  // 唤醒主循环中的状态机
        }
    }else if (GPIO_Pin == KEY_3_Pin)
    {
        if (HAL_GPIO_ReadPin(KEY_3_GPIO_Port, KEY_3_Pin) == GPIO_PIN_RESET)
        {
            //Safe_Off();
        }
    }else if (GPIO_Pin == KEY_2_Pin)
    {
        // 当前状态机正处于空闲
        if (key2_state == KEY1_STATE_IDLE)
        {
            // 关闭当前引脚的外部中断
            __HAL_GPIO_EXTI_CLEAR_IT(KEY_2_Pin);
            HAL_NVIC_DisableIRQ(EXTI15_10_IRQn);

            KEY2_EXIT_DOWN_TIME = HAL_GetTick();   // 记住按下时的时间
            key2_state = KEY2_STATE_DOWN;  // 唤醒主循环中的状态机
        }
    }
}

/**
 * @brief  BUCK 按键状态机服务函数
 * @note   在 main.c 的 while(1) 中调用
 */
void BUCK_Key1_Process(void)
{
    switch (key_state)
    {
        case KEY1_STATE_IDLE:
            // 如果中断还没触发，退出
            break;
        case KEY1_STATE_DOWN:
            // 时间戳消抖：等待 20ms 的物理前沿抖动过去
            if ((HAL_GetTick() - KEY1_EXIT_DOWN_TIME) >= 20)
            {
                if (HAL_GPIO_ReadPin(KEY_1_GPIO_Port, KEY_1_Pin) == GPIO_PIN_RESET)
                {
                    // 20ms 后依然是低电平，说明已经按下，进入功能态
                    HAL_GPIO_TogglePin(BUCK_EN_GPIO_Port, BUCK_EN_Pin);
                    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
                    key_state = KEY1_STATE_UP; // 检测是否抬起
                }
                else
                {
                    // 20ms 后变高电平了，说明是环境干扰，直接复位
                    key_state = KEY1_STATE_IDLE;
                    __HAL_GPIO_EXTI_CLEAR_IT(KEY_1_Pin);
                    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn); // 重新开启外部中断
                }
            }
            break;

        case KEY1_STATE_UP:
            if (HAL_GPIO_ReadPin(KEY_1_GPIO_Port, KEY_1_Pin) == GPIO_PIN_SET)
            {
                // 手松开
                key_state = KEY1_STATE_IDLE;          // 回归最初的空闲态
                __HAL_GPIO_EXTI_CLEAR_IT(KEY_1_Pin);  // 清除松手瞬间可能产生的后沿毛刺
                HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);   // 等待下一次按下
            }
            break;

        default:
            break;
    }
}

void Key2_Process(void)
{
    switch (key2_state)
    {
        case KEY2_STATE_IDLE:
            // 如果中断还没触发，退出
            break;
        case KEY2_STATE_DOWN:
            // 时间戳消抖：等待 20ms 的物理前沿抖动过去
            if ((HAL_GetTick() - KEY2_EXIT_DOWN_TIME) >= 20)
            {
                if (HAL_GPIO_ReadPin(KEY_2_GPIO_Port, KEY_2_Pin) == GPIO_PIN_RESET)
                {
                    // 20ms 后依然是低电平，说明已经按下，进入功能态
                    if (VOL_STEP_flag<3) {
                        VOL_STEP =VOL_STEP * 10;
                        VOL_STEP_flag++;
                    }else if (VOL_STEP_flag>=3) {
                        VOL_STEP_flag = 0;
                        VOL_STEP = 0.001f;
                    }

                    key_state = KEY2_STATE_UP; // 检测是否抬起
                }
                else
                {
                    // 20ms 后变高电平了，说明是环境干扰，直接复位
                    key_state = KEY2_STATE_IDLE;
                    __HAL_GPIO_EXTI_CLEAR_IT(KEY_2_Pin);
                    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn); // 重新开启外部中断
                }
            }
            break;

        case KEY2_STATE_UP:
            if (HAL_GPIO_ReadPin(KEY_2_GPIO_Port, KEY_2_Pin) == GPIO_PIN_SET)
            {
                // 手松开
                key_state = KEY2_STATE_IDLE;          // 回归最初的空闲态
                __HAL_GPIO_EXTI_CLEAR_IT(KEY_2_Pin);  // 清除松手瞬间可能产生的后沿毛刺
                HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);   // 等待下一次按下
            }
            break;

        default:
            break;
    }
}

// void BUCK_Enable(void){
//     if (HAL_GPIO_ReadPin(KEY_1_GPIO_Port, KEY_1_Pin) == GPIO_PIN_RESET) {
//         HAL_Delay(20);
//         if (HAL_GPIO_ReadPin(KEY_1_GPIO_Port, KEY_1_Pin) == GPIO_PIN_RESET) {
//             HAL_GPIO_TogglePin(BUCK_EN_GPIO_Port, BUCK_EN_Pin);
//             HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
//             while (HAL_GPIO_ReadPin(KEY_1_GPIO_Port, KEY_1_Pin) == GPIO_PIN_RESET);
//             HAL_Delay(20);
//         }
//     }
// }

// void SW_IN_Enable(void){
//
//     if (HAL_GPIO_ReadPin(KEY_1_GPIO_Port, KEY_1_Pin) == GPIO_PIN_RESET) {
//         HAL_Delay(20);
//         if (HAL_GPIO_ReadPin(KEY_1_GPIO_Port, KEY_1_Pin) == GPIO_PIN_RESET) {
//             HAL_GPIO_TogglePin(SW_IN_GPIO_Port, SW_IN_Pin);
//             HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
//             while (HAL_GPIO_ReadPin(KEY_1_GPIO_Port, KEY_1_Pin) == GPIO_PIN_RESET);
//             HAL_Delay(20);
//         }
//     }
// }

/**
 * @brief  旋转编码器解耦控制函数
 * @note   在 main.c 的 while(1) 中调用。顺时针纯加，逆时针纯减。
 */
void Encoder_Process(void)
{
    // 读取纯无符号硬件原始计数
    uint16_t current_encoder_cnt = __HAL_TIM_GET_COUNTER(&htim4);
    // 差分计算增量：利用无符号数溢出回绕特性，正旋为正（+1），反旋为负（-1）
    int16_t delta = (int16_t)( current_encoder_cnt-last_encoder_cnt );
    // 只有当旋钮发生旋转时，才进入核心逻辑
    if (delta != 0)
    {
        last_encoder_cnt = current_encoder_cnt;
        switch (DAC_CHANNEL_FLAG)
        {
            case DAC_CHANNEL_1:
                DAC1_Target_voltage -= (float)delta * VOL_STEP ;
                if (DAC1_Target_voltage <= 0 ) {
                    DAC1_Target_voltage = 0;
                } else if (DAC1_Target_voltage >= 3.3f) {
                    DAC1_Target_voltage = 3.3f;
                }
                DAC_Set_Voltage(DAC_CHANNEL_1, DAC_GAIN_2X, DAC1_Target_voltage);
                break;

            case DAC_CHANNEL_2:
                DAC2_Target_voltage -= (float)delta * VOL_STEP ;
                if (DAC2_Target_voltage <= 0 ) {
                    DAC2_Target_voltage = 0;
                } else if (DAC2_Target_voltage >= 1.3f) {
                    DAC2_Target_voltage = 1.3f;
                }
                DAC_Set_Voltage(DAC_CHANNEL_2, DAC_GAIN_1X, DAC2_Target_voltage);
                break;

            default:
                break;
        }
    }
}

/**
 * @brief  系统状态遥测串口输出
 */
void DAC_OUTPUT(void) {
    char msg[256];

    //加上 0.5f 转换为四舍五入
    int DAC1_Print_mv = (int)(DAC1_Target_voltage * 1000.0f + 0.5f);
    int DAC2_Print_mv = (int)(DAC2_Target_voltage * 1000.0f + 0.5f);

    snprintf(msg, sizeof(msg),
       "--------------------------------------------------\r\n"
       "DAC1_Target: %4d mV | DAC2_Target: %4d mV\r\n",
       DAC1_Print_mv, DAC2_Print_mv);

    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 50);
}



void ENC_SW(void){
    if (HAL_GPIO_ReadPin(ENC_SW_GPIO_Port, ENC_SW_Pin) == GPIO_PIN_RESET) {
        HAL_Delay(20);
        if (HAL_GPIO_ReadPin(ENC_SW_GPIO_Port, ENC_SW_Pin) == GPIO_PIN_RESET) {
            DAC_CHANNEL_FLAG=!DAC_CHANNEL_FLAG;
            while (HAL_GPIO_ReadPin(ENC_SW_GPIO_Port, ENC_SW_Pin) == GPIO_PIN_RESET);
            HAL_Delay(20);
        }
    }
}