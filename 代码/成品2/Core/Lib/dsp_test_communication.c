//
// Created by lin on 2026/6/2.
//

#include "dsp_test_communication.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "global_value.h"
#include "key_service.h"
#include "usart.h"
#include "TJC_HMI.h"

/**
 * @brief  系统全局状态遥测串口高精度输出 (全整型高能版)
 * @note   位置：覆盖原 Global_OUTPUT。纯整型算术，无任何 %f 软件开销，电压精确至 0.1mV。
 */
void Global_OUTPUT(void)
{
    char msg[768];
    HAL_StatusTypeDef hmi_status;

    /*-----------------------------------------------------------------
     * 1. 现实工程硬核对齐：将浮点账本通过定点扩展，全部映射为 32 位无符号整型
     * 扩展物理量：电压/电流放大 10000 倍 (对应 4 位小数)；电阻放大 1000 倍 (对应 3 位小数)
     *----------------------------------------------------------------*/
    uint32_t i_set_x1e4_A      = (uint32_t)(I_set * 10000.0f + 0.5f);
    uint32_t i_set_mA          = (uint32_t)(I_set * 1000.0f + 0.5f);
    uint32_t dac2_cmd_x1e4_V   = (uint32_t)(DAC2_cmd * 10000.0f + 0.5f);
    uint32_t dac1_cmd_x1e4_V   = (uint32_t)(DAC1_Target_voltage * 10000.0f + 0.5f);
    

    uint32_t voutp_adc_x1e4_V  = (uint32_t)(VOUTP_adc * 10000.0f + 0.5f);
    uint32_t voutn_adc_x1e4_V  = (uint32_t)(VOUTN_adc * 10000.0f + 0.5f);
    int32_t v_load_mV          = (int32_t)((voutp_adc_x1e4_V - voutn_adc_x1e4_V) / 10);
    uint32_t vsence_adc_x1e4_V = (uint32_t)(Vsence_adc * 10000.0f + 0.5f);

    uint32_t vmos_x1e4_V       = (uint32_t)(Vmos * 10000.0f + 0.5f);
    uint32_t i_disp_x1e4_A     = (uint32_t)(I_disp * 10000.0f + 0.5f);
    uint32_t i_disp_uA         = (uint32_t)(I_disp * 1000000.0f + 0.5f);
    uint32_t i_fast_x1e4_A     = (uint32_t)(I_fast * 10000.0f + 0.5f);

    // Rload_disp 按 Ohm 计，这里转换成 mOhm 供串口屏接口使用。
    if (I_disp > 0.000001f || I_disp < -0.000001f) {
        Rload_disp = (VOUTP_adc - VOUTN_adc) / I_disp;
    } else {
        Rload_disp = 0.0f;
    }
    uint32_t rload_disp_mOhm   = (uint32_t)(Rload_disp * 1000.0f + 0.5f);

    // mA * mV = uW，因此要除以 1000 才得到 mW。这里采用实际负载电流而不是设定电流。
    uint32_t i_disp_mA         = (uint32_t)(i_disp_uA / 1000U);
    uint32_t power_mW          = (uint32_t)((i_disp_mA * (uint32_t)(v_load_mV < 0 ? 0 : v_load_mV)) / 1000U);

    /*-----------------------------------------------------------------
     * 2. 核心机理：利用 %d.%04d 语法糖规避浮点引擎
     * %04d 的物理现实：当余数为 5 时，会自动强行补零输出 "0005"，保证账本不串行
     *----------------------------------------------------------------*/
    snprintf(msg, sizeof(msg),
       "\r\n==================== SYSTEM TELEMETRY ====================\r\n"
       " [Control Target]  I_set      : %5lu.%04lu A\r\n"
       " [DAC Commands]    DAC2_cmd   : %5lu.%04lu V\r\n"
       "----------------------------------------------------------\r\n"
       " [ADC Raw Voltage] VOUTP_adc  : %5lu.%04lu V\r\n"
       "                   VOUTN_adc  : %5lu.%04lu V\r\n"
       "                   Vsence_adc : %5lu.%04lu V\r\n"
       "----------------------------------------------------------\r\n"
       " [Calculated Load] Vmos       : %5lu.%04lu V\r\n"
       "                   I_disp     : %5lu.%04lu A\r\n"
       "                   I_fast     : %5lu.%04lu A\r\n"
       "                   Rload_disp : %5lu.%03lu Ohm\r\n"
       "==========================================================\r\n",
       i_set_x1e4_A / 10000,       i_set_x1e4_A % 10000,
       dac2_cmd_x1e4_V / 10000,    dac2_cmd_x1e4_V % 10000,
       voutp_adc_x1e4_V / 10000,   voutp_adc_x1e4_V % 10000,
       voutn_adc_x1e4_V / 10000,   voutn_adc_x1e4_V % 10000,
       vsence_adc_x1e4_V / 10000,  vsence_adc_x1e4_V % 10000,
       vmos_x1e4_V / 10000,        vmos_x1e4_V % 10000,
       i_disp_x1e4_A / 10000,      i_disp_x1e4_A % 10000,
       i_fast_x1e4_A / 10000,      i_fast_x1e4_A % 10000,
       rload_disp_mOhm / 1000,     rload_disp_mOhm % 1000);

    // 3. 交付 UART 硬件发送线
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);
    hmi_status = TJC_DAC_CH1((int32_t)(dac1_cmd_x1e4_V * 10U)); // 传递给串口屏的 DAC1 电压值，单位 mV
    hmi_status = (hmi_status == HAL_OK) ? TJC_DAC_CH2((int32_t)(dac2_cmd_x1e4_V * 10U)) : hmi_status;
    hmi_status = (hmi_status == HAL_OK) ? TJC_SetOutputState(TJC_OUTPUT_ON) : hmi_status;
    hmi_status = (hmi_status == HAL_OK) ? TJC_SetProtectState(TJC_PROTECT_NORMAL) : hmi_status;
    hmi_status = (hmi_status == HAL_OK) ? TJC_SetActualCurrent_uA((int32_t)i_disp_uA) : hmi_status;
    hmi_status = (hmi_status == HAL_OK) ? TJC_SetLoadResistance_mOhm((int32_t)rload_disp_mOhm) : hmi_status;
    hmi_status = (hmi_status == HAL_OK) ? TJC_SetSetCurrent_mA((int32_t)i_set_mA) : hmi_status;
    hmi_status = (hmi_status == HAL_OK) ? TJC_SetLoadVoltage_mV(v_load_mV) : hmi_status;
    hmi_status = (hmi_status == HAL_OK) ? TJC_SetLoadPower_mW((int32_t)power_mW) : hmi_status;
    (void)((hmi_status == HAL_OK) ? TJC_WaveS0_AddCurrentPoint((int32_t)i_set_mA, (int32_t)i_disp_uA) : hmi_status);
}
