//
// Created by lin on 2026/6/2.
//

#include "key_service.h"
#include "button.h"  // 包含按键中间件接口
#include "main.h"
#include "dsp_DAC.h"
#include "tim.h"
#include "global_value.h"

/* 搬迁过来的物理账本变量 */
float I_STEP = 0.01f;
static const float sg_I_step_table[3] = {0.001f, 0.01f, 0.1f};
static uint8_t sg_step_index = 1; // 默认 0.01f
uint8_t DAC_CHANNEL_FLAG = SW_DAC_CHANNEL_1;

static uint16_t last_encoder_cnt = 0;

/* ======================================================================== */
/* ======= 应用层业务私有函数（在此处可以随意改写、增加控制算法）======= */
/* ======================================================================== */

static void KEY1_Action(void)
{
    // KEY1：切换电源开关与指示灯
    HAL_GPIO_TogglePin(BUCK_EN_GPIO_Port, BUCK_EN_Pin);
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    if (Safe_flag!=0) {
    Safe_flag = 0; // 恢复安全标志位为0，表示系统状态恢复安全
    }
}

static void KEY2_Action(void)
{
    // KEY2：环形切换步进
    // sg_step_index++;
    // if (sg_step_index >= 4) {
    //     sg_step_index = 0;
    // }
    // VOL_STEP = sg_vol_step_table[sg_step_index];
}

static void KEY3_Action(void)
{
    // KEY3：紧急安全关断
    sg_step_index++;
    if (sg_step_index >= 3) {
        sg_step_index = 0;
    }
    I_STEP = sg_I_step_table[sg_step_index];
}

static void ENC_SW_Action(void)
{
    // 编码器按键业务
    DAC_CHANNEL_FLAG=!DAC_CHANNEL_FLAG;
}


/**
 * @brief  应用层业务总初始化
 * @note   在这里完成“物理驱动”与“上层业务逻辑”的纽带连接
 */
void KEY_Init(void)
{
    // 1. 先呼叫底层驱动进行引脚配置
    Button_Init();

    /* -----------------------------------------------------------------
     * 2. 通过动态注册将此处的业务函数挂载到按键上（核心对齐现场）
     * ----------------------------------------------------------------*/
    Button_RegisterCallback(KEY_INDEX_1, KEY1_Action,  NULL);
    Button_RegisterCallback(KEY_INDEX_2, KEY2_Action,   NULL);
    Button_RegisterCallback(KEY_INDEX_3, KEY3_Action, NULL);
    Button_RegisterCallback(ENC_SW, ENC_SW_Action,  NULL);
}

/**
 * @brief  旋转编码器解耦控制引擎 (仅控制 I_Set 并反向推导硬件)
 * @note   配合定时器正交解码模式，在 main.c 的 while(1) 中高频非阻塞轮询
 */
void Encoder_Process(void) {
    // 1. 读取硬件计数器原始无符号值 (0 - 65535)
    uint16_t current_encoder_cnt = __HAL_TIM_GET_COUNTER(&htim4);

    // 2. 利用无符号差分回绕特性，计算出纯粹的物理点位变化
    int16_t delta = (int16_t)(current_encoder_cnt - last_encoder_cnt);

    // 只有当物理旋钮发生旋转时，才触发核心控制链路
    if (delta != 0) {

        /* ⚡【工业防线 1】：异常步进拦截滤波
         * 手拧编码器在快速反转时，正常的 delta 绝对不会超过 ±50。
         * 如果因为硬件抖动或定时器回绕错位算出一个几千的巨变值，直接乱丢不要，防止 I_set 瞬间崩塌。
         */
        if (delta > 50 || delta < -50) {
            last_encoder_cnt = current_encoder_cnt; // 强行拉齐账本，抛弃这一帧脏数据
            return;
        }

        // 账本基础步进安全拉齐
        last_encoder_cnt = current_encoder_cnt;

        switch (DAC_CHANNEL_FLAG)
        {
            case SW_DAC_CHANNEL_1:
                // 目前为空，直接退出，但账本已被上方安全更新
                break;

            case SW_DAC_CHANNEL_2:
                /* ⚡【工业防线 2】：严格单向步进限制
                 * 依据你的工程设定：顺时针(delta > 0)纯加，逆时针(delta < 0)纯减。
                 * 注意：原代码写的是 "-="，会导致正转变减。此处将其改为标准的符合逻辑的调节。
                 * 如果你希望顺时针加，请用 "+="；如果是顺时针减，请保持 "-="。
                 * * 这里我们用标准的单步限定，防止突变量过大。
                 */
                I_set -= (float)delta * I_STEP;

                // 【边界纠偏】：硬件最大功率红线锁死在 0.0A ~ 0.6A 之间
                if (I_set < 0.0f) {
                    I_set = 0.0f;
                } else if (I_set > 0.6f) {
                    I_set = 0.6f;
                }

                // 【核心纠偏机制】：单位强制对齐
                float I_set_mA = I_set * 1000.0f;

                // 多项式反推出 dac2_target_mv 毫伏值
                float dac2_target_mv = (I_set_mA - 1.368386f) / 0.498188f;

                // 硬件动态防呆
                if (dac2_target_mv < 0.0f) {
                    dac2_target_mv = 0.0f;
                } else if (dac2_target_mv > 1400.0f) {
                    dac2_target_mv = 1400.0f; // 限制在 1400mV 硬件安全电压内
                }

                /* 物理落地：刷新硬件寄存器 (如果需要实时更新请解除此处的注释) */
                // DAC2_cmd = dac2_target_mv / 1000.0f;
                // DAC_Set_Voltage(DAC_CHANNEL_2, DAC_GAIN_1X, DAC2_cmd);
                break;

            default:
                break;
        }
    }
}

// void DAC_SW_CH(void){
//     if (HAL_GPIO_ReadPin(ENC_SW_GPIO_Port, ENC_SW_Pin) == GPIO_PIN_RESET) {
//         HAL_Delay(20);
//         if (HAL_GPIO_ReadPin(ENC_SW_GPIO_Port, ENC_SW_Pin) == GPIO_PIN_RESET) {
//             DAC_CHANNEL_FLAG=!DAC_CHANNEL_FLAG;
//             //HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
//             while (HAL_GPIO_ReadPin(ENC_SW_GPIO_Port, ENC_SW_Pin) == GPIO_PIN_RESET);
//             HAL_Delay(20);
//         }
//     }
// }


