//
// Created by lin on 2026/6/4.
//

#include "PID.h"
#include <stdlib.h>
#include "main.h"
#include "dsp_ADC.h"
#include "global_value.h"
#include "dsp_DAC.h"


/*均值滤波*/
// 为 连续采样开辟的独立通道物理数组（保持静态全局区分配，防栈溢出）
static uint16_t sample_box_p[NA];
static uint16_t sample_box_n[NA];
static uint16_t sample_box_s[NA];


float VOUTP_Middle;
float VOUTN_Middle;
float Vsence_Middle;
float I_Middle;

// 实例化PID电流全局变量
PID_Controller_t pid_current;


/*卡尔曼滤波*/
// 初始化卡尔曼参数
Kalman_Scalar_t kalman_v_p = {
    .x_last = 0.0f,
    .EST_last = 1.0f,   // 不为0即可
    .Q = 0.001f,      //
    .MEA = 0.05f        // 根据你ADC跳动范围来定（比如跳动幅度的方差）
};

Kalman_Scalar_t kalman_v_n = {
    .x_last = 0.0f,
    .EST_last = 1.0f,   // 不为0即可
    .Q = 0.001f,      //
    .MEA = 0.05f        // 根据ADC跳动范围来定（比如跳动幅度的方差）
};

Kalman_Scalar_t kalman_v_s = {
    .x_last = 0.0f,
    .EST_last = 1.0f,   // 不为0即可
    .Q = 0.001f,      //
    .MEA = 0.05f        // 根据你ADC跳动范围来定（比如跳动幅度的方差）
};


/**
 * @brief  系统级高精度采样与滤波总调度任务 (纠偏规整版)
 * @note   在主循环 while(1) 中调用。引入物理时序防线，彻底消除高频重复读取导致的滤波退化。
 */
void Filter_Output(void) {
    uint32_t r_p = 0, r_n = 0, r_s = 0;
    static uint8_t Filter_count = 0;
    // 1.读取
    ADC_Raw_Read(&r_p, &r_n, &r_s);
    sample_box_p[Filter_count]=r_p;
    sample_box_n[Filter_count]=r_n;
    sample_box_s[Filter_count]=r_s;
    Filter_count++;
    /*-----------------------------------------------------------------
     * 2. 先均值滤波再换算
     *----------------------------------------------------------------*/
    if (Filter_count < NA) {
        return;
    }
    Filter_count=0;


    float raw_P = (float) GetADC_Average_Engine(sample_box_p)* (3.3f / 4095.0f);
    float raw_N = (float) GetADC_Average_Engine(sample_box_n) * (3.3f / 4095.0f);
    float raw_S = (float) GetADC_Average_Engine(sample_box_s) * (3.3f / 4095.0f);

    /*-----------------------------------------------------------------
     * 3. 滤波
     *----------------------------------------------------------------*/
    VOUTP_Middle = KalmanFilter(&kalman_v_p, raw_P);
    VOUTN_Middle = KalmanFilter(&kalman_v_n, raw_N);
    Vsence_Middle = KalmanFilter(&kalman_v_s, raw_S);

    /*-----------------------------------------------------------------
     * 4. 核心物理账本刷新
     *----------------------------------------------------------------*/
    //===========负载电压============//
    if (VOUTP_Middle < 0.001f) {
        VOUTP_adc = 0.0f;
    }else {
        VOUTP_adc = 0.98663f*(VOUTP_Middle * 11.0f)+0.13675f;
    }
    if (VOUTN_Middle < 0.001f) {
        VOUTN_adc = 0.0f;
    }else {
        VOUTN_adc = 0.97992f * (VOUTN_Middle * 11.0f)+0.24886f;
    }

    Vsence_adc = Vsence_Middle;
    Vmos = VOUTN_adc - Vsence_adc;
    I_Middle = Vsence_adc / Rs;


    if (I_Middle < 0.001f)
    {
        I_disp = 0.0f;
    }
    else
    {
        // 1. 基础线性修正（单位：A）
        float I_base = 0.98189f * I_Middle + 0.004821f;

        // 2. 连续型非线性残余误差精细修正（基于最新测试数据精密对齐）
        if (I_base < 0.010f) {
            I_disp = (I_base > 0.0f) ? (I_base * 0.943f) : 0.0f;
        }
        else if (0.010f <= I_base && I_base < 0.020f) I_disp = 0.9861f * I_base + 0.000649f;
        else if (0.020f <= I_base && I_base < 0.030f) I_disp = 0.9919f * I_base + 0.001253f - 0.0007f;
        else if (0.030f <= I_base && I_base < 0.040f) I_disp = 1.032f * I_base - 0.000350f - 0.0007f;
        else if (0.040f <= I_base && I_base < 0.050f) I_disp = 0.979f * I_base + 0.001740f;
        else if (0.050f <= I_base && I_base < 0.060f) I_disp = 1.036f * I_base - 0.001240f;
        else if (0.060f <= I_base && I_base < 0.070f) I_disp = 1.015f * I_base - 0.000520f;
        else if (0.070f <= I_base && I_base < 0.080f) I_disp = 0.986f * I_base + 0.001510f;
        else if (0.080f <= I_base && I_base < 0.090f) I_disp = 0.941f * I_base + 0.005410f;
        else if (0.090f <= I_base && I_base < 0.100f) I_disp = 1.028f * I_base - 0.001720f;
        else {
            // 大于 100mA 维持原有的高精度基础线，并整体无缝平移补偿大电流中值硬误差
            I_disp = I_base + 0.00085f;
        }

        // 3. 异常大电流保护
        if (I_disp > 0.55f) {
            I_disp = I_Middle;
            Safe_flag = TJC_PROTECT_OVERCURRENT;
        }
    }

    ADC_FLAG = 1;
}



/**
 * @brief  PID 参数初始化
 * 修改点：目标值转换引入 PID_CURRENT_SCALE 宏，消除硬编码数字
 */
void PID_Init(void)
{
    // 严格对齐 2000 倍标度的物理阻尼参数
    pid_current.Kp = 0.0007f;
    pid_current.Ki = 0.00025f;
    pid_current.Kd = 0.0f;

    pid_current.target = (int32_t)(I_set * PID_CURRENT_SCALE);
    pid_current.actual = 0;

    pid_current.err_curr = 0;
    pid_current.err_last1 = 0;
    pid_current.err_last2 = 0;
    pid_current.out_delta = 0.0f;

}

/**
 * @brief  电流环 PID 增量式核心计算
 * 修改点：全面应用 PID_CURRENT_SCALE 和 PID_CURRENT_DEADBAND 宏，实现业务与参数解耦
 */

void PID_Current_Loop(void)
{

    // 1. 同步当前最新的全局变量，精度为1/2000.
    pid_current.target = (int32_t)(I_set * PID_CURRENT_SCALE);
    pid_current.actual = (int32_t)(I_disp * PID_CURRENT_SCALE);

    // 2. 计算当前误差 E(n) (0.5mA)
    pid_current.err_curr = pid_current.target - pid_current.actual;

    /* ======= 3. 工业级死区控制（防微幅震荡干扰） ======= */
    // 使用死区宏进行边界拦截，低于死区阈值则判定误差为0
    if (abs(pid_current.err_curr) < PID_CURRENT_DEADBAND)
    {
        pid_current.err_curr = 0;
    }

    /* ======= 4. 标准增量式 PID  ======= */
    pid_current.out_delta = (pid_current.Kp * (float)(pid_current.err_curr - pid_current.err_last1)) +
                            (pid_current.Ki * (float)pid_current.err_curr) +
                            (pid_current.Kd * (float)(pid_current.err_curr - 2 * pid_current.err_last1 + pid_current.err_last2));

    /* ======= 5. 作用于外设：改变 DAC2 目标控制量 ======= */
    DAC2_cmd += pid_current.out_delta;

    /* ======= 6. 强制纠偏：硬件防御边界拦截（红线保护） ======= */
    if (DAC2_cmd <= 0.0f)
    {
        DAC2_cmd = 0.0f;
    }
    else if (DAC2_cmd >= 1.4f)
    {
        DAC2_cmd = 1.4f;
    }

    // 7. 物理刷新硬件
    DAC_Set_Voltage(DAC_CHANNEL_2, DAC_GAIN_1X, DAC2_cmd);

    // 8. 滚动的状态历史记录前移
    pid_current.err_last2 = pid_current.err_last1;
    pid_current.err_last1 = pid_current.err_curr;
}

void BUCK_Loop(void) {
    // 1. 前置防线：如果硬件未使能，立刻退出
    if (HAL_GPIO_ReadPin(BUCK_EN_GPIO_Port, BUCK_EN_Pin) == GPIO_PIN_RESET) {
        return;
    }

    static uint32_t last_slow_tick = 0;

    // 2. 1000ms 定时器时轴拦截（1秒定量刷新一次）
    if (HAL_GetTick() - last_slow_tick < 1000) {
        return;
    }
    last_slow_tick = HAL_GetTick();

    // 3. 小电流保护拦截：低于 10mA 时保持安全低电压输出，防止空载开路过压
    if (I_set < 0.010f) {
        DAC1_cmd = 0.35f; // 给一个安全低基准电平
        DAC_Set_Voltage(DAC_CHANNEL_1, DAC_GAIN_2X, DAC1_cmd);
        return;
    }

    // 4. 计算VOUTP
    float VOUTP_target = (I_set * Rload_disp) + (I_set * Rs) + 1.2f;

    // 5. 通过板载硬件传递函数，将物理电压逆向转换为 DAC1 的控制量
    // 公式源自物理校准：VOUT+ = 10.012879 * DAC1 - 1.797554
    DAC1_cmd = (VOUTP_target + 1.797554f) / 10.012879f;

    // 6. 强制硬件红线边界拦截 (依据你提供的 3.3V 极限安全边界钳位)
    if (DAC1_cmd < 0.0f) {
        DAC1_cmd = 0.0f;
    }
    else if (DAC1_cmd >= 3.3f) {
        DAC1_cmd = 3.3f;
    }

    // 7. 物理刷新硬件外设
    DAC_Set_Voltage(DAC_CHANNEL_1, DAC_GAIN_2X, DAC1_cmd);
}
