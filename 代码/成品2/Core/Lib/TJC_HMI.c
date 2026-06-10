/**
 * Created by sysuwilliam on 2026/6/3.
 *
 * 陶晶驰串口屏驱动库实现
 *
 * 本文件只负责“屏幕显示层”的事情：
 * - 底层：通过 UART 非阻塞发送陶晶驰指令，并自动追加 0xff 0xff 0xff。
 * - 中层：提供设置文本控件、数字控件、虚拟浮点数控件的通用函数。
 * - 上层：按当前页面控件编号封装恒流源主界面的显示更新函数。
 *
 * 单位换算约定：
 * - 实际电流输入 uA，屏幕 x0 显示 mA，保留 2 位小数。
 * - 负载电阻输入 mOhm，屏幕 x1 显示 Ohm，保留 2 位小数。
 * - 负载电压输入 mV，屏幕 x2 显示 V，保留 2 位小数。
 * - 负载功率输入 mW，屏幕 x3 显示 W，保留 2 位小数。
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
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define TJC_CMD_BUF_LEN            64
#define TJC_FRAME_BUF_LEN          (TJC_CMD_BUF_LEN + 3U)
#define TJC_TX_QUEUE_DEPTH         16U
#define TJC_TX_STUCK_TIMEOUT_MS    100U
#define TJC_OFFLINE_RETRY_DELAY_MS 500U

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
#define TJC_OBJ_DAC_CH1_VOLTAGE "x4"
#define TJC_OBJ_DAC_CH2_VOLTAGE "x5"
#define TJC_OBJ_OVERCURRENT     "n1"
#define TJC_WAVEFORM_CURRENT_ID 28U
#define TJC_WAVEFORM_SET_CH     0U
#define TJC_WAVEFORM_LOAD_CH    1U

#define TJC_WAVEFORM_CURRENT_MIN_MA 0
#define TJC_WAVEFORM_CURRENT_MAX_MA 500
#define TJC_WAVEFORM_RAW_MIN        0
#define TJC_WAVEFORM_RAW_MAX        138
#define TJC_WAVEFORM_WINDOW_SPAN_MA 10
#define TJC_WAVEFORM_WINDOW_MARGIN_MA 2

#define TJC_BEEP_SHORT_LIMIT_MS 200
#define TJC_BEEP_OPEN_LOAD_MS   400
#define TJC_BEEP_OVERCURRENT_MS 600

#define TJC_COLOR_BLACK 0
#define TJC_COLOR_RED   63488

typedef struct
{
    uint16_t len;
    uint8_t data[TJC_FRAME_BUF_LEN];
} TJC_TxFrame_t;

// 默认使用 USART2 连接串口屏：PA2(TX) -> 屏幕 RX，PA3(RX) -> 屏幕 TX。
static UART_HandleTypeDef *tjc_uart = &huart2;
static TJC_ProtectState_t tjc_last_protect_state = TJC_PROTECT_NORMAL;

static TJC_TxFrame_t tjc_tx_queue[TJC_TX_QUEUE_DEPTH];
static volatile uint8_t tjc_tx_head = 0;
static volatile uint8_t tjc_tx_tail = 0;
static volatile uint8_t tjc_tx_count = 0;
static volatile uint8_t tjc_tx_active = 0;
static volatile uint8_t tjc_online = 1;
static volatile uint32_t tjc_tx_start_tick = 0;
static volatile uint32_t tjc_retry_after_tick = 0;
static volatile HAL_StatusTypeDef tjc_last_error = HAL_OK;
static int32_t tjc_wave_window_min_mA = TJC_WAVEFORM_CURRENT_MIN_MA;


/******************** 底层函数：UART 发送队列与状态管理 ********************/

static uint8_t TJC_TimeReached(uint32_t now, uint32_t target)
{
    return (uint8_t)((int32_t)(now - target) >= 0);
}

static void TJC_Lock(uint32_t *primask)
{
    *primask = __get_PRIMASK();
    __disable_irq();
}

static void TJC_Unlock(uint32_t primask)
{
    if (primask == 0U) {
        __enable_irq();
    }
}

static void TJC_ResetQueueLocked(void)
{
    tjc_tx_head = 0;
    tjc_tx_tail = 0;
    tjc_tx_count = 0;
}

static void TJC_SetLastError(HAL_StatusTypeDef status)
{
    tjc_last_error = status;
}

static void TJC_MarkOnline(HAL_StatusTypeDef status)
{
    tjc_online = 1;
    TJC_SetLastError(status);
}

static void TJC_MarkOfflineLocked(HAL_StatusTypeDef status)
{
    tjc_online = 0;
    tjc_retry_after_tick = HAL_GetTick() + TJC_OFFLINE_RETRY_DELAY_MS;
    tjc_tx_active = 0;
    tjc_tx_start_tick = 0;
    TJC_ResetQueueLocked();
    TJC_SetLastError(status);
}

static HAL_StatusTypeDef TJC_CombineStatus(HAL_StatusTypeDef aggregate, HAL_StatusTypeDef current)
{
    return aggregate == HAL_OK ? current : aggregate;
}

static void TJC_ServiceTx(void)
{
    HAL_StatusTypeDef status;
    uint8_t *data;
    uint16_t len;
    uint32_t primask;

    if (tjc_uart == NULL) {
        TJC_SetLastError(HAL_ERROR);
        return;
    }

    if (tjc_tx_active != 0U &&
        TJC_TimeReached(HAL_GetTick(), tjc_tx_start_tick + TJC_TX_STUCK_TIMEOUT_MS) != 0U) {
        (void)HAL_UART_AbortTransmit(tjc_uart);
        TJC_Lock(&primask);
        TJC_MarkOfflineLocked(HAL_TIMEOUT);
        TJC_Unlock(primask);
        return;
    }

    TJC_Lock(&primask);
    if (tjc_tx_active != 0U || tjc_tx_count == 0U) {
        TJC_Unlock(primask);
        return;
    }

    data = tjc_tx_queue[tjc_tx_head].data;
    len = tjc_tx_queue[tjc_tx_head].len;
    tjc_tx_active = 1;
    tjc_tx_start_tick = HAL_GetTick();
    TJC_Unlock(primask);

    status = HAL_UART_Transmit_IT(tjc_uart, data, len);
    if (status != HAL_OK) {
        TJC_Lock(&primask);
        tjc_tx_active = 0;
        tjc_tx_start_tick = 0;
        if (status == HAL_ERROR || status == HAL_TIMEOUT) {
            TJC_MarkOfflineLocked(status);
        } else {
            TJC_SetLastError(status);
        }
        TJC_Unlock(primask);
        return;
    }

    TJC_MarkOnline(HAL_OK);
}

static HAL_StatusTypeDef TJC_QueueFrame(const uint8_t *frame, uint16_t frame_len)
{
    uint32_t primask;

    if (tjc_uart == NULL || frame == NULL || frame_len == 0U || frame_len > TJC_FRAME_BUF_LEN) {
        TJC_SetLastError(HAL_ERROR);
        return HAL_ERROR;
    }

    TJC_ServiceTx();

    if (tjc_online == 0U &&
        TJC_TimeReached(HAL_GetTick(), tjc_retry_after_tick) == 0U) {
        TJC_SetLastError(HAL_ERROR);
        return HAL_ERROR;
    }

    TJC_Lock(&primask);
    if (tjc_tx_count >= TJC_TX_QUEUE_DEPTH) {
        TJC_SetLastError(HAL_BUSY);
        TJC_Unlock(primask);
        return HAL_BUSY;
    }

    memcpy(tjc_tx_queue[tjc_tx_tail].data, frame, frame_len);
    tjc_tx_queue[tjc_tx_tail].len = frame_len;
    tjc_tx_tail = (uint8_t)((tjc_tx_tail + 1U) % TJC_TX_QUEUE_DEPTH);
    tjc_tx_count++;
    TJC_Unlock(primask);

    TJC_ServiceTx();
    return HAL_OK;
}

static HAL_StatusTypeDef TJC_FormatAndSendCmd(const char *fmt, ...)
{
    char cmd[TJC_CMD_BUF_LEN];
    int written;
    va_list args;

    if (fmt == NULL) {
        TJC_SetLastError(HAL_ERROR);
        return HAL_ERROR;
    }

    va_start(args, fmt);
    written = vsnprintf(cmd, sizeof(cmd), fmt, args);
    va_end(args);

    if (written < 0 || written >= (int)sizeof(cmd)) {
        TJC_SetLastError(HAL_ERROR);
        return HAL_ERROR;
    }

    return TJC_SendCmd(cmd);
}

static void TJC_OnTxFinished(UART_HandleTypeDef *huart, HAL_StatusTypeDef status)
{
    uint32_t primask;

    if (huart != tjc_uart) {
        return;
    }

    TJC_Lock(&primask);
    if (tjc_tx_count > 0U) {
        tjc_tx_head = (uint8_t)((tjc_tx_head + 1U) % TJC_TX_QUEUE_DEPTH);
        tjc_tx_count--;
    }
    tjc_tx_active = 0;
    tjc_tx_start_tick = 0;

    if (status == HAL_OK) {
        TJC_MarkOnline(HAL_OK);
    } else {
        TJC_MarkOfflineLocked(status);
    }
    TJC_Unlock(primask);

    if (status == HAL_OK) {
        TJC_ServiceTx();
    }
}


/******************** 中层函数：通用控件属性写入 ********************/

HAL_StatusTypeDef TJC_SendCmd(const char *cmd)
{
    uint8_t frame[TJC_FRAME_BUF_LEN];
    size_t cmd_len;

    if (cmd == NULL) {
        TJC_SetLastError(HAL_ERROR);
        return HAL_ERROR;
    }

    cmd_len = strlen(cmd);
    if (cmd_len == 0U || cmd_len > TJC_CMD_BUF_LEN) {
        TJC_SetLastError(HAL_ERROR);
        return HAL_ERROR;
    }

    memcpy(frame, cmd, cmd_len);
    frame[cmd_len] = 0xffU;
    frame[cmd_len + 1U] = 0xffU;
    frame[cmd_len + 2U] = 0xffU;
    return TJC_QueueFrame(frame, (uint16_t)(cmd_len + 3U));
}

HAL_StatusTypeDef TJC_SetText(const char *obj, const char *txt)
{
    if (obj == NULL || txt == NULL) {
        TJC_SetLastError(HAL_ERROR);
        return HAL_ERROR;
    }

    return TJC_FormatAndSendCmd("%s.txt=\"%s\"", obj, txt);
}

HAL_StatusTypeDef TJC_SetInt(const char *obj, int32_t value)
{
    if (obj == NULL) {
        TJC_SetLastError(HAL_ERROR);
        return HAL_ERROR;
    }

    return TJC_FormatAndSendCmd("%s.val=%ld", obj, (long)value);
}

HAL_StatusTypeDef TJC_SetXFloatRaw(const char *obj, int32_t value)
{
    return TJC_SetInt(obj, value);
}

HAL_StatusTypeDef TJC_SetColor(const char *obj, uint16_t color)
{
    if (obj == NULL) {
        TJC_SetLastError(HAL_ERROR);
        return HAL_ERROR;
    }

    return TJC_FormatAndSendCmd("%s.pco=%u", obj, (unsigned int)color);
}

HAL_StatusTypeDef TJC_Page(const char *page_name)
{
    if (page_name == NULL) {
        TJC_SetLastError(HAL_ERROR);
        return HAL_ERROR;
    }

    return TJC_FormatAndSendCmd("page %s", page_name);
}

HAL_StatusTypeDef TJC_Beep(uint16_t time_ms)
{
    return TJC_FormatAndSendCmd("beep %u", (unsigned int)time_ms);
}


/******************** 上层函数：恒流源主界面显示封装 ********************/

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

static const char *TJC_OutputStateText(TJC_OutputState_t state)
{
    switch (state) {
        case TJC_OUTPUT_ON:
            return "OUTPUT";
        case TJC_OUTPUT_FAULT:
            return "FAULT";
        case TJC_OUTPUT_OFF:
        default:
            return "CLOSE";
    }
}

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
            return "NORMAL";
    }
}

static uint8_t TJC_MapCurrentToWaveRaw(int32_t current_mA)
{
    int32_t clamped_mA;
    int32_t mapped;

    clamped_mA = TJC_ClampI32(current_mA,
                              TJC_WAVEFORM_CURRENT_MIN_MA,
                              TJC_WAVEFORM_CURRENT_MAX_MA);
    mapped = TJC_RoundDivI32((int64_t)(clamped_mA - TJC_WAVEFORM_CURRENT_MIN_MA) *
                             (TJC_WAVEFORM_RAW_MAX - TJC_WAVEFORM_RAW_MIN),
                             TJC_WAVEFORM_CURRENT_MAX_MA - TJC_WAVEFORM_CURRENT_MIN_MA);
    mapped += TJC_WAVEFORM_RAW_MIN;
    mapped = TJC_ClampI32(mapped, TJC_WAVEFORM_RAW_MIN, TJC_WAVEFORM_RAW_MAX);
    return (uint8_t)mapped;
}

static uint8_t TJC_MapCurrentToWindowRaw(int32_t current_mA, int32_t window_min_mA, int32_t window_max_mA)
{
    int32_t clamped_current;
    int32_t mapped;
    int32_t span_mA;

    span_mA = window_max_mA - window_min_mA;
    if (span_mA <= 0) {
        return 0U;
    }

    clamped_current = TJC_ClampI32(current_mA, window_min_mA, window_max_mA);
    mapped = TJC_RoundDivI32((int64_t)(clamped_current - window_min_mA) *
                             (TJC_WAVEFORM_RAW_MAX - TJC_WAVEFORM_RAW_MIN),
                             span_mA);
    mapped += TJC_WAVEFORM_RAW_MIN;
    mapped = TJC_ClampI32(mapped, TJC_WAVEFORM_RAW_MIN, TJC_WAVEFORM_RAW_MAX);
    return (uint8_t)mapped;
}

static void TJC_AdjustDynamicWindow(int32_t sample_min_mA, int32_t sample_max_mA)
{
    int32_t window_min_mA;
    int32_t window_max_mA;
    int32_t target_center_mA;

    window_min_mA = tjc_wave_window_min_mA;
    window_min_mA = TJC_ClampI32(window_min_mA,
                                 TJC_WAVEFORM_CURRENT_MIN_MA,
                                 TJC_WAVEFORM_CURRENT_MAX_MA - TJC_WAVEFORM_WINDOW_SPAN_MA);
    window_max_mA = window_min_mA + TJC_WAVEFORM_WINDOW_SPAN_MA;

    if (sample_min_mA < window_min_mA + TJC_WAVEFORM_WINDOW_MARGIN_MA ||
        sample_max_mA > window_max_mA - TJC_WAVEFORM_WINDOW_MARGIN_MA) {
        target_center_mA = TJC_RoundDivI32((int64_t)sample_min_mA + (int64_t)sample_max_mA, 2);
        window_min_mA = target_center_mA - (TJC_WAVEFORM_WINDOW_SPAN_MA / 2);
        window_min_mA = TJC_ClampI32(window_min_mA,
                                     TJC_WAVEFORM_CURRENT_MIN_MA,
                                     TJC_WAVEFORM_CURRENT_MAX_MA - TJC_WAVEFORM_WINDOW_SPAN_MA);
        tjc_wave_window_min_mA = window_min_mA;
    }
}

HAL_StatusTypeDef TJC_HMI_Init(UART_HandleTypeDef *huart)
{
    HAL_StatusTypeDef status = HAL_OK;
    uint32_t primask;

    if (huart != NULL) {
        tjc_uart = huart;
    }
    if (tjc_uart == NULL) {
        TJC_SetLastError(HAL_ERROR);
        return HAL_ERROR;
    }

    TJC_Lock(&primask);
    TJC_ResetQueueLocked();
    tjc_tx_active = 0;
    tjc_tx_start_tick = 0;
    tjc_online = 1;
    tjc_retry_after_tick = 0;
    tjc_last_error = HAL_OK;
    tjc_last_protect_state = TJC_PROTECT_NORMAL;
    tjc_wave_window_min_mA = TJC_WAVEFORM_CURRENT_MIN_MA;
    TJC_Unlock(primask);

    status = TJC_CombineStatus(status, TJC_SetWorkModeConstCurrent());
    status = TJC_CombineStatus(status, TJC_SetOutputState(TJC_OUTPUT_OFF));
    status = TJC_CombineStatus(status, TJC_SetProtectState(TJC_PROTECT_NORMAL));
    status = TJC_CombineStatus(status, TJC_SetSetCurrent_mA(0));
    status = TJC_CombineStatus(status, TJC_SetActualCurrent_uA(0));
    status = TJC_CombineStatus(status, TJC_SetLoadResistance_mOhm(0));
    status = TJC_CombineStatus(status, TJC_SetLoadVoltage_mV(0));
    status = TJC_CombineStatus(status, TJC_SetLoadPower_mW(0));
    status = TJC_CombineStatus(status, TJC_DAC_CH1(0));
    status = TJC_CombineStatus(status, TJC_DAC_CH2(0));
    status = TJC_CombineStatus(status, TJC_SetOvercurrentThreshold_mA(550));
    status = TJC_CombineStatus(status, TJC_WaveS0_CurrentAxisInit());
    return status;
}

void TJC_HMI_Process(void)
{
    TJC_ServiceTx();
}

HAL_StatusTypeDef TJC_HMI_GetLastError(void)
{
    return tjc_last_error;
}

uint8_t TJC_HMI_IsOnline(void)
{
    return tjc_online;
}

HAL_StatusTypeDef TJC_SetWorkModeConstCurrent(void)
{
    return TJC_SetText(TJC_OBJ_WORK_MODE, "NC");
}

HAL_StatusTypeDef TJC_SetOutputState(TJC_OutputState_t state)
{
    return TJC_SetText(TJC_OBJ_OUTPUT_STATE, TJC_OutputStateText(state));
}

HAL_StatusTypeDef TJC_SetProtectState(TJC_ProtectState_t state)
{
    HAL_StatusTypeDef status;
    HAL_StatusTypeDef text_status;
    HAL_StatusTypeDef color_status;

    text_status = TJC_SetText(TJC_OBJ_PROTECT_STATE, TJC_ProtectStateText(state));
    color_status = TJC_SetColor(TJC_OBJ_PROTECT_STATE,
                                state == TJC_PROTECT_NORMAL ? TJC_COLOR_BLACK : TJC_COLOR_RED);
    status = TJC_CombineStatus(text_status, color_status);

    if (text_status == HAL_OK && color_status == HAL_OK && state != tjc_last_protect_state) {
        switch (state) {
            case TJC_PROTECT_OVERCURRENT:
                status = TJC_CombineStatus(status, TJC_Beep(TJC_BEEP_OVERCURRENT_MS));
                break;
            case TJC_PROTECT_SHORT_LIMIT:
                status = TJC_CombineStatus(status, TJC_Beep(TJC_BEEP_SHORT_LIMIT_MS));
                break;
            case TJC_PROTECT_OPEN_LOAD:
                status = TJC_CombineStatus(status, TJC_Beep(TJC_BEEP_OPEN_LOAD_MS));
                break;
            case TJC_PROTECT_NORMAL:
            default:
                break;
        }
        tjc_last_protect_state = state;
    }

    return status;
}

HAL_StatusTypeDef TJC_SetSetCurrent_mA(int32_t current_mA)
{
    return TJC_SetInt(TJC_OBJ_SET_CURRENT, current_mA);
}

HAL_StatusTypeDef TJC_SetActualCurrent_uA(int32_t current_uA)
{
    int32_t centi_mA = TJC_RoundDivI32((int64_t)current_uA, 10);
    centi_mA = TJC_ClampI32(centi_mA, 0, 99999);
    return TJC_SetXFloatRaw(TJC_OBJ_ACTUAL_CURRENT, centi_mA);
}

HAL_StatusTypeDef TJC_SetLoadResistance_mOhm(int32_t resistance_mOhm)
{
    int32_t centi_ohm = TJC_RoundDivI32((int64_t)resistance_mOhm, 10);
    centi_ohm = TJC_ClampI32(centi_ohm, 0, 99999);
    return TJC_SetXFloatRaw(TJC_OBJ_LOAD_RESISTANCE, centi_ohm);
}

HAL_StatusTypeDef TJC_SetLoadVoltage_mV(int32_t voltage_mV)
{
    int32_t centi_v = TJC_RoundDivI32((int64_t)voltage_mV, 10);
    centi_v = TJC_ClampI32(centi_v, 0, 99999);
    return TJC_SetXFloatRaw(TJC_OBJ_LOAD_VOLTAGE, centi_v);
}

HAL_StatusTypeDef TJC_SetLoadPower_mW(int32_t power_mW)
{
    int32_t centi_w = TJC_RoundDivI32((int64_t)power_mW, 10);
    centi_w = TJC_ClampI32(centi_w, 0, 99999);
    return TJC_SetXFloatRaw(TJC_OBJ_LOAD_POWER, centi_w);
}

HAL_StatusTypeDef TJC_SetOvercurrentThreshold_mA(int32_t threshold_mA)
{
    threshold_mA = TJC_ClampI32(threshold_mA, 0, 999);
    return TJC_SetInt(TJC_OBJ_OVERCURRENT, threshold_mA);
}

HAL_StatusTypeDef TJC_DAC_CH1(int32_t DAC_CH1_mV)
{
    int32_t centi_v = TJC_RoundDivI32((int64_t)DAC_CH1_mV, 10);
    centi_v = TJC_ClampI32(centi_v, 0, 99999);
    return TJC_SetXFloatRaw(TJC_OBJ_DAC_CH1_VOLTAGE, centi_v);
}

HAL_StatusTypeDef TJC_DAC_CH2(int32_t DAC_CH2_mV)
{
    int32_t centi_v = TJC_RoundDivI32((int64_t)DAC_CH2_mV, 10);
    centi_v = TJC_ClampI32(centi_v, 0, 99999);
    return TJC_SetXFloatRaw(TJC_OBJ_DAC_CH2_VOLTAGE, centi_v);
}

HAL_StatusTypeDef TJC_WaveformClear(uint8_t obj_id, uint8_t channel)
{
    return TJC_FormatAndSendCmd("cle %u,%u", (unsigned int)obj_id, (unsigned int)channel);
}

HAL_StatusTypeDef TJC_WaveformAdd(uint8_t obj_id, uint8_t channel, uint8_t value)
{
    return TJC_FormatAndSendCmd("add %u,%u,%u",
                                (unsigned int)obj_id,
                                (unsigned int)channel,
                                (unsigned int)value);
}

HAL_StatusTypeDef TJC_WaveS0_CurrentAxisInit(void)
{
    // 运行时能确定的是“通道数据按 0~500mA -> 0~138 原始值映射”。
    // 页面上的刻度文字仍建议在 HMI 工程里固定标注为 0~500mA。
    return TJC_WaveformClear(TJC_WAVEFORM_CURRENT_ID, 255U);
}

HAL_StatusTypeDef TJC_WaveS0_AddCurrentPoint(int32_t set_current_mA, int32_t load_current_uA)
{
    HAL_StatusTypeDef status = HAL_OK;
    int32_t load_current_mA;

    load_current_mA = TJC_RoundDivI32((int64_t)load_current_uA, 1000);
    status = TJC_CombineStatus(status,
                               TJC_WaveformAdd(TJC_WAVEFORM_CURRENT_ID,
                                               TJC_WAVEFORM_SET_CH,
                                               TJC_MapCurrentToWaveRaw(set_current_mA)));
    status = TJC_CombineStatus(status,
                               TJC_WaveformAdd(TJC_WAVEFORM_CURRENT_ID,
                                               TJC_WAVEFORM_LOAD_CH,
                                               TJC_MapCurrentToWaveRaw(load_current_mA)));
    return status;
}

void TJC_WaveS0_DynamicWindowReset(int32_t window_min_mA)
{
    tjc_wave_window_min_mA = TJC_ClampI32(window_min_mA,
                                          TJC_WAVEFORM_CURRENT_MIN_MA,
                                          TJC_WAVEFORM_CURRENT_MAX_MA - TJC_WAVEFORM_WINDOW_SPAN_MA);
}

HAL_StatusTypeDef TJC_WaveS0_AddCurrentPointDynamicWindow(int32_t set_current_mA, int32_t load_current_uA)
{
    HAL_StatusTypeDef status = HAL_OK;
    int32_t load_current_mA;
    int32_t sample_min_mA;
    int32_t sample_max_mA;
    int32_t window_min_mA;
    int32_t window_max_mA;

    load_current_mA = TJC_RoundDivI32((int64_t)load_current_uA, 1000);
    set_current_mA = TJC_ClampI32(set_current_mA,
                                  TJC_WAVEFORM_CURRENT_MIN_MA,
                                  TJC_WAVEFORM_CURRENT_MAX_MA);
    load_current_mA = TJC_ClampI32(load_current_mA,
                                   TJC_WAVEFORM_CURRENT_MIN_MA,
                                   TJC_WAVEFORM_CURRENT_MAX_MA);

    sample_min_mA = set_current_mA < load_current_mA ? set_current_mA : load_current_mA;
    sample_max_mA = set_current_mA > load_current_mA ? set_current_mA : load_current_mA;
    TJC_AdjustDynamicWindow(sample_min_mA, sample_max_mA);

    window_min_mA = tjc_wave_window_min_mA;
    window_max_mA = window_min_mA + TJC_WAVEFORM_WINDOW_SPAN_MA;

    status = TJC_CombineStatus(status,
                               TJC_WaveformAdd(TJC_WAVEFORM_CURRENT_ID,
                                               TJC_WAVEFORM_SET_CH,
                                               TJC_MapCurrentToWindowRaw(set_current_mA,
                                                                         window_min_mA,
                                                                         window_max_mA)));
    status = TJC_CombineStatus(status,
                               TJC_WaveformAdd(TJC_WAVEFORM_CURRENT_ID,
                                               TJC_WAVEFORM_LOAD_CH,
                                               TJC_MapCurrentToWindowRaw(load_current_mA,
                                                                         window_min_mA,
                                                                         window_max_mA)));
    return status;
}

HAL_StatusTypeDef TJC_UpdateAll(const TJC_HmiData_t *data)
{
    HAL_StatusTypeDef status = HAL_OK;

    if (data == NULL) {
        TJC_SetLastError(HAL_ERROR);
        return HAL_ERROR;
    }

    status = TJC_CombineStatus(status, TJC_SetWorkModeConstCurrent());
    status = TJC_CombineStatus(status, TJC_SetOutputState(data->output_state));
    status = TJC_CombineStatus(status, TJC_SetProtectState(data->protect_state));
    status = TJC_CombineStatus(status, TJC_SetSetCurrent_mA(data->set_current_mA));
    status = TJC_CombineStatus(status, TJC_SetActualCurrent_uA(data->actual_current_uA));
    status = TJC_CombineStatus(status, TJC_SetLoadResistance_mOhm(data->load_resistance_mOhm));
    status = TJC_CombineStatus(status, TJC_SetLoadVoltage_mV(data->load_voltage_mV));
    status = TJC_CombineStatus(status, TJC_SetLoadPower_mW(data->load_power_mW));
    status = TJC_CombineStatus(status, TJC_SetOvercurrentThreshold_mA(data->overcurrent_mA));
    return status;
}


/******************** HAL UART 回调接管 ********************/

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    TJC_OnTxFinished(huart, HAL_OK);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    TJC_OnTxFinished(huart, HAL_ERROR);
}

void HAL_UART_AbortTransmitCpltCallback(UART_HandleTypeDef *huart)
{
    TJC_OnTxFinished(huart, HAL_TIMEOUT);
}
