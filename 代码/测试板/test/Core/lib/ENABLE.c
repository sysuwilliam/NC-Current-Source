//
// Created by lin on 2026/5/17.
//

#include "ENABLE.h"
#include "DAC.h"

void BUCK_Enable(void){

    if (HAL_GPIO_ReadPin(KEY_1_GPIO_Port, KEY_1_Pin) == GPIO_PIN_RESET) {
        HAL_Delay(20);
        if (HAL_GPIO_ReadPin(KEY_1_GPIO_Port, KEY_1_Pin) == GPIO_PIN_RESET) {
            HAL_GPIO_TogglePin(BUCK_EN_GPIO_Port, BUCK_EN_Pin);
            HAL_GPIO_TogglePin(SW_IN_GPIO_Port, SW_IN_Pin);
            HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
            while (HAL_GPIO_ReadPin(KEY_1_GPIO_Port, KEY_1_Pin) == GPIO_PIN_RESET);
            HAL_Delay(20);
        }
    }
}

// void SW_IN_Enable(void){
//
//     if (HAL_GPIO_ReadPin(KEY_1_GPIO_Port, KEY_1_Pin) == GPIO_PIN_RESET) {
//         HAL_Delay(20);
//         if (HAL_GPIO_ReadPin(KEY_1_GPIO_Port, KEY_1_Pin) == GPIO_PIN_RESET) {
//             HAL_GPIO_TogglePin(SW_IN_GPIO_Port, SW_IN_Pin);
//             HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
//             while (HAL_GPIO_ReadPin(KEY_1_GPIO_Port, KEY_1_Pin) == GPIO_PIN_RESET);
//             HAL_Delay(20);
//         }
//     }
// }

void Safe_Init (void) {
    HAL_GPIO_WritePin(SW_IN_GPIO_Port, SW_IN_Pin,GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin,GPIO_PIN_SET);
    HAL_GPIO_WritePin(BUCK_EN_GPIO_Port, BUCK_EN_Pin,GPIO_PIN_RESET);

    DAC_Set_Voltage(DAC_CHANNEL_1, 1,2.6f);
    DAC_Set_Voltage(DAC_CHANNEL_2, 1,0);
}


void Delay_us_Block(uint32_t us)
{
    uint32_t count = us * 8; // 根据主频粗略估算
    while(count--);
}

void Safe_Off(void) {
    DAC_Set_Voltage(DAC_CHANNEL_2,1, 0);
    Delay_us_Block(20000);
    HAL_GPIO_WritePin(SW_IN_GPIO_Port, SW_IN_Pin,GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin,GPIO_PIN_SET);
    HAL_GPIO_WritePin(BUCK_EN_GPIO_Port, BUCK_EN_Pin,GPIO_PIN_RESET);
}