//
// Created by lin on 2026/6/2.
//

#include "dsp_ADC.h"
#include "adc.h"
#include "tim.h"

uint32_t adc_buff[2];
volatile uint8_t adc_dma_ready = 0;//dma完成标志


void ADC_Init(void) {
    //校准ADC
    HAL_ADCEx_Calibration_Start(&hadc1);
    HAL_ADCEx_Calibration_Start(&hadc2);

    HAL_ADC_Start(&hadc2); // 先开启从机
    HAL_ADCEx_MultiModeStart_DMA(&hadc1,adc_buff , 2);//开启DMA
    HAL_TIM_Base_Start(&htim3);//同步时钟
}

/**
 * @brief  纯粹的硬件ADC原始数据读取
 * @param  out_raw_p : 输出原始 VOUT_P 寄存器值 (0 - 4095)
 * @param  out_raw_n : 输出原始 VOUT_N 寄存器值 (0 - 4095)
 * @param  out_raw_s : 输出原始 Vsense 寄存器值 (0 - 4095)
 */
void ADC_Raw_Read(uint32_t *out_raw_p, uint32_t *out_raw_n, uint32_t *out_raw_s)
{
    uint32_t local_buf[2];

    // 防止读取过程中被 DMA 传输中断打断
    __disable_irq();
    local_buf[0] = adc_buff[0];
    local_buf[1] = adc_buff[1];
    __enable_irq();

    // 提取原始的 12 位 ADC 寄存器值并交付给调用的函数
    *out_raw_p = (uint32_t)(local_buf[0] & 0xFFFF);
    *out_raw_n = (uint32_t)((local_buf[0] >> 16) & 0xFFFF);
    *out_raw_s = (uint32_t)(local_buf[1] & 0xFFFF);
}

/**
 * @brief  中间值滤波算法引擎 (最大精度无损版)
 * @note   不再中途做除法，全额保留 100 个有效点的累加和，彻底消除截断误差
 * @param  adc_buf: 指向已采集满 N(300) 个元素的原始数据集缓冲区
 * @return 100 个有效采样点的无损累加和 (uint32_t)
 */
uint16_t GetADC_Average_Engine(uint16_t *adc_buf)
{
    uint16_t AdcValueTemp = 0;
    uint32_t sum = 0;
    int count = 0;
    int i = 0;
    int j = 0;

    // 1. 严格复现双重 for 循环冒泡排序 (已修正越界 Bug)
    for (j = 0; j < NA - 1; j++)
    {
        for (i = 0; i < NA - j - 1; i++)
        {
            if (adc_buf[i] > adc_buf[i + 1])
            {
                AdcValueTemp = adc_buf[i];
                adc_buf[i]   = adc_buf[i + 1];
                adc_buf[i + 1] = AdcValueTemp;
            }
        }
    }

    // 2. 去头去尾求和：有效计算点数为 (300 - 100 * 2) = 100 个
    for (count = BAN; count < NA - BAN; count++)
    {
        sum += adc_buf[count];
    }

    return ((uint16_t)(((float)sum / (NA - BAN * 2)) + 0.5f));

}


/**
 * @brief  卡尔曼滤波算法
 * @note   基于上次估计数据与这次采集数据计算,对换算后的数据滤波
 * @param
 * @return 滤波后的结果数据
 */
float KalmanFilter(Kalman_Scalar_t *k_ptr,float measured_val) {

    // 1. 更新增益
    float x_mid = k_ptr->x_last;               // X(k|k-1) = X(k-1|k-1) (A=1)
    float EST_mid = k_ptr->EST_last + k_ptr->Q;    // P(k|k-1) = P(k-1|k-1) + Q，根据上一次修正
    k_ptr->kg =EST_mid / (EST_mid + k_ptr->MEA);    // K(k) = P(k|k-1) / (P(k|k-1) + R)

    // 2. 计算阶段
    // 当前最优估计值 X(k|k)
    float x_now = x_mid + k_ptr->kg * (measured_val - x_mid);

    // 更新估计方差 P(k|k)
    k_ptr->EST_last = (1.0f - k_ptr->kg) * EST_mid;

    // 滚动更新
    k_ptr->x_last = x_now;

    return x_now;
}

/**
 * @brief  DMA 传输完成中断回调函数
 * @note   此函数由 HAL 库在系统的 ADC/DMA 中断服务程序中自动调用
 * @param  hadc: 指向触发中断的 ADC 句柄
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1)
    {
        adc_dma_ready = 1;
    }
}
