//
// Created by lin on 2026/5/16.
//

#ifndef INC_2026_5_4_DAC_H
#define INC_2026_5_4_DAC_H

#include "main.h"

#define DAC_SYNC_LOW()           HAL_GPIO_WritePin(DAC_CS_GPIO_Port, DAC_CS_Pin, GPIO_PIN_RESET)
#define DAC_SYNC_HIGH()          HAL_GPIO_WritePin(DAC_CS_GPIO_Port, DAC_CS_Pin, GPIO_PIN_SET)

#define DAC_INTERNAL_VOLTAGE    2.048f
/* --- 命令宏定义，按照数据手册 --- */
#define DAC_SHUT_UP            0x01  //  开启输出
#define DAC_SHUP_DOWN          0x00  //  关闭输出

/* --- 通道宏定义，按照数据手册 --- */
#define DAC_CHANNEL_1          0x00  // 选中通道 1
#define DAC_CHANNEL_2          0x01  // 选中通道 2

/* --- 增益宏定义，按照数据手册 --- */
#define DAC_GAIN_1             0x01  //  增益1
#define DAC_GAIN_2             0x00  //  增益2


/* --- 函数声明 --- */
void DAC_Init(SPI_HandleTypeDef *hspi);
void DAC_Write_Cmd(uint8_t channel, uint8_t gain, uint8_t shut, uint16_t data);
void DAC_Set_Voltage(uint8_t channel, uint8_t gain, float voltage);

#endif //INC_2026_5_4_DAC_H