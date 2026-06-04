//
// Created by lin on 2026/5/31.
//

#include "PID.h"
#include "DSP_ADC.h" // 确保能读取到你上一轮写好的 I_actual
#include <stdlib.h>
#include "DAC.h"

// 实例化全局变量
PID_Controller_t pid_current;


/**
 * @brief  PID 参数初始化
 */
void BUCK_PID_Init(void)
{
    // ⚠️ 这里的参数需要你在实际电路中通过微调（试凑法）决定
    pid_current.Kp = 0.001f;   // 比例放大系数
    pid_current.Ki = 0.0005f;  // 积分累加系数
    pid_current.Kd = 0.0f;     // 微分控制系数（电源环路一般不加Kd，设为0即可）

    pid_current.target = I_target;
    pid_current.actual = 0;

    pid_current.err_curr = 0;
    pid_current.err_next = 0;
    pid_current.err_last = 0;
    pid_current.out_delta = 0.0f;
}

/**
 * @brief  电流环 PID 增量式核心计算
 * @note   不能在中断中做复杂浮点运算，在 main.c 的 while(1) 中由定时器标记或软件周期性触发
 */
void BUCK_PID_Current_Loop(void)
{
    // 1. 同步当前最新的全局真相（目标与反馈）
    pid_current.target = I_target;
    pid_current.actual = I_actual; // 来自 DSP_ADC.c 过滤后的平稳电流

    // 2. 计算当前误差 E(n)
    pid_current.err_curr = pid_current.target - pid_current.actual;

    /* ======= 3. 工业级死区控制（防微幅震荡干扰） ======= */
    if (abs(pid_current.err_curr) < 3) // 误差小于 3mA 时不动作
    {
        pid_current.err_curr = 0;
    }

    /* ======= 4. 标准增量式 PID 数学模型推演 ======= */
    // ΔU = Kp * [E(n) - E(n-1)] + Ki * E(n) + Kd * [E(n) - 2*E(n-1) + E(n-2)]
    pid_current.out_delta = (pid_current.Kp * (float)(pid_current.err_curr - pid_current.err_next)) +
                            (pid_current.Ki * (float)pid_current.err_curr) +
                            (pid_current.Kd * (float)(pid_current.err_curr - 2 * pid_current.err_next + pid_current.err_last));

    /* ======= 5. 作用于外设：改变 DAC2 目标电压 ======= */
    // 将 PID 算出的物理增量直接叠加到 DAC 期望值上
    DAC2_Target_voltage += pid_current.out_delta;

    /* ======= 6. 强制纠偏：硬件防御边界拦截（红线保护） ======= */
    if (DAC2_Target_voltage <= 0.0f)
    {
        DAC2_Target_voltage = 0.0f;
    }
    else if (DAC2_Target_voltage >= 1.4f)
    {
        DAC2_Target_voltage = 1.4f; // 锁定硬件安全天花板，防止控制过冲烧毁后级
    }

    // 7. 物理刷新硬件，使新的 DAC 输出生效
    DAC_Set_Voltage(DAC_CHANNEL_2, DAC_GAIN_1X, DAC2_Target_voltage);

    // 8. 滚动机理：状态历史记录前移，为下一个控制周期做准备
    pid_current.err_last = pid_current.err_next;
    pid_current.err_next = pid_current.err_curr;
}