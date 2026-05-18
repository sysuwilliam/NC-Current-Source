//
// Created by lin on 2026/5/17.
//

#include "ENABLE.h"

void BUCK_Enable(void){

    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_12) == GPIO_PIN_RESET) {
        HAL_Delay(20);
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_12) == GPIO_PIN_RESET) {
            HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_11);
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
            HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_10);
            while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_12) == GPIO_PIN_RESET);
            HAL_Delay(20);
        }
    }
}