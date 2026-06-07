/**
 * Created by sysuwilliam on 2026/6/3.
 *
 * 陶晶驰串口屏驱动库
 *
 * 功能：
 * 1. 封装陶晶驰串口屏指令发送格式：ASCII 指令 + 0xff 0xff 0xff 结束符。
 * 2. 按当前 HMI 工程中的控件编号，提供高层显示更新函数。
 * 3. 默认使用 USART2 与串口屏通信，也可通过 TJC_HMI_Init() 指定其他串口。
 *
 * 当前页面控件对应关系：
 * t6  工作模式文本        n0  设定电流，单位 mA，整数
 * t7  输出状态文本        x0  实际电流，单位 mA，保留 2 位小数
 * t8  保护状态文本        x1  负载电阻，单位 Ohm，保留 2 位小数
 *                        x2  负载电压，单位 V，保留 2 位小数
 *                        x3  负载功率，单位 W，保留 2 位小数
 *                        n1  过流阈值，单位 mA，整数
 *
 * 报警声音：
 * 当保护状态写入“过流 / 短路限流 / 开路报警”时，本库会发送 beep 指令让屏幕蜂鸣一次。
 * TJC_SetProtectState() 内部带状态变化判断，避免 TJC_UpdateAll() 周期刷新时反复蜂鸣。
 * 保护状态文本颜色会同步变化：正常为黑色，非正常为红色。
 *
 * 注意：
 * 串口屏中的 x0/x1/x2/x3 是“虚拟浮点数”控件，本库给它们发送的是放大
 * 100 倍后的整数值。例如显示 12.34，就发送 val=1234。
 */

#ifndef TEST_TJC_HMI_H
#define TEST_TJC_HMI_H

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    TJC_OUTPUT_OFF = 0,
    TJC_OUTPUT_ON,
    TJC_OUTPUT_FAULT
} TJC_OutputState_t;

typedef enum
{
    TJC_PROTECT_NORMAL = 0,
    TJC_PROTECT_OVERCURRENT,
    TJC_PROTECT_SHORT_LIMIT,
    TJC_PROTECT_OPEN_LOAD
} TJC_ProtectState_t;

typedef struct
{
    int32_t set_current_mA;       // n0：设定电流，整数 mA
    int32_t actual_current_uA;    // x0：实际电流，输入单位 uA，屏幕显示 mA
    int32_t load_resistance_mOhm; // x1：负载电阻，输入单位 mOhm，屏幕显示 Ohm
    int32_t load_voltage_mV;      // x2：负载电压，输入单位 mV，屏幕显示 V
    int32_t load_power_mW;        // x3：负载功率，输入单位 mW，屏幕显示 W
    int32_t overcurrent_mA;       // n1：过流阈值，整数 mA
    TJC_OutputState_t output_state;
    TJC_ProtectState_t protect_state;
} TJC_HmiData_t;

/**
 * @brief  初始化陶晶驰串口屏驱动，并向屏幕写入默认显示值。
 * @param  huart 串口句柄；传 NULL 时保持默认 USART2。
 */
void TJC_HMI_Init(UART_HandleTypeDef *huart);

/**
 * @brief  发送一条陶晶驰原始指令，并自动追加 0xff 0xff 0xff。
 * @param  cmd 不包含结束符的 ASCII 指令字符串。
 * @retval HAL 状态码。
 */
HAL_StatusTypeDef TJC_SendCmd(const char *cmd);

/**
 * @brief  设置文本控件的 txt 属性。
 * @param  obj 控件名，例如 "t6"。
 * @param  txt 要显示的文本内容。
 * @retval HAL 状态码。
 */
HAL_StatusTypeDef TJC_SetText(const char *obj, const char *txt);

/**
 * @brief  设置数字控件或虚拟浮点数控件的 val 属性。
 * @param  obj 控件名，例如 "n0"。
 * @param  value 要写入的整数值。
 * @retval HAL 状态码。
 */
HAL_StatusTypeDef TJC_SetInt(const char *obj, int32_t value);

/**
 * @brief  设置虚拟浮点数控件的原始整数值。
 * @param  obj 控件名，例如 "x0"。
 * @param  value 已按控件小数位放大的整数值。
 * @retval HAL 状态码。
 */
HAL_StatusTypeDef TJC_SetXFloatRaw(const char *obj, int32_t value);

/**
 * @brief  设置控件前景色 pco 属性。
 * @param  obj 控件名，例如 "t8"。
 * @param  color RGB565 颜色值，例如黑色 0，红色 63488。
 * @retval HAL 状态码。
 */
HAL_StatusTypeDef TJC_SetColor(const char *obj, uint16_t color);

/**
 * @brief  跳转到指定页面。
 * @param  page_name 页面名或页面 ID 字符串。
 * @retval HAL 状态码。
 */
HAL_StatusTypeDef TJC_Page(const char *page_name);

/**
 * @brief  控制串口屏蜂鸣器响指定时间。
 * @param  time_ms 蜂鸣时间，单位 ms。
 * @retval HAL 状态码。
 */
HAL_StatusTypeDef TJC_Beep(uint16_t time_ms);

/**
 * @brief  设置工作模式显示为“恒流模式”。
 */
void TJC_SetWorkModeConstCurrent(void);

/**
 * @brief  更新输出状态显示。
 * @param  state 输出状态枚举值。
 */
void TJC_SetOutputState(TJC_OutputState_t state);

/**
 * @brief  更新保护状态显示，并在故障状态变化时触发蜂鸣报警。
 * @param  state 保护状态枚举值。
 */
void TJC_SetProtectState(TJC_ProtectState_t state);

/**
 * @brief  更新设定电流显示。
 * @param  current_mA 设定电流，单位 mA，范围限制为 0~500。
 */
void TJC_SetSetCurrent_mA(int32_t current_mA);

/**
 * @brief  更新实际电流显示。
 * @param  current_uA 实际电流，单位 uA，屏幕显示为 mA 并保留 2 位小数。
 */
void TJC_SetActualCurrent_uA(int32_t current_uA);

/**
 * @brief  更新负载电阻显示。
 * @param  resistance_mOhm 负载电阻，单位 mOhm，屏幕显示为 Ohm 并保留 2 位小数。
 */
void TJC_SetLoadResistance_mOhm(int32_t resistance_mOhm);

/**
 * @brief  更新负载电压显示。
 * @param  voltage_mV 负载电压，单位 mV，屏幕显示为 V 并保留 2 位小数。
 */
void TJC_SetLoadVoltage_mV(int32_t voltage_mV);

/**
 * @brief  更新负载功率显示。
 * @param  power_mW 负载功率，单位 mW，屏幕显示为 W 并保留 2 位小数。
 */
void TJC_SetLoadPower_mW(int32_t power_mW);

/**
 * @brief  更新过流阈值显示。
 * @param  threshold_mA 过流阈值，单位 mA。
 */
void TJC_SetOvercurrentThreshold_mA(int32_t threshold_mA);

/**
 * @brief  一次性刷新主界面的所有动态显示量。
 * @param  data 主界面显示数据结构指针。
 */
void TJC_UpdateAll(const TJC_HmiData_t *data);

void TJC_DAC_CH1(int32_t DAC_CH1_mV);
void TJC_DAC_CH2(int32_t DAC_CH2_mV);

#ifdef __cplusplus
}
#endif

#endif // TEST_TJC_HMI_H
