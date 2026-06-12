//
// Created by lin on 2026/6/11.
//

#include "App_control.h"
#include "global_value.h"
#include "dsp_test_communication.h"

void Safe_control(void) {
    if (Safe_flag == TJC_PROTECT_IDLE) {
        HAL_GPIO_WritePin(BUCK_EN_GPIO_Port, BUCK_EN_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
        return;
    }

    static uint8_t Open_count = 0;

    if (I_disp < 0.0001f && HAL_GPIO_ReadPin(BUCK_EN_GPIO_Port, BUCK_EN_Pin) == GPIO_PIN_SET && I_set>=0.001f) {
        Open_count++;
        if (Open_count > 10) {
            Safe_flag = TJC_PROTECT_OPEN_LOAD;
            Open_count = 0;
        }
    } else if (I_disp > 0.550f && HAL_GPIO_ReadPin(BUCK_EN_GPIO_Port, BUCK_EN_Pin) == GPIO_PIN_SET) {
        Open_count = 0;
        Safe_flag = TJC_PROTECT_OVERCURRENT;
    } else if (Rload_disp < 0.5f && HAL_GPIO_ReadPin(BUCK_EN_GPIO_Port, BUCK_EN_Pin) == GPIO_PIN_SET) {
        Safe_flag = TJC_PROTECT_SHORT_LIMIT;
    } else if (HAL_GPIO_ReadPin(BUCK_EN_GPIO_Port, BUCK_EN_Pin) == GPIO_PIN_SET) {
        Open_count = 0;
        Safe_flag = TJC_PROTECT_NORMAL;
    }

    switch (Safe_flag) {
        case TJC_PROTECT_OVERCURRENT:
            HAL_GPIO_WritePin(BUCK_EN_GPIO_Port, BUCK_EN_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
            I_set = 0.0f;
            DAC2_cmd = 0.0f;
            Global_OUTPUT(TJC_PROTECT_OVERCURRENT);
            break;

        case TJC_PROTECT_OPEN_LOAD:
            if (I_set<0.001f) {
                Safe_flag=TJC_PROTECT_NORMAL;
            }else {
                Global_OUTPUT(TJC_PROTECT_OPEN_LOAD);
                TJC_Beep(50);
                TJC_SetLoadVoltage_mV(0);
                TJC_SetLoadPower_mW(0);
                HAL_GPIO_WritePin(BUCK_EN_GPIO_Port, BUCK_EN_Pin, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
            }
            break;

        case TJC_PROTECT_SHORT_LIMIT:
            Global_OUTPUT(TJC_PROTECT_SHORT_LIMIT); //只要输出表示短路。短路依旧正常工作
            HAL_GPIO_WritePin(BUCK_EN_GPIO_Port, BUCK_EN_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
            break;

        case TJC_PROTECT_NORMAL:
            Global_OUTPUT(TJC_PROTECT_NORMAL);
            HAL_GPIO_WritePin(BUCK_EN_GPIO_Port, BUCK_EN_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
        break;

        default:
            Global_OUTPUT(TJC_PROTECT_NORMAL);
            break;
    }
}
