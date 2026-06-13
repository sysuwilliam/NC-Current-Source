//
// Created by lin on 2026/6/4.
//

#ifndef CI_SOURCE_FINISHED_PID_H
#define CI_SOURCE_FINISHED_PID_H

#include "main.h"
// PID 参数与控制状态结构体
typedef struct {
    float Kp;               // 比例系数
    float Ki;               // 积分系数
    float Kd;               // 微分系数

    int32_t target;         // 控制目标值 ( I_Set, 单位 mA)
    int32_t actual;         // 当前反馈真实值 ( I_disp, 单位 mA)

    int32_t err_curr;       // 当前采样周期的误差 E(n)
    int32_t err_last1;       // 上一个采样周期的误差 E(n-1)
    int32_t err_last2;       // 上上一个采样周期的误差 E(n-2)

    float out_delta;        // 本次 PID 计算出的控制量增量
} PID_Controller_t;


/* ======================================================================== */
/* ======= 电流环 PID 精度与控制契约宏定义 (工业标准规范)                  ======= */
/* ======================================================================== */

/** * @brief 电流控制标度扩展倍数 (Current Scaling Factor)
 * @note  若控制精度为 0.5mA，则 1A 对应 2000 个定点字 (1.0f / 0.0005f = 2000.0f)
 */
#define PID_CURRENT_SCALE       2000.0f
#define PID_VOUTP_SCALE         1000.0f

/** * @brief 工业级闭环控制死区阈值 (单位: 对应扩展后的定点字)
 * @note  物理死区设为 1mA。在 0.5mA/字的标度下：1mA / 0.5mA = 2 个字
 */
#define PID_CURRENT_DEADBAND    2
#define PID_VOUTP_DEADBAND      2


// extern PID_Controller_t pid_current;

// 函数声明
void PID_Init(void);
void PID_Current_Loop(void);
void Filter_Output(void);
void PID_BUCK_Loop(void);
void BUCK_Loop(void);


#endif //CI_SOURCE_FINISHED_PID_H