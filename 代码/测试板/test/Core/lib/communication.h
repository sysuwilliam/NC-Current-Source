//
// Created by lin on 2026/6/2.
//

#ifndef TEST_COMMUNICATION_H
#define TEST_COMMUNICATION_H
#include "main.h"

// 工业标准配置
#define USART_RX_BUF_SIZE  64

// 符号广播
extern volatile uint8_t RxBuff[USART_RX_BUF_SIZE];
extern volatile uint8_t g_usart1_rx_flag;
extern uint8_t g_usart1_rx_byte;

// 函数接口声明
void BUCK_USART1_InitRx(void);
void BUCK_USART1_ParseStringCommand(void); // 新增的字符串解析函数
void Uart_Command_Parser(void);
#endif //TEST_COMMUNICATION_H