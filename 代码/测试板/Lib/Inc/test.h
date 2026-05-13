#ifndef __INFRARED_H
#define __INFRARED_H

#include "usart.h"
#include "tim.h"
#include "cmsis_os2.h"

#include <stdio.h>
#include <stdint.h>

void Infrared_Init(void);
void Infrared_IC_Callback(void);
uint8_t Infrared_GetKey(void);
uint8_t Infrared_EXTI_Decode(uint8_t *key);

extern osMessageQueueId_t IRQueueHandle;//声明队列

#endif