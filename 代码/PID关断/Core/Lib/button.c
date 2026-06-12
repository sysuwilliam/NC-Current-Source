//
// Created by lin on 2026/6/2.
//

#include "button.h"
#include "main.h"
#include <stddef.h>

// 实体化物理按键对象数组（对象实例化）
static Button_t g_buttons[BTN_NUM];

/**
 * @brief  按键架构初始化，绑定物理硬件与上层业务 (纠偏规整版)
 * @note   严格遵循“先全局清零，后局部实例化”的工程 conventions，消除指针覆盖死锁
 */
void Button_Init(void)
{
    /*-----------------------------------------------------------------
     * 1. 标准习惯法：必须先在入口处，对整个物理账本进行彻底的初值安全留白
     *----------------------------------------------------------------*/
    for (uint8_t i = 0; i < BTN_NUM; i++)
    {
        g_buttons[i].GPIOx       = NULL;
        g_buttons[i].GPIO_Pin     = 0;
        g_buttons[i].state        = BTN_STATE_IDLE;
        g_buttons[i].down_tick    = 0;
        g_buttons[i].OnPress      = NULL;
        g_buttons[i].OnLongPress  = NULL;
        g_buttons[i].OnRelease    = NULL;
    }

    /*-----------------------------------------------------------------
     * 2. 局部落地：在安全的白纸上，精准雕刻各个按键的物理硬件外设映射
     *----------------------------------------------------------------*/
    // 配置 KEY 1
    g_buttons[KEY_INDEX_1].GPIOx     = KEY_1_GPIO_Port;
    g_buttons[KEY_INDEX_1].GPIO_Pin   = KEY_1_Pin;

    // 配置 KEY 2
    g_buttons[KEY_INDEX_2].GPIOx     = KEY_2_GPIO_Port;
    g_buttons[KEY_INDEX_2].GPIO_Pin   = KEY_2_Pin;

    // 配置 KEY 3
    g_buttons[KEY_INDEX_3].GPIOx     = KEY_3_GPIO_Port;
    g_buttons[KEY_INDEX_3].GPIO_Pin   = KEY_3_Pin;

    // 配置 编码器按键 ENC_SW
    g_buttons[ENC_SW].GPIOx          = ENC_SW_GPIO_Port;
    g_buttons[ENC_SW].GPIO_Pin       = ENC_SW_Pin;
}

/**
 * @brief  统一的外部中断分发器
 * @note   在 stm32f1xx_it.c 的 HAL_GPIO_EXTI_Callback 中调用此函数
 */
void Button_EXTI_Callback(uint16_t GPIO_Pin)
{
    for (uint8_t i = 0; i < BTN_NUM; i++)
    {
        if (GPIO_Pin == g_buttons[i].GPIO_Pin)
        {
            // 只有当按键真正处于空闲时，才捕获边缘，防止机械抖动频繁进入
            if (g_buttons[i].state == BTN_STATE_IDLE)
            {
                g_buttons[i].down_tick = HAL_GetTick();
                g_buttons[i].state     = BTN_STATE_DOWN; // 激活主循环状态机
            }
            break;
        }
    }
}

/**
 * @brief  解耦型单按键时间戳长短按判定引擎
 * @note   参考示例时序：手松开的瞬间，依据物理总耗时进行多级区间拦截
 */
static void Button_Single_Engine(Button_t *btn)
{
    switch (btn->state)
    {
        case BTN_STATE_IDLE:
            break;

        case BTN_STATE_DOWN:
            // 前沿消抖：确认 20ms 后引脚依然保持稳定压低
            if ((HAL_GetTick() - btn->down_tick) >= 20)
            {
                if (HAL_GPIO_ReadPin(btn->GPIOx, btn->GPIO_Pin) == GPIO_PIN_RESET)
                {
                    // 确认是真实按下，直接进入挂起态，等待后续释放
                    btn->state = BTN_STATE_UP;
                }
                else
                {
                    // 环境毛刺，无条件复位
                    btn->state = BTN_STATE_IDLE;
                }
            }
            break;

        case BTN_STATE_UP:
            // 对应示例中的 case 1: 判定引脚何时恢复为高电平 (Bit_SET 说明手已释放)
            if (HAL_GPIO_ReadPin(btn->GPIOx, btn->GPIO_Pin) == GPIO_PIN_SET)
            {
                // 计算从按下到松手的总计物理跨度（相当于示例中 Timeout 检查的基准值）
                uint32_t duration = HAL_GetTick() - btn->down_tick;

                /* ======= 核心功能区：长/短按多层区间防御拦截 ======= */
                if (duration >= 2000)
                {
                    // 1. 区间一：物理按下超过 2000ms (2秒)，无条件判定为【长按】
                    if (btn->OnLongPress != NULL)
                    {
                        btn->OnLongPress();
                    }
                }
                else if (duration >= 20)
                {
                    // 2. 区间二：耗时在 20ms ~ 2000ms 之间，无条件判定为【短按】
                    if (btn->OnPress != NULL)
                    {
                        btn->OnPress();
                    }
                }
                // 如果小于 20ms 则落入示例中的消抖拦截线，被视作无效抖动直接丢弃

                // 统一执行释放动作（若上层有注册）
                if (btn->OnRelease != NULL)
                {
                    btn->OnRelease();
                }

                // 3. 【原子动作】：状态机彻底解开，清除松手产生的后沿机械毛刺
                btn->state = BTN_STATE_IDLE;
                __HAL_GPIO_EXTI_CLEAR_IT(btn->GPIO_Pin);
            }
            break;

        default:
            btn->state = BTN_STATE_IDLE;
            break;
    }
}

/**
 * @brief  多路按键轮询总引擎
 * @note   在 main.c 的 while(1) 中调用，遍历执行
 */
void Button_Process(void)
{
    for (uint8_t i = 0; i < BTN_NUM; i++)
    {
        Button_Single_Engine(&g_buttons[i]);
    }
}

/**
 * @brief  【动态解耦核心】动态注册业务函数接口
 * @note   供外部任意业务文件调用，将具体的业务逻辑挂载到对应的物理按键上
 */
void Button_RegisterCallback(ButtonIndex_t index, ButtonCallback_t onPress, ButtonCallback_t onLongPress)
{
    if (index < BTN_NUM) {
        g_buttons[index].OnPress     = onPress;
        g_buttons[index].OnLongPress = onLongPress;
    }
}