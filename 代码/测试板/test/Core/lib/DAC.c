//
// Created by lin on 2026/5/16.
//
#include "DAC.h"

static SPI_HandleTypeDef *p_dac_spi = NULL;

/**
 * @brief  初始化 DAC并激活内部基准源
 * @param  hspi: 指向 STM32 硬件 SPI 句柄的指针（如 &hspi1）
 */
void DAC_Init(SPI_HandleTypeDef *hspi)
{
    if (hspi == NULL) return;
    p_dac_spi = hspi;
    // 初始化拉高片选，关闭输出
    DAC_SYNC_HIGH();
    HAL_Delay(5);
    DAC_Write_Cmd(DAC_CMD_ENABLE_REFERENCE, DAC_ADDR_CHANNEL_ALL , 0x0001);//开启DAC
    HAL_Delay(5);
    DAC_Write_Cmd(DAC_CMD_WRITE_INPUT, DAC_ADDR_GAIN_CONFIG , 0x0000); //设置增益为2
    HAL_Delay(5);
    DAC_Write_Cmd(DAC_CMD_WRITE_UPDATE, DAC_ADDR_CHANNEL_ALL , 0x0000); //更新增益到全部通道

}

/**
 * @brief  底层驱动：向 DAC写入标准的 24 位 SPI 数据帧
 * @param  cmd:  C2-C0 命令选择
 * @param  addr: A2-A0 地址/通道选择
 * @param  data: 16位模拟数字量数据输入 (范围 0 ~ 65535)
 */
void DAC_Write_Cmd(uint8_t cmd, uint8_t addr, uint16_t data)
{
    uint8_t tx_buf[3] = {0}; //三个8bit数据
    if (p_dac_spi == NULL) return;

    /* * 严格遵循时序字节拆解拼装：
     * tx_buf[0]: (00) + (cmd) + (addr)
     */
    tx_buf[0] = ((cmd & 0x07) << 3) | (addr & 0x07);

    // 拆分为标准 8-bit SPI 字节
    tx_buf[1] = (uint8_t)((data >> 8) & 0xFF); // 高8位 (DB15-DB8)
    tx_buf[2] = (uint8_t)(data & 0xFF);        // 低8位 (DB7-DB0)

    // 拉低片选，开始发送
    DAC_SYNC_LOW();
    HAL_SPI_Transmit(p_dac_spi, tx_buf, 3, 10);

    // 拉高片选，结束发送
    DAC_SYNC_HIGH();
}

/**
 * @brief  根据设定电压值控制输出
 * @param  channel: DAC_ADDR_CHANNEL_1 或 DAC_ADDR_CHANNEL_2
 * @param  voltage: 目标输出物理电压值 (单位: V)
 */
void DAC_Set_Voltage(uint8_t channel, float voltage)
{
    uint16_t digital_value = 0;
    const float vref_max = 5.0f; // 内部基准 2.5V * Gain 2 = 5.0V

    // 防止数据越界溢出
    if (voltage < 0.0f)     voltage = 0.0f;
    if (voltage > vref_max) voltage = vref_max;

    /* * 16位数字化推演：
     * 满量程电压分度为 2^16 - 1 = 65535。
     * 理论模型：数据 = (当前电压 / 5.0V) * 65535
     */
    digital_value = (uint16_t)((voltage / vref_max) * 65535.0f);

    // 发送带有即时更新输出的命令 (DAC_CMD_WRITE_UPDATE_SINGLE)
    DAC_Write_Cmd(DAC_CMD_WRITE_INPUT_UPDATE_REG, channel, digital_value);
}