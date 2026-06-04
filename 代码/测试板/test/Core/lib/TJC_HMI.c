/**
 * Created by sysuwilliam on 2026/6/3.
 *
 * 陶晶驰串口屏驱动库实现
 *
 * 本文件只负责“屏幕显示层”的事情：
 * - 底层：通过 UART 发送陶晶驰指令，并自动追加 0xff 0xff 0xff。
 * - 中层：提供设置文本控件、数字控件、虚拟浮点数控件的通用函数。
 * - 上层：按当前页面控件编号封装恒流源主界面的显示更新函数。
 *
 * 单位换算约定：
 * - 实际电流输入 uA，屏幕 x0 显示 mA，保留 2 位小数。
 * - 负载电阻输入 mOhm，屏幕 x1 显示 Ohm，保留 2 位小数。
 * - 负载电压输入 mV，屏幕 x2 显示 V，保留 2 位小数。
 * - 负载功率输入 mW，屏幕 x3 显示 W，保留 2 位小数。
 * - 电流波形 s0 通道 0 显示设定电流，通道 1 显示实际电流。
 *   波形按 0~500 mA 映射到曲线控件 0~255 的数据范围。
 *
 * 报警声音：
 * 当保护状态切换到“过流 / 短路限流 / 开路报警”时，本库会发送 beep 指令。
 * 注意 TJC_UpdateAll() 如果周期调用很多次，蜂鸣只会在保护状态变化时触发一次。
 * 保护状态文本颜色会同步变化：正常为黑色，非正常为红色。
 *
 * 例如：TJC_SetActualCurrent_uA(123450) 会向 x0 发送 12345，
 * 若 x0 设置为 2 位小数，则屏幕显示为 123.45 mA。
 */

#include "TJC_HMI.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

#define TJC_CMD_BUF_LEN 64
#define TJC_TX_TIMEOUT_MS 50

// 当前 HMI 页面中的动态控件编号。
// 若后续在 USART HMI 工具中改了控件名，只需要改这里。
#define TJC_OBJ_WORK_MODE       "t6"
#define TJC_OBJ_OUTPUT_STATE    "t7"
#define TJC_OBJ_PROTECT_STATE   "t8"
#define TJC_OBJ_SET_CURRENT     "n0"
#define TJC_OBJ_ACTUAL_CURRENT  "x0"
#define TJC_OBJ_LOAD_RESISTANCE "x1"
#define TJC_OBJ_LOAD_VOLTAGE    "x2"
#define TJC_OBJ_LOAD_POWER      "x3"
#define TJC_OBJ_OVERCURRENT     "n1"
#define TJC_OBJ_CURRENT_WAVE    "s0"

#define TJC_BEEP_SHORT_LIMIT_MS 200
#define TJC_BEEP_OPEN_LOAD_MS   400
#define TJC_BEEP_OVERCURRENT_MS 600

#define TJC_COLOR_BLACK 0
#define TJC_COLOR_RED   63488

#define TJC_CURRENT_WAVE_SET_CH    0
#define TJC_CURRENT_WAVE_ACTUAL_CH 1
#define TJC_CURRENT_WAVE_MAX_MA    500
#define TJC_WAVE_VALUE_MAX         255

// 默认使用 USART2 连接串口屏：PA2(TX) -> 屏幕 RX，PA3(RX) -> 屏幕 TX。
static UART_HandleTypeDef *tjc_uart = &huart2;
static TJC_ProtectState_t tjc_last_protect_state = TJC_PROTECT_NORMAL;


/******************** 底层函数：UART 发送与陶晶驰指令结束符 ********************/

/**
 * @brief  通过当前串口发送原始字节数据。
 * @param  data 数据缓冲区指针。
 * @param  len  要发送的字节数。
 * @retval HAL 状态码。
 */
static HAL_StatusTypeDef TJC_WriteBytes(const uint8_t *data, uint16_t len)
{
    if (tjc_uart == NULL || data == NULL || len == 0) {
        return HAL_ERROR;
    }
    return HAL_UART_Transmit(tjc_uart, (uint8_t *)data, len, TJC_TX_TIMEOUT_MS);
}

/**
 * @brief  发送陶晶驰指令结束符 0xff 0xff 0xff。
 * @retval HAL 状态码。
 */
static HAL_StatusTypeDef TJC_SendEnd(void)
{
    static const uint8_t end_bytes[3] = {0xff, 0xff, 0xff};
    return TJC_WriteBytes(end_bytes, sizeof(end_bytes));
}

/**
 * @brief  发送一条陶晶驰原始指令，并自动追加 0xff 0xff 0xff。
 * @param  cmd 不包含结束符的 ASCII 指令字符串。
 * @retval HAL 状态码。
 */
HAL_StatusTypeDef TJC_SendCmd(const char *cmd)
{
    HAL_StatusTypeDef status;

    if (cmd == NULL) {
        return HAL_ERROR;
    }

    // 先发送 ASCII 指令正文，再补陶晶驰结束符。
    status = TJC_WriteBytes((const uint8_t *)cmd, (uint16_t)strlen(cmd));
    if (status != HAL_OK) {
        return status;
    }

    return TJC_SendEnd();
}

/**
 * @brief  将 int32_t 数值限制在指定范围内。
 * @param  value     输入值。
 * @param  min_value 最小允许值。
 * @param  max_value 最大允许值。
 * @retval 限幅后的值。
 */
static int32_t TJC_ClampI32(int32_t value, int32_t min_value, int32_t max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

/**
 * @brief  对整数除法做四舍五入。
 * @param  numerator   分子。
 * @param  denominator 分母。
 * @retval 四舍五入后的整数结果。
 */
static int32_t TJC_RoundDivI32(int64_t numerator, int32_t denominator)
{
    if (denominator <= 0) {
        return 0;
    }
    if (numerator >= 0) {
        return (int32_t)((numerator + denominator / 2) / denominator);
    }
    return (int32_t)((numerator - denominator / 2) / denominator);
}

/**
 * @brief  将电流值映射到曲线控件的数据范围 0~255。
 * @param  current_uA 电流，单位 uA。
 * @retval 曲线控件点值。
 */
static uint8_t TJC_CurrentToWaveValue_uA(int32_t current_uA)
{
    int32_t value;

    current_uA = TJC_ClampI32(current_uA, 0, TJC_CURRENT_WAVE_MAX_MA * 1000);
    value = TJC_RoundDivI32((int64_t)current_uA * TJC_WAVE_VALUE_MAX,
                            TJC_CURRENT_WAVE_MAX_MA * 1000);
    value = TJC_ClampI32(value, 0, TJC_WAVE_VALUE_MAX);
    return (uint8_t)value;
}

/**
 * @brief  将输出状态枚举转换成屏幕显示文本。
 * @param  state 输出状态枚举值。
 * @retval 对应的中文状态字符串。
 */
static const char *TJC_OutputStateText(TJC_OutputState_t state)
{
    switch (state) {
        case TJC_OUTPUT_ON:
            return "输出中";
        case TJC_OUTPUT_FAULT:
            return "故障";
        case TJC_OUTPUT_OFF:
        default:
            return "关闭";
    }
}

/**
 * @brief  将保护状态枚举转换成屏幕显示文本。
 * @param  state 保护状态枚举值。
 * @retval 对应的中文状态字符串。
 */
static const char *TJC_ProtectStateText(TJC_ProtectState_t state)
{
    switch (state) {
        case TJC_PROTECT_OVERCURRENT:
            return "过流";
        case TJC_PROTECT_SHORT_LIMIT:
            return "短路限流";
        case TJC_PROTECT_OPEN_LOAD:
            return "开路报警";
        case TJC_PROTECT_NORMAL:
        default:
            return "正常";
    }
}


/******************** 中层函数：通用控件属性写入 ********************/

/**
 * @brief  设置文本控件的 txt 属性。
 * @param  obj 控件名，例如 "t6"。
 * @param  txt 要显示的文本内容。
 * @retval HAL 状态码。
 */
HAL_StatusTypeDef TJC_SetText(const char *obj, const char *txt)
{
    char cmd[TJC_CMD_BUF_LEN];

    if (obj == NULL || txt == NULL) {
        return HAL_ERROR;
    }

    // 文本控件格式：控件.txt="内容"
    snprintf(cmd, sizeof(cmd), "%s.txt=\"%s\"", obj, txt);
    return TJC_SendCmd(cmd);
}

/**
 * @brief  设置数字控件或虚拟浮点数控件的 val 属性。
 * @param  obj 控件名，例如 "n0"。
 * @param  value 要写入的整数值。
 * @retval HAL 状态码。
 */
HAL_StatusTypeDef TJC_SetInt(const char *obj, int32_t value)
{
    char cmd[TJC_CMD_BUF_LEN];

    if (obj == NULL) {
        return HAL_ERROR;
    }

    // 数字控件和虚拟浮点数控件都使用 val 属性。
    snprintf(cmd, sizeof(cmd), "%s.val=%ld", obj, (long)value);
    return TJC_SendCmd(cmd);
}

/**
 * @brief  设置虚拟浮点数控件的原始整数值。
 * @param  obj 控件名，例如 "x0"。
 * @param  value 已按控件小数位放大的整数值。
 * @retval HAL 状态码。
 */
HAL_StatusTypeDef TJC_SetXFloatRaw(const char *obj, int32_t value)
{
    // 虚拟浮点数控件的“小数点位置”由 HMI 工程属性决定。
    // 本函数只负责发送已经按小数位放大的整数。
    return TJC_SetInt(obj, value);
}

/**
 * @brief  设置控件前景色 pco 属性。
 * @param  obj 控件名，例如 "t8"。
 * @param  color RGB565 颜色值，例如黑色 0，红色 63488。
 * @retval HAL 状态码。
 */
HAL_StatusTypeDef TJC_SetColor(const char *obj, uint16_t color)
{
    char cmd[TJC_CMD_BUF_LEN];

    if (obj == NULL) {
        return HAL_ERROR;
    }

    // 文本/数字控件前景色格式：控件.pco=颜色值
    snprintf(cmd, sizeof(cmd), "%s.pco=%u", obj, (unsigned int)color);
    return TJC_SendCmd(cmd);
}

/**
 * @brief  跳转到指定页面。
 * @param  page_name 页面名或页面 ID 字符串。
 * @retval HAL 状态码。
 */
HAL_StatusTypeDef TJC_Page(const char *page_name)
{
    char cmd[TJC_CMD_BUF_LEN];

    if (page_name == NULL) {
        return HAL_ERROR;
    }

    // 页面跳转格式：page 页面名。正式工程优先用页面名，少用页面 ID。
    snprintf(cmd, sizeof(cmd), "page %s", page_name);
    return TJC_SendCmd(cmd);
}

/**
 * @brief  控制串口屏蜂鸣器响指定时间。
 * @param  time_ms 蜂鸣时间，单位 ms。
 * @retval HAL 状态码。
 */
HAL_StatusTypeDef TJC_Beep(uint16_t time_ms)
{
    char cmd[TJC_CMD_BUF_LEN];

    // 手册指令格式：beep time，time 单位为 ms。
    snprintf(cmd, sizeof(cmd), "beep %u", (unsigned int)time_ms);
    return TJC_SendCmd(cmd);
}

/**
 * @brief  向曲线/波形控件追加一个数据点。
 * @param  obj     曲线控件名，例如 "s0"。
 * @param  channel 通道号，从 0 开始。
 * @param  value   曲线点值，范围 0~255。
 * @retval HAL 状态码。
 */
HAL_StatusTypeDef TJC_AddWavePoint(const char *obj, uint8_t channel, uint8_t value)
{
    char cmd[TJC_CMD_BUF_LEN];

    if (obj == NULL) {
        return HAL_ERROR;
    }

    snprintf(cmd, sizeof(cmd), "add %s.id,%u,%u",
             obj, (unsigned int)channel, (unsigned int)value);
    return TJC_SendCmd(cmd);
}


/******************** 上层函数：恒流源主界面显示封装 ********************/

/**
 * @brief  初始化陶晶驰串口屏驱动，并向屏幕写入默认显示值。
 * @param  huart 串口句柄；传 NULL 时保持默认 USART2。
 */
void TJC_HMI_Init(UART_HandleTypeDef *huart)
{
    if (huart != NULL) {
        tjc_uart = huart;
    }

    // 给屏幕写入一组默认值，避免刚上电时显示旧数据。
    TJC_SetWorkModeConstCurrent();
    TJC_SetOutputState(TJC_OUTPUT_OFF);
    TJC_SetProtectState(TJC_PROTECT_NORMAL);
    TJC_SetSetCurrent_mA(0);
    TJC_SetActualCurrent_uA(0);
    TJC_SetLoadResistance_mOhm(0);
    TJC_SetLoadVoltage_mV(0);
    TJC_SetLoadPower_mW(0);
    TJC_SetOvercurrentThreshold_mA(550);
}

/**
 * @brief  设置工作模式显示为“恒流模式”。
 */
void TJC_SetWorkModeConstCurrent(void)
{
    TJC_SetText(TJC_OBJ_WORK_MODE, "恒流模式");
}

/**
 * @brief  更新输出状态显示。
 * @param  state 输出状态枚举值。
 */
void TJC_SetOutputState(TJC_OutputState_t state)
{
    TJC_SetText(TJC_OBJ_OUTPUT_STATE, TJC_OutputStateText(state));
}

/**
 * @brief  更新保护状态显示，并在故障状态变化时触发蜂鸣报警。
 * @param  state 保护状态枚举值。
 */
void TJC_SetProtectState(TJC_ProtectState_t state)
{
    TJC_SetText(TJC_OBJ_PROTECT_STATE, TJC_ProtectStateText(state));
    TJC_SetColor(TJC_OBJ_PROTECT_STATE,
                 state == TJC_PROTECT_NORMAL ? TJC_COLOR_BLACK : TJC_COLOR_RED);

    // 只在保护状态发生变化时报警，避免周期刷新屏幕导致蜂鸣器一直重复响。
    if (state != tjc_last_protect_state) {
        switch (state) {
            case TJC_PROTECT_OVERCURRENT:
                TJC_Beep(TJC_BEEP_OVERCURRENT_MS);
                break;
            case TJC_PROTECT_SHORT_LIMIT:
                TJC_Beep(TJC_BEEP_SHORT_LIMIT_MS);
                break;
            case TJC_PROTECT_OPEN_LOAD:
                TJC_Beep(TJC_BEEP_OPEN_LOAD_MS);
                break;
            case TJC_PROTECT_NORMAL:
            default:
                break;
        }
        tjc_last_protect_state = state;
    }
}

/**
 * @brief  更新设定电流显示。
 * @param  current_mA 设定电流，单位 mA，范围限制为 0~500。
 */
void TJC_SetSetCurrent_mA(int32_t current_mA)
{
    current_mA = TJC_ClampI32(current_mA, 0, 500);
    TJC_SetInt(TJC_OBJ_SET_CURRENT, current_mA);
}

/**
 * @brief  更新实际电流显示。
 * @param  current_uA 实际电流，单位 uA，屏幕显示为 mA 并保留 2 位小数。
 */
void TJC_SetActualCurrent_uA(int32_t current_uA)
{
    // uA -> 0.01 mA：1 mA = 1000 uA，所以除以 10 得到百分之一 mA。
    int32_t centi_mA = TJC_RoundDivI32((int64_t)current_uA, 10);
    centi_mA = TJC_ClampI32(centi_mA, 0, 99999);
    TJC_SetXFloatRaw(TJC_OBJ_ACTUAL_CURRENT, centi_mA);
}

/**
 * @brief  更新负载电阻显示。
 * @param  resistance_mOhm 负载电阻，单位 mOhm，屏幕显示为 Ohm 并保留 2 位小数。
 */
void TJC_SetLoadResistance_mOhm(int32_t resistance_mOhm)
{
    // mOhm -> 0.01 Ohm：1 Ohm = 1000 mOhm，所以除以 10。
    int32_t centi_ohm = TJC_RoundDivI32((int64_t)resistance_mOhm, 10);
    centi_ohm = TJC_ClampI32(centi_ohm, 0, 99999);
    TJC_SetXFloatRaw(TJC_OBJ_LOAD_RESISTANCE, centi_ohm);
}

/**
 * @brief  更新负载电压显示。
 * @param  voltage_mV 负载电压，单位 mV，屏幕显示为 V 并保留 2 位小数。
 */
void TJC_SetLoadVoltage_mV(int32_t voltage_mV)
{
    // mV -> 0.01 V：1 V = 1000 mV，所以除以 10。
    int32_t centi_v = TJC_RoundDivI32((int64_t)voltage_mV, 10);
    centi_v = TJC_ClampI32(centi_v, 0, 99999);
    TJC_SetXFloatRaw(TJC_OBJ_LOAD_VOLTAGE, centi_v);
}

/**
 * @brief  更新负载功率显示。
 * @param  power_mW 负载功率，单位 mW，屏幕显示为 W 并保留 2 位小数。
 */
void TJC_SetLoadPower_mW(int32_t power_mW)
{
    // mW -> 0.01 W：1 W = 1000 mW，所以除以 10。
    int32_t centi_w = TJC_RoundDivI32((int64_t)power_mW, 10);
    centi_w = TJC_ClampI32(centi_w, 0, 99999);
    TJC_SetXFloatRaw(TJC_OBJ_LOAD_POWER, centi_w);
}

/**
 * @brief  更新过流阈值显示。
 * @param  threshold_mA 过流阈值，单位 mA。
 */
void TJC_SetOvercurrentThreshold_mA(int32_t threshold_mA)
{
    threshold_mA = TJC_ClampI32(threshold_mA, 0, 999);
    TJC_SetInt(TJC_OBJ_OVERCURRENT, threshold_mA);
}

/**
 * @brief  同步更新设定电流与实际电流波形。
 * @param  set_current_mA    设定电流，单位 mA，0~500 mA 映射到 0~255。
 * @param  actual_current_uA 实际电流，单位 uA，0~500 mA 映射到 0~255。
 */
void TJC_AddCurrentWavePoint(int32_t set_current_mA, int32_t actual_current_uA)
{
    set_current_mA = TJC_ClampI32(set_current_mA, 0, TJC_CURRENT_WAVE_MAX_MA);

    uint8_t set_wave = TJC_CurrentToWaveValue_uA(set_current_mA * 1000);
    uint8_t actual_wave = TJC_CurrentToWaveValue_uA(actual_current_uA);

    TJC_AddWavePoint(TJC_OBJ_CURRENT_WAVE, TJC_CURRENT_WAVE_SET_CH, set_wave);
    TJC_AddWavePoint(TJC_OBJ_CURRENT_WAVE, TJC_CURRENT_WAVE_ACTUAL_CH, actual_wave);
}

/**
 * @brief  一次性刷新主界面的所有动态显示量。
 * @param  data 主界面显示数据结构指针。
 */
void TJC_UpdateAll(const TJC_HmiData_t *data)
{
    if (data == NULL) {
        return;
    }

    // 一次刷新所有主界面动态量。建议不要过高频率调用，200~500ms 一次即可。
    TJC_SetWorkModeConstCurrent();
    TJC_SetOutputState(data->output_state);
    TJC_SetProtectState(data->protect_state);
    TJC_SetSetCurrent_mA(data->set_current_mA);
    TJC_SetActualCurrent_uA(data->actual_current_uA);
    TJC_SetLoadResistance_mOhm(data->load_resistance_mOhm);
    TJC_SetLoadVoltage_mV(data->load_voltage_mV);
    TJC_SetLoadPower_mW(data->load_power_mW);
    TJC_SetOvercurrentThreshold_mA(data->overcurrent_mA);
}
