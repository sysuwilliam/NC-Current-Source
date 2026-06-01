//
// Created by lin on 2026/5/16.
//

#ifndef INC_2026_5_4_DAC_H
#define INC_2026_5_4_DAC_H

#include "main.h"

// 片选控制
#define DAC_SYNC_LOW()           HAL_GPIO_WritePin(DAC_CS_GPIO_Port, DAC_CS_Pin, GPIO_PIN_RESET)
#define DAC_SYNC_HIGH()          HAL_GPIO_WritePin(DAC_CS_GPIO_Port, DAC_CS_Pin, GPIO_PIN_SET)

#define DAC_INTERNAL_VOLTAGE    2.048f

/* --- 按照 MCP4822 数据手册严格定义 --- */
// Bit 12: SHDN (1 = 工作, 0 = 关断)
#define DAC_CMD_ACTIVE         0x01  // 开启输出工作
#define DAC_CMD_SHUTDOWN       0x00  // 关闭输出进入低功耗

// Bit 15: 通道选择 (0 = 通道A, 1 = 通道B)
#define DAC_CHANNEL_1          0x00  // 通道二输出
#define DAC_CHANNEL_2          0x01  // 通道一输出

// Bit 13: GAIN增益选择 (1 = 1x增益, 0 = 2x增益)
#define DAC_GAIN_1X            0x01  // 满量程 2.048V
#define DAC_GAIN_2X            0x00  // 满量程 4.096V


/* --- 函数声明 --- */
void DAC_Init(SPI_HandleTypeDef *hspi);
void DAC_Write_Cmd(uint8_t channel, uint8_t gain, uint8_t shut, uint16_t data);
void DAC_Set_Voltage(uint8_t channel, uint8_t gain, float voltage);

#endif //INC_2026_5_4_DAC_H