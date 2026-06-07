//
// Created by lin on 2026/6/2.
//

#include "dsp_DAC.h"


static SPI_HandleTypeDef *p_dac_spi = NULL;

/**
 * @brief  初始化 DAC 并默认关闭输出
 */
void DAC_Init(SPI_HandleTypeDef *hspi)
{
    if (hspi == NULL) return;
    p_dac_spi = hspi;

    // 初始化拉高片选
    DAC_SYNC_HIGH();

    // 初始化时主动向两通道写入断电配置，防止上电浮空输出高电平
    DAC_Write_Cmd(DAC_CHANNEL_1, DAC_GAIN_1X, DAC_CMD_SHUTDOWN, 0);
    HAL_Delay(5);
    DAC_Write_Cmd(DAC_CHANNEL_2, DAC_GAIN_1X, DAC_CMD_SHUTDOWN, 0);
}

/**
 * @brief  底层驱动：向 DAC 写入标准的 16 位 SPI 数据帧 (SPI配置为16bit模式)
 * @param  channel: DAC_CHANNEL_1 或 DAC_CHANNEL_2
 * @param  gain: DAC_GAIN_1X 或 DAC_GAIN_2X
 * @param  shut: 控制通道的开启或关闭
 * @param  data: 12位数据，控制输出电压值（0 ~ 4095）
 */
void DAC_Write_Cmd(uint8_t channel, uint8_t gain, uint8_t shut, uint16_t data)
{
    uint16_t tx_buf = 0;
    if (p_dac_spi == NULL) return;

    data &= 0x0FFF; // 确保数据只有 12 位

    // 严格对照数据手册拼接高字节和低字节
    tx_buf = (channel << 15) | (gain << 13) | (shut << 12) | (data);

    // 拉低片选，开始发送
    DAC_SYNC_LOW();
    HAL_SPI_Transmit(p_dac_spi, (uint8_t *)&tx_buf, 1, 10);
    // 拉高片选，结束发送并触发 DAC 锁存输出
    DAC_SYNC_HIGH();
}

/**
 * @brief  根据设定电压值控制输出
 * @param  channel: DAC_CHANNEL_1 或 DAC_CHANNEL_2
 * @param  gain: DAC_GAIN_1X 或 DAC_GAIN_2X
 * @param  voltage: 目标输出物理电压值 (单位: V)
 */
void DAC_Set_Voltage(uint8_t channel, uint8_t gain, float voltage)
{
    uint16_t digital_value = 0;
    float vref_max = DAC_INTERNAL_VOLTAGE;

    // 根据增益动态计算满量程电压
    if (gain == DAC_GAIN_2X)
    {
        vref_max = DAC_INTERNAL_VOLTAGE * 2.0f; // 4.096V
    }
    else
    {
        vref_max = DAC_INTERNAL_VOLTAGE;        // 2.048V
    }

    // 限幅防溢出
    if (voltage < 0.0f)     voltage = 0.0f;
    if (voltage > vref_max) voltage = vref_max;

    // 四舍五入计算 12 位数字量 (0 ~ 4095)
    digital_value = (uint16_t)((voltage / vref_max) * 4096.0f + 0.5f);

    // 发送数据并使能工作 (DAC_CMD_ACTIVE)
    DAC_Write_Cmd(channel, gain, DAC_CMD_ACTIVE, digital_value);
}