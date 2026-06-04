//
// Created by lin on 2026/5/29.
//

#include "DSP_ADC.h"
#include "adc.h"
#include "tim.h"
#include <stdio.h>
#include <string.h>
#include "usart.h"
#include <stdlib.h>



int32_t I_actual = 0;  // 单位：uA (微安)，保留 3 位小数
int32_t Vload = 0;     // 单位：mV
int32_t VMOS = 0;      // 单位：mV
int32_t RLOAD = 0;     // 单位：mOhm (毫欧)，保留 3 位小数

uint32_t VOUT_P = 0;   // 单位：mV
uint32_t VOUT_N = 0;   // 单位：mV
uint32_t Vsense = 0;   // 单位：mV

// 引入滤波线，用于累加跳变
static uint64_t filter_I_actual_uA = 0;

#define LPF_SHIFT  3
uint32_t adc_ui_original[2];
uint16_t ADC1_CH1 = 0;
uint16_t ADC2_CH1 = 0;
uint16_t ADC1_CH2 = 0;

#define LPF_SHIFT  3

void ADC_Init(void) {
    //校准ADC
    HAL_ADCEx_Calibration_Start(&hadc1);
    HAL_ADCEx_Calibration_Start(&hadc2);

    HAL_ADC_Start(&hadc2); // 先开启从机
    HAL_ADCEx_MultiModeStart_DMA(&hadc1, adc_ui_original, 2);//开启DMA
    HAL_TIM_Base_Start(&htim3);//同步时钟
}

void ADC_Calculate(void) {
    uint32_t local_buf[2];
    __disable_irq();
    local_buf[0] = adc_ui_original[0];
    local_buf[1] = adc_ui_original[1];
    __enable_irq();

    // 1. 提取原始的 12 位 ADC 寄存器值 (0 - 4095)
    uint32_t raw_adc_P = (uint32_t)(local_buf[0] & 0xFFFF);
    uint32_t raw_adc_N = (uint32_t)((local_buf[0] >> 16) & 0xFFFF);
    uint32_t raw_adc_S = (uint32_t)(local_buf[1] & 0xFFFF);

    // 2. 理想理论模型计算（转换为原始 mV ）
    uint32_t calc_VOUT_P_raw = (raw_adc_P * 3300 * 11) / 4095;
    uint32_t calc_VOUT_N_raw = (raw_adc_N * 3300 * 11) / 4095;
    uint32_t calc_Vsense_raw = (raw_adc_S * 3300) / 4095;

    /*-----------------------------------------------------------------
     * 【强制纠偏机制】：植入真实环境校准曲线 (防止显示偏高/偏低)
     * 使用 uint64_t 放大系数防整型截断，最后加上/减去系统偏置
     *----------------------------------------------------------------*/
    uint32_t calc_VOUT_P = (uint32_t)(((uint64_t)calc_VOUT_P_raw * 985864ULL) / 1000000ULL) + 2;

    uint32_t calc_VOUT_N = 0;
    uint64_t vout_n_scaled = ((uint64_t)calc_VOUT_N_raw * 998639ULL) / 1000000ULL;
    if (vout_n_scaled > 27) {
        calc_VOUT_N = (uint32_t)(vout_n_scaled - 27);
    } else {
        calc_VOUT_N = 0; // 防止负溢出卡死
    }

    // Vsense 直接参与恒流环不改变原始物理意义，此处保持原样
    uint32_t calc_Vsense = calc_Vsense_raw;

    // 3. 一阶低通滤波架设
    static uint8_t is_first_run = 1;
    if (is_first_run) {
        VOUT_P = calc_VOUT_P;
        VOUT_N = calc_VOUT_N;
        Vsense = calc_Vsense;
    } else {
        VOUT_P = VOUT_P + (uint32_t)(((int32_t)calc_VOUT_P - (int32_t)VOUT_P) >> LPF_SHIFT);
        VOUT_N = VOUT_N + (uint32_t)(((int32_t)calc_VOUT_N - (int32_t)VOUT_N) >> LPF_SHIFT);
        Vsense = Vsense + (uint32_t)(((int32_t)calc_Vsense - (int32_t)Vsense) >> LPF_SHIFT);
    }

    // 4. 计算外部主链路真实压差
    Vload = (int32_t)VOUT_P - (int32_t)VOUT_N;
    VMOS  = (int32_t)VOUT_N - (int32_t)Vsense;

    // 5. 电流高精度原始 uA 计算
    uint64_t current_uA_raw = ((uint64_t)raw_adc_S * 3300000000ULL) / (4095ULL * Rs);

    // 6. 核心电流低通滤波
    if (is_first_run) {
        filter_I_actual_uA = current_uA_raw;
        is_first_run = 0;
    } else {
        filter_I_actual_uA = filter_I_actual_uA + (((int64_t)current_uA_raw - (int64_t)filter_I_actual_uA) >> LPF_SHIFT);
    }

    /*-----------------------------------------------------------------
     * 【数据对齐修改】：对滤波后的高精度电流进行“显示链路校准拦截”
     * 消除 I_serial 明显偏高的问题，使串口数据向标准仪表靠拢
     *----------------------------------------------------------------*/
    int32_t I_actual_raw = (int32_t)filter_I_actual_uA;

    // 注入拟合公式: I_calibrated = I_raw * 0.98022 + 23 uA
    I_actual = (int32_t)(((int64_t)I_actual_raw * 98022LL) / 100000LL) + 23;

    // 7. 阈值拦截与小信号卡死
    if (I_actual < 500) { // 小于 500 uA (0.5 mA)
        I_actual = 0;
        RLOAD = 0;
    } else {
        // 动态电阻高精度计算 (利用校准后的 Vload 和 I_actual)
        // 计算结果单位为 mOhm
        RLOAD = (int32_t)(((uint64_t)Vload * 1000000ULL) / (uint32_t)I_actual);
    }
}


void ADC_OUTPUT(void) {
    char msg[256];

    // 计算用于显示的绝对值余数
    uint32_t i_rem = (uint32_t)labs(I_actual % 1000);
    uint32_t r_rem = (uint32_t)labs(RLOAD % 1000);

    // 串口输出
    snprintf(msg, sizeof(msg),
       "--------------------------------------------------\r\n"
       "VOUT_P: %4lu mV | VOUT_N: %4lu mV | Vsense: %4lu mV\r\n"
       "Vload : %ld mV | VMOS  : %ld mV\r\n"
       "I_actual: %ld.%03lu mA | RLOAD : %ld.%03lu Ohm\r\n",
       VOUT_P, VOUT_N, Vsense,
       Vload, VMOS,
       I_actual / 1000, i_rem,
       RLOAD / 1000, r_rem);

    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 50);
}
