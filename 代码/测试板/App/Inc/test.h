#include "FreeRTOS.h"
#include "infrared.h"
#include "cmsis_os2.h"
#include "motor.h"
#include "gpio.h"
#include "main.h"
#include <stdint.h>

void StartIRTask(void *argument);

//声明队列
extern osMessageQueueId_t IRQueueHandle;
extern osMessageQueueId_t MotorQueueHandle;

//定义
#define SPEED_LOW          50
#define SPEED_MEDIUM       80
#define SPEED_HIGH         100

//按键名称
typedef enum {
    IR_CMD_FORWARD = 12,
    IR_CMD_BACK    = 13,
    IR_CMD_LEFT    = 14,
    IR_CMD_RIGHT   = 15,
    IR_CMD_STOP    = 16,
} IR_Command_t;

