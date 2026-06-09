//
// Created by lin on 2026/6/2.
//

#ifndef CI_SOURCE_FINISHED_DSP_ADC_H
#define CI_SOURCE_FINISHED_DSP_ADC_H
#include "stdint.h"

/*均值滤波*/
#define NA    20     // 采样次数
#define BAN  8     // 舍弃数据个数

/*卡尔曼滤波*/
typedef struct {
    float x_last;      // 上一次的估计值X
    float EST_last;    // 上一次的估计误差eEST
    float Q;           // 噪声误差
    float MEA;         // 测量误差：ADC读取的值与实际值存在的误差范围
    float kg;          // 卡尔曼增益，通过公式可计算
} Kalman_Scalar_t;



void ADC_Raw_Read(uint32_t *out_v_p, uint32_t *out_v_n, uint32_t *out_v_s);
void ADC_Init(void);
uint16_t GetADC_Average_Engine(uint16_t *adc_buf);
float KalmanFilter(Kalman_Scalar_t *k_ptr,float measured_val);


#endif //CI_SOURCE_FINISHED_DSP_ADC_H