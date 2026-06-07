//
// Created by lin on 2026/6/2.
//

#ifndef CI_SOURCE_FINISHED_KEY_SERVICE_H
#define CI_SOURCE_FINISHED_KEY_SERVICE_H


void KEY_Init(void);
void Encoder_Process(void);
void DAC_SW_CH(void);
extern float DAC1_Target_voltage;
#define SW_DAC_CHANNEL_1         0
#define SW_DAC_CHANNEL_2         1


#endif //CI_SOURCE_FINISHED_KEY_SERVICE_H