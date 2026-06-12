//
// Created by lin on 2026/6/4.
//

#include "global_value.h"
#include "TJC_HMI.h"
#include "main.h"

float I_set = 0.0f; //用户设定电流
float DAC2_cmd = 0.0f;
float I_step = 0.01f;

float VOUTP_set = 0.0f;
float DAC1_cmd= 0.0f;
float V_step = 0.01f;

float I_disp = 0.0f;
float I_fast = 0.0f;

float VOUTP_adc = 0.0f;
float VOUTN_adc = 0.0f;
float Rload_disp = 0.0f;

float Vsence_adc = 0.0f;
float Vmos = 0.0f;

uint8_t ADC_FLAG = 0;

TJC_ProtectState_t Safe_flag = TJC_PROTECT_NORMAL;
