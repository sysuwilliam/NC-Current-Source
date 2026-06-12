//
// Created by lin on 2026/6/4.

#ifndef CI_SOURCE_FINISHED_GLOBAL_VALUE_H
#define CI_SOURCE_FINISHED_GLOBAL_VALUE_H

#include "TJC_HMI.h"
#define Rs 2
/**
 *
 */
extern float I_set ; //用户设定电流
extern float DAC2_cmd ;
extern float I_disp ;
extern float I_fast ;
extern float VOUTP_adc ;
extern float VOUTN_adc ;
extern float Rload_disp ;
extern float Vmos ;
extern float Vsence_adc ;
extern float VOUTP_set ;
extern float DAC1_cmd ;

extern float I_step;
extern float V_step;

extern uint8_t ADC_FLAG;
extern TJC_ProtectState_t Safe_flag;
#endif //CI_SOURCE_FINISHED_GLOBAL_VALUE_H