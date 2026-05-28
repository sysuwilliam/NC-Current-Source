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
    // 初始化时主动向两通道写入断电或0V配置
    DAC_Write_Cmd(DAC_CHANNEL_1, DAC_GAIN_1, DAC_SHUP_DOWN, 0);
    HAL_Delay(5);
    DAC_Write_Cmd(DAC_CHANNEL_2, DAC_GAIN_1, DAC_SHUP_DOWN, 0);

}

/**
 * @brief  底层驱动：向 DAC写入标准的 16 位 SPI 数据帧
 * @param  channel:  地址/通道选择
 * @param  gain:增益选择
 * @param  shut:电源开启
 * @param  data: 12位模拟数字量数据输入 (范围 0 ~ 4096)
 */
void DAC_Write_Cmd(uint8_t channel, uint8_t gain,uint8_t shut,uint16_t data)
{
    uint8_t tx_buf[2] = {0}; //两个8bit数据
    if (p_dac_spi == NULL) return;

    data &= 0x0FFF; //数据截断为12位
    tx_buf[0] = (uint8_t)(((channel & 0x01) << 7) | ((gain & 0x01) << 5) | ((shut & 0x01) << 4) | ((data >> 8) & 0x0F));
    tx_buf[1] = (uint8_t)(data & 0xFF);

    // 拉低片选，开始发送
    DAC_SYNC_LOW();
    HAL_SPI_Transmit(p_dac_spi, tx_buf, 2, 10);

    // 拉高片选，结束发送
    DAC_SYNC_HIGH();
}

/**
 * @brief  根据设定电压值控制输出
 * @param  channel: DAC_CHANNEL_1 或 DAC_CHANNEL_2
 * @param  voltage: 目标输出物理电压值 (单位: V)
 */
void DAC_Set_Voltage(uint8_t channel, uint8_t gain, float voltage)
{
    uint16_t digital_value = 0;
    float vref_max = DAC_INTERNAL_VOLTAGE;

    if (gain == DAC_GAIN_2)
    {
        vref_max = DAC_INTERNAL_VOLTAGE * 2; // 2x 增益
    }
    else
    {
        vref_max = DAC_INTERNAL_VOLTAGE; // 1x 增益
    }

    // 防止数据越界溢出
    if (voltage < 0.0f)     voltage = 0.0f;
    if (voltage > vref_max) voltage = vref_max;

    /* * 12位数字化推演：
     * 满量程电压分度为 2^12 - 1 = 4095。
     * 理论模型：数据 = (当前电压 / 4.096V) * 4095
     */
    digital_value = (uint16_t)((voltage / vref_max) * 4095.0f + 0.5f);
    DAC_Write_Cmd(channel, gain, DAC_SHUT_UP, digital_value);
}