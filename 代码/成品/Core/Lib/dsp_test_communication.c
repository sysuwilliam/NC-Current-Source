//
// Created by lin on 2026/6/2.
//

#include "dsp_test_communication.h"
#include <stdio.h>
#include <string.h>
#include "global_value.h"
#include "usart.h"
#include "TJC_HMI.h"

/**
 * @brief  系统全局状态遥测串口高精度输出 (全整型高能版)
 * @note   位置：覆盖原 Global_OUTPUT。纯整型算术，无任何 %f 软件开销，电压精确至 0.1mV。
 */
void Global_OUTPUT(void)
{
    char msg[648];

    /*-----------------------------------------------------------------
     * 1. 现实工程硬核对齐：将浮点账本通过定点扩展，全部映射为 32 位无符号整型
     * 扩展物理量：电压/电流放大 10000 倍 (对应 4 位小数)；电阻放大 1000 倍 (对应 3 位小数)
     *----------------------------------------------------------------*/
    uint32_t i_set_uv       = (uint32_t)(I_set * 10000.0f + 0.5f);
    uint32_t i_set_mv       = (uint32_t)(I_set * 1000.0f + 0.5f);
    uint32_t dac2_cmd_uv    = (uint32_t)(DAC2_cmd * 10000.0f + 0.5f);

    uint32_t voutp_adc_uv   = (uint32_t)(VOUTP_adc * 10000.0f + 0.5f);
    uint32_t voutn_adc_uv   = (uint32_t)(VOUTN_adc * 10000.0f + 0.5f);
    uint32_t v_load         = (voutp_adc_uv - voutn_adc_uv)/10;
    uint32_t vsence_adc_uv  = (uint32_t)(Vsence_adc * 10000.0f + 0.5f); // 确保变量名 Vsence_adc 与全局对齐

    uint32_t vmos_uv        = (uint32_t)(Vmos * 10000.0f + 0.5f);
    uint32_t i_disp_uv      = (uint32_t)(I_disp * 10000.0f + 0.5f);
    uint32_t i_disp_mv      = (uint32_t)(I_disp * 100000.0f + 0.5f);
    uint32_t i_fast_uv      = (uint32_t)(I_fast * 10000.0f + 0.5f);

    // 电阻通常保留 3 位小数即可，放大 1000 倍
    Rload_disp              = (VOUTP_adc-VOUTN_adc)/I_disp;
    uint32_t rload_disp_uv  = (uint32_t)(Rload_disp * 1000.0f + 0.5f);

    uint32_t power_mw       = i_set_mv * v_load ;

    /*-----------------------------------------------------------------
     * 2. 核心机理：利用 %d.%04d 语法糖规避浮点引擎
     * %04d 的物理现实：当余数为 5 时，会自动强行补零输出 "0005"，保证账本不串行
     *----------------------------------------------------------------*/
    snprintf(msg, sizeof(msg),
       "\r\n========= SYSTEM TELEMETRY =============\r\n"
       " [Control Target]  I_set      : %5lu.%04lu A\r\n"
       " [DAC Commands]    DAC2_cmd   : %5lu.%04lu V\r\n"
       "--------------------------------------------\r\n"
       " [ADC Raw Voltage] VOUTP_adc  : %5lu.%04lu V\r\n"
       "                   VOUTN_adc  : %5lu.%04lu V\r\n"
       "                   Vsence_adc : %5lu.%04lu V\r\n"
       "-------------------------------------------\r\n"
       " [Calculated Load] Vmos       : %5lu.%04lu V\r\n"
       "                   I_disp     : %5lu.%04lu A\r\n"
       "                   I_fast     : %5lu.%04lu A\r\n"
       "                   Rload_disp : %5lu.%03lu Ohm\r\n"
       "=============================================\r\n",
       i_set_uv / 10000,       i_set_uv % 10000,
       dac2_cmd_uv / 10000,    dac2_cmd_uv % 10000,
       voutp_adc_uv / 10000,   voutp_adc_uv % 10000,
       voutn_adc_uv / 10000,   voutn_adc_uv % 10000,
       vsence_adc_uv / 10000,  vsence_adc_uv % 10000,
       vmos_uv / 10000,        vmos_uv % 10000,
       i_disp_uv / 10000,      i_disp_uv % 10000,
       i_fast_uv / 10000,      i_fast_uv % 10000,
       rload_disp_uv / 1000,   rload_disp_uv % 1000);

    // 3. 交付 UART 硬件发送线
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 150);
    TJC_SetActualCurrent_uA(i_disp_mv);
    TJC_SetLoadResistance_mOhm(rload_disp_uv);
    TJC_SetSetCurrent_mA(i_set_mv);
    TJC_SetLoadVoltage_mV(v_load);
    TJC_SetLoadPower_mW(power_mw);
}