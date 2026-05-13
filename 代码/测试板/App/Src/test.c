#include "IRTask.h"


void StartIRTask(void *argument) {
  /* USER CODE BEGIN StartIRTask */
  uint8_t irCode;
  /* Infinite loop */
  for(;;)
  {
    //测试
    /*
    char msg[20] = "test\n";
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, sizeof(msg), HAL_MAX_DELAY);
    */

    //阻塞等待红外码
    if (osMessageQueueGet(IRQueueHandle, &irCode, NULL, osWaitForever) == osOK) {

      //调试：收到红外信号
      // HAL_GPIO_TogglePin(LED_Yellow_GPIO_Port, LED_Yellow_Pin);
      /*
      char testircode[20] = "";
      sprintf(testircode, "%d\n", irCode);
      HAL_UART_Transmit(&huart1, (uint8_t*)testircode, strlen(testircode), HAL_MAX_DELAY);
      */
        IR_Command_t cmd = (IR_Command_t)irCode;    //红外指令映射
        switch (cmd) {
            case IR_CMD_FORWARD:
            case IR_CMD_BACK:
            case IR_CMD_LEFT:
            case IR_CMD_RIGHT:
            case IR_CMD_STOP:
                osMessageQueuePut(MotorQueueHandle, &cmd, 0, 0);
                //调试:正常发送电机控制指令
                /*
                char MotorMsg[20] = "";
                sprintf(MotorMsg, "%d\n", cmd);
                HAL_UART_Transmit(&huart1, (uint8_t*)MotorMsg, strlen(MotorMsg), HAL_MAX_DELAY);
                */
                break;
            default:
                break;
        }


    }
      
  }
  /* USER CODE END StartIRTask */
}