//
// Created by lin on 2026/6/2.
//
#include "usart.h"
#include "communication.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "main.h"
#include "DAC.h"


volatile uint8_t RxBuff[USART_RX_BUF_SIZE] = {0};
uint8_t RxCount = 0;

// 引入一个全局帧完成标志位（通知主循环可以解析了）
volatile uint8_t g_uart_frame_ready = 0;

static uint8_t sg_rx_index = 0; // 内部私有缓冲区指针
// volatile uint8_t g_usart1_rx_flag = 0;
// uint8_t g_usart1_rx_byte = 0;


void BUCK_USART1_InitRx(void)
{
    HAL_UART_Receive_IT(&huart1, &g_usart1_rx_byte, 1);
}

// void BUCK_USART1_ParseStringCommand(void)
// {
//     if (g_usart1_rx_flag == 1)
//     {
//         float parsed_voltage = 0.0f;
//         char *p_cmd = NULL;
//
//         // 【安全习惯】：在解析前，强制强行将缓冲区末尾填 0，确保 strstr 安全闭环，防止指针越界
//         g_usart1_rx_buf[USART_RX_BUF_SIZE - 1] = '\0';
//
//         p_cmd = strstr((char*)g_usart1_rx_buf, "@DAC1:");
//         if (p_cmd != NULL)
//         {
//             if (sscanf(p_cmd + 6, "%f", &parsed_voltage) == 1)
//             {
//                 if (parsed_voltage < 0.0f) parsed_voltage = 0.0f;
//                 if (parsed_voltage > 3.3f) parsed_voltage = 3.3f;
//
//                 DAC1_Target_voltage = parsed_voltage;
//                 DAC_Set_Voltage(DAC_CHANNEL_1, DAC_GAIN_2X, DAC1_Target_voltage);
//             }
//         }
//         else if ((p_cmd = strstr((char*)g_usart1_rx_buf, "@DAC2:")) != NULL)
//         {
//             if (sscanf(p_cmd + 6, "%f", &parsed_voltage) == 1)
//             {
//                 if (parsed_voltage < 0.0f) parsed_voltage = 0.0f;
//                 if (parsed_voltage > 1.3f) parsed_voltage = 1.3f;
//
//                 DAC2_Target_voltage = parsed_voltage;
//                 DAC_Set_Voltage(DAC_CHANNEL_2, DAC_GAIN_1X, DAC2_Target_voltage);
//             }
//         }
//
//         // 3. 【原子清理】：数据复位
//         memset((void*)g_usart1_rx_buf, 0, USART_RX_BUF_SIZE);
//         sg_rx_index = 0;
//         g_usart1_rx_flag = 0;
//     }
// }

/**
 * @brief  HAL库标准的串口接收中断回调函数
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART1)
    {
        // 1. 【安全防御拦截】：防止数据溢出踩坏相邻内存
        if (RxCount >= (USART_RX_BUF_SIZE - 1))
        {
            RxCount = 0; // 缓冲区满了直接强制覆盖，拒绝死机
        }

        // 2. 检查当前收到的字符是否为终止符（工程上通常用 '\r' 或 '\n' 作为换行结束标志）
        // 如果你的串口助手发送时勾选了“发送新行”，这会极其稳定
        if (RxBuff[RxCount] == '\n' || RxBuff[RxCount] == '\r')
        {
            if (RxCount > 0) // 确保不是空行
            {
                RxBuff[RxCount] = '\0'; // 强行写入字符串结束符
                g_uart_frame_ready = 1;  // 激活主循环解析看板
            }
        }
        else
        {
            RxCount++; // 没有收到结束符，计数递增，继续接收下一个字节
        }

        // 3. 只有在当前帧没处理完时，才继续挂载接收指针
        if (g_uart_frame_ready == 0)
        {
            HAL_UART_Receive_IT(&huart1, (uint8_t*)&RxBuff[RxCount], 1);
        }
    }
}

/* ======= 【工程纠偏 1】：无条件拦截硬件溢出错误，防止外设瘫痪锁死 ======= */
/**
 * @brief  HAL库标准的串口错误中断回调函数
 * @note   当发生上电火花噪声、过载丢包、波特率轻微对不齐时，由此处托底复位硬件状态
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    // if (huart->Instance == USART1)
    // {
    //     // STM32F1 硬件特性：连续读取 SR 和 DR 寄存器，即可硬件级无条件清除 ORE/NE/FE/PE 标志
    //     volatile uint32_t tmpreg = huart->Instance->SR;
    //     tmpreg = huart->Instance->DR;
    //     (void)tmpreg; // 抑制未使用变量的警告
    //
    //     // 强行把 HAL 库内部锁死的Ready状态解开，并重新喂入接收中断指针
    //     huart->RxState = HAL_UART_STATE_READY;
    //     HAL_UART_Receive_IT(huart, &g_usart1_rx_byte, 1);
    // }
}

/**
 * @brief  串口命令解析器
 * @note   支持语法示例：
 * "DAC1:1500\n" -> 设置恒压Buck输出 1500mV
 * "DAC2:500\n"  -> 设置恒流环目标 500mA
 */
void Uart_Command_Parser(void)
{
    // 如果中断没有把帧准备好，无条件退出，绝不阻塞主循环
    if (g_uart_frame_ready == 0) return;

    /*-----------------------------------------------------------------
     * 核心业务解析区
     *----------------------------------------------------------------*/
    int target_val = 0;

    // 1. 匹配格式 "DAC1:xxxx"
    if (sscanf((char*)RxBuff, "DAC1:%d", &target_val) == 1)
    {
        // 限制硬件安全边界 (0V ~ 3.3V)
        if (target_val < 0)    target_val = 0;
        if (target_val > 3300) target_val = 3300;

        // 更新全局目标物理账本并推入硬件
        DAC1_Target_voltage = (float)target_val / 1000.0f;
        DAC_Set_Voltage(DAC_CHANNEL_1, DAC_GAIN_2X, DAC1_Target_voltage);
    }
    // 2. 匹配格式 "DAC2:xxxx" (设定目标电流，单位：mA)
    else if (sscanf((char*)RxBuff, "DAC2:%d", &target_val) == 1)
    {
        // 限制电流环安全边界 (0mA ~ 1300mA)
        if (target_val < 0)    target_val = 0;
        if (target_val > 1300) target_val = 1300;

        // 转换为目标电压伏特数
        DAC2_Target_voltage = (float)target_val / 1000.0f;

        /* 引入你上次更新的校准控制公式进行纠偏 */
        float I_set_mA = DAC2_Target_voltage * 1000.0f;
        float calibrated_DAC2_cmd_mV = (I_set_mA - 1.3684f) / 0.49819f;
        if (calibrated_DAC2_cmd_mV < 0.0f) calibrated_DAC2_cmd_mV = 0.0f;

        DAC_Set_Voltage(DAC_CHANNEL_2, DAC_GAIN_1X, calibrated_DAC2_cmd_mV / 1000.0f);
    }

    /*-----------------------------------------------------------------
     * 【原子动作】：清除看板状态，重新开启下一轮接收
     *----------------------------------------------------------------*/
    __disable_irq(); // 关中断，保护缓冲区清除过程不被踩坏
    RxCount = 0;
    memset((uint8_t*)RxBuff, 0, sizeof(RxBuff));
    g_uart_frame_ready = 0;
    __enable_irq();

    // 重新拉起底层硬件接收中断中断
    HAL_UART_Receive_IT(&huart1, (uint8_t*)&RxBuff[0], 1);
}