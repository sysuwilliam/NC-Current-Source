//
// Created by lin on 2026/5/16.
//

#ifndef INC_2026_5_4_DAC_H
#define INC_2026_5_4_DAC_H

#include "main.h"

#define DAC_SYNC_LOW()           HAL_GPIO_WritePin(DAC_CS_GPIO_Port, DAC_CS_Pin, GPIO_PIN_RESET)
#define DAC_SYNC_HIGH()          HAL_GPIO_WritePin(DAC_CS_GPIO_Port, DAC_CS_Pin, GPIO_PIN_SET)

/* --- 命令宏定义，按照数据手册 --- */
#define DAC_CMD_WRITE_INPUT             0x00  // 仅写入输入寄存器 n，不更新输出
#define DAC_CMD_WRITE_UPDATE            0x00  // 仅更新输入寄存器 n
#define DAC_CMD_WRITE_INPUT_UPDATE_ALL  0x02  // 写入输入寄存器 n，并同时更新所有通道输出
#define DAC_CMD_WRITE_INPUT_UPDATE_REG  0x03  // 写入输入寄存器 n，并同时更新通道 n 输出
#define DAC_CMD_POWER_DOWN              0x04  // 设置进入断电模式
#define DAC_CMD_RESET                   0x05  // 软件复位
#define DAC_CMD_ENABLE_REFERENCE        0x07  // 内置基准源开启/关闭控制

/* --- 地址宏定义，按照数据手册 --- */
#define DAC_ADDR_CHANNEL_1            0x00  // 选中通道 1
#define DAC_ADDR_CHANNEL_2            0x01  // 选中通道 2
#define DAC_ADDR_GAIN_CONFIG          0x02  // 增益配置（gain为2才能输出3.3V电压）
#define DAC_ADDR_CHANNEL_ALL          0x07  // 选中所有通道

/* --- 函数声明 --- */
void DAC_Init(SPI_HandleTypeDef *hspi);
void DAC_Write_Cmd(uint8_t cmd, uint8_t addr, uint16_t data);
void DAC_Set_Voltage(uint8_t channel, float voltage);

#endif //INC_2026_5_4_DAC_H