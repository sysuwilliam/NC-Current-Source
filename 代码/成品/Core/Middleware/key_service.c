//
// Created by lin on 2026/6/2.
//

#include "key_service.h"
#include "button.h"  // 包含按键中间件接口
#include "main.h"
#include "dsp_DAC.h"
#include "tim.h"
#include "global_value.h"

//设置步进
float I_STEP = 0.001f;
static const float sg_I_step_table[3] = {0.001f, 0.01f, 0.1f};
static uint8_t sg_step_index = 1;

// uint8_t DAC_CHANNEL_FLAG = SW_DAC_CHANNEL_1;

static uint16_t last_encoder_cnt = 0;

/* ======================================================================== */
/* ============================== 按键控制函数 ============================== */
/* ======================================================================== */

static void KEY1_Action(void)
{
    if (Safe_flag==TJC_PROTECT_IDLE){Safe_flag=TJC_PROTECT_NORMAL;}
    else {Safe_flag=TJC_PROTECT_IDLE;}
}

static void KEY2_Action(void)
{

}

static void KEY3_Action(void)
{
    // KEY3：调节步进
    sg_step_index++;
    if (sg_step_index >= 3) {
        sg_step_index = 0;
    }
    I_STEP = sg_I_step_table[sg_step_index];
}

static void ENC_SW_Action(void)
{
    // 编码器按键业务
    // DAC_CHANNEL_FLAG=!DAC_CHANNEL_FLAG;
}


/**
 * @brief  应用层业务总初始化
 * @note   在这里完成“物理驱动”与“上层业务逻辑”的纽带连接
 */
void KEY_Init(void)
{
    // 按键初始化
    Button_Init();

    /* -----------------------------------------------------------------
     * 动态注册，将此处的业务函数挂载到按键上
     * ----------------------------------------------------------------*/
    Button_RegisterCallback(KEY_INDEX_1, KEY1_Action,  NULL);
    Button_RegisterCallback(KEY_INDEX_2, KEY2_Action,   NULL);
    Button_RegisterCallback(KEY_INDEX_3, KEY3_Action, NULL);
    Button_RegisterCallback(ENC_SW, ENC_SW_Action,  NULL);
}

/**
 * @brief  旋转编码器控制 (仅控制 I_Set)
 * @note   配合定时器正交解码模式，在 main.c 的 while(1) 中高频非阻塞轮询
 */
void Encoder_Process(void) {
    // 1. 读取硬件计数器原始无符号值 (0 - 65535)
    uint16_t current_encoder_cnt = __HAL_TIM_GET_COUNTER(&htim4);

    // 2. 利用无符号差分回绕特性，计算出纯粹的物理点位变化
    int16_t delta = (int16_t) (current_encoder_cnt - last_encoder_cnt);

    // 只有当物理旋钮发生旋转时，才触发核心控制链路
    if (delta != 0) {
        //边界检测
        if (delta > 50 || delta < -50) {
            last_encoder_cnt = current_encoder_cnt; // 强行拉齐账本，抛弃这一帧脏数据
            return;
        }

        // 账本基础步进安全拉齐
        last_encoder_cnt = current_encoder_cnt;

        I_set -= (float) delta * I_STEP;

        // 设置电流在 0.0A ~ 0.6A 之间
        if (I_set < 0.0f) {
            I_set = 0.0f;
        } else if (I_set > 0.6f) {
            I_set = 0.6f;
        }
    }
}



