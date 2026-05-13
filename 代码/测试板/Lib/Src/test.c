#include "infrared.h"


//声明变量
uint16_t time_us = 0;  //用于存储捕获的时间值
uint8_t cnt;      //用于存储计数器的值
uint8_t IR_temp;  //用于存储临时的红外数据
uint8_t IR_buf[4] = {0}; //用于存储最终接收到的红外数据

//定义数字
#define NEC_START_MIN   12000
#define NEC_START_MAX   15000
#define NEC_0_MIN       900
#define NEC_0_MAX       1300
#define NEC_1_MIN       2000
#define NEC_1_MAX       2500
#define NEC_REPEAT_MIN  10000
#define NEC_REPEAT_MAX  12000


#define IR_KEY_NONE 0xFF

volatile uint8_t ir_frame_ready = 0; //红外帧完成标志

static uint8_t last_key = IR_KEY_NONE;


//红外接收初始化函数
void Infrared_Init(void) {
    HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_1);//启动定时器3的输入捕获中断
    __HAL_TIM_SET_CAPTUREPOLARITY(&htim3, TIM_CHANNEL_1, TIM_INPUTCHANNELPOLARITY_FALLING);//设置输入捕获的极性为下降沿
    TIM3->CNT = 0; //清零计数器
}


//接收信号函数
void Infrared_IC_Callback(void) {
        time_us = TIM3->CCR1;                       //获取捕获的计数值
    /*起始码*/
    if (time_us > NEC_START_MIN && time_us < NEC_START_MAX) {   //判断是否为起始信号（9ms + 4.45ms = 13500us）
        cnt = 0;                                //如果是起始信号，重置计数器
        IR_temp = 0;                            //重置临时变量
    } 
    /*Repeat码*/
    else if (time_us > NEC_REPEAT_MIN && time_us < NEC_REPEAT_MAX) {    //判断是否为repeat信号（9ms +2.25ms = 11500ms）
        if (last_key != IR_KEY_NONE) {
            osMessageQueuePut(IRQueueHandle, &last_key, 0, 0);
        }
        //调试: 串口输出重复码
        /*
        char repeatmsg[50] = "";
        sprintf(repeatmsg, "%d %d %d %d\n", IR_buf[0], IR_buf[1], IR_buf[2], IR_buf[3]);
        HAL_UART_Transmit(&huart1, (uint8_t*)repeatmsg, strlen(repeatmsg), HAL_MAX_DELAY);
        */
        TIM3->CNT = 0;
        return;
    }
    /*逻辑0*/
    else if (time_us > NEC_0_MIN && time_us < NEC_0_MAX) { //接收到逻辑0（0.56ms + 0.56ms = 1120us）
        cnt++;          //计数器加1
        IR_temp >>= 1;  //将临时变量右移1位，为接收下一个数据位做准备
    } 
    /*逻辑1*/
    else if (time_us > NEC_1_MIN && time_us < NEC_1_MAX) { //接收到逻辑1（0.56ms + 1.68ms = 2240us）
        cnt++;          //计数器加1
        IR_temp >>= 1;  //将临时变量右移1位，为接收下一个数据位做准备
        IR_temp |= 0x80; //将最高位设置为1，表示接收到逻辑1
    } 
    /*错误码*/
    else {
        cnt = 0;
        IR_temp = 0;    //如果接收到的信号不符合逻辑0或逻辑1的时间范围，重置计数器和临时变量
    }

    if (cnt == 8) {   //当计数器达到8时，表示已经接收了8位数据
        IR_buf[0] = IR_temp; //将接收到的数据存储到缓冲区
    } else if (cnt == 16) { 
        IR_buf[1] = IR_temp; 
    } else if (cnt == 24) { 
        IR_buf[2] = IR_temp; 
    } else if (cnt == 32) { 
        IR_buf[3] = IR_temp;
        ir_frame_ready = 1; //标记一帧数据接收完成

        uint8_t key = Infrared_GetKey();
        if (key != IR_KEY_NONE) {
            last_key = key;     //记录最后一次有效按键
            osMessageQueuePut(IRQueueHandle, &key, 0, 0);
        }

        //调试：串口输出IR_buf
        // HAL_GPIO_TogglePin(LED_White_GPIO_Port, LED_White_Pin);
        /*
        char testmsg[50] = "";
        sprintf(testmsg, "%d %d %d %d\n", IR_buf[0], IR_buf[1], IR_buf[2], IR_buf[3]);
        HAL_UART_Transmit(&huart1, (uint8_t*)testmsg, strlen(testmsg), HAL_MAX_DELAY);
        */
        
        //为下一帧做准备
        cnt = 0;
        IR_temp = 0;
    }

    TIM3->CNT = 0; //清零计数器，为下一次捕获做准备

}

//分析接收到的红外数据，返回对应的按键值
uint8_t Infrared_GetKey(void) {
    /*
    if (!ir_frame_ready) {
        return IR_KEY_NONE;
    }

    ir_frame_ready = 0; //立即清除，防止重复读取
    */
    //正式处理数据
        if (IR_buf[0] == (uint8_t)~IR_buf[1] && IR_buf[2] == (uint8_t)~IR_buf[3]) { //验证数据的正确性
            switch (IR_buf[2]) { //根据接收到的数据返回对应的按键值
                case 0x45: return 0; //按键1
                break;
                case 0x46: return 1; //按键2
                break;
                case 0x47: return 2; //按键3
                break;
                case 0x44: return 3; //按键4
                break;
                case 0x40: return 4; //按键5
                break;
                case 0x43: return 5; //按键6
                break;
                case 0x07: return 6; //按键7
                break;
                case 0x15: return 7; //按键8
                break;
                case 0x09: return 8; //按键9
                break;
                case 0x16: return 9; //按键*
                break;
                case 0x19: return 10; //按键0
                break;
                case 0x0D: return 11; //按键#
                break;
                case 0x18: return 12; //按键UP
                break;
                case 0x52: return 13; //按键DOWN
                break;
                case 0x08: return 14; //按键LEFT
                break;
                case 0x5A: return 15; //按键RIGHT
                break;
                case 0x1C: return 16; //按键OK
                break;
                default: return IR_KEY_NONE;
            }
        }

        return IR_KEY_NONE;
}



