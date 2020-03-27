/**
  ******************************************************************************
  * File Name          : main.c
  * Description        : Main program body
  ******************************************************************************
  *
  * COPYRIGHT(c) 2016 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "cmsis_os.h"

/* USER CODE BEGIN Includes */

/*******************************************************************************
* Author: Benjamin Scholtz
* Contact: bscholtz.bds@gmail.com
* Purpose: Mechatronic Engineering Undergrad Thesis: Baleka - Robotic Hopping Leg
* Tools: STM32CubeMX, FreeRTOS, HAL
*******************************************************************************/

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <complex.h>    /* Standard Library of Complex Numbers */

#include "CRC.h"
#include "arrayFunctions.h"
#include <stm32f4xx_hal_uart.h>
#include <stm32f4xx_hal_usart.h>

#include "rtraj_lookuptable.h"
#include "thetatraj_lookuptable.h"
#include "kstraj_lookuptable.h"

//https://github.com/PetteriAimonen/Baselibc
//#include "memccpy.c"
//#include "memcmp.c"


/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart4;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;
UART_HandleTypeDef huart6;
DMA_HandleTypeDef hdma_uart4_rx;
DMA_HandleTypeDef hdma_uart4_tx;
DMA_HandleTypeDef hdma_usart2_rx;
DMA_HandleTypeDef hdma_usart2_tx;
DMA_HandleTypeDef hdma_usart3_rx;
DMA_HandleTypeDef hdma_usart3_tx;
DMA_HandleTypeDef hdma_usart6_rx;
DMA_HandleTypeDef hdma_usart6_tx;

osThreadId defaultTaskHandle;
osThreadId TXPCHandle;
osThreadId RXPCHandle;
osThreadId HeartbeatHandle;
osThreadId TXMotor1Handle;
osThreadId TXMotor2Handle;
osThreadId RXMotor1Handle;
osThreadId RXMotor2Handle;
osThreadId ControllerHandle;
osMessageQId ProcessQM1Handle;
osMessageQId ProcessQM2Handle;
osMessageQId ProcessQPCHandle;
osMessageQId TransmitM1QHandle;
osMessageQId TransmitM2QHandle;
osMessageQId ICommandM1QHandle;
osMessageQId ICommandM2QHandle;
osMessageQId PCommandM1QHandle;
osMessageQId PCommandM2QHandle;
osMessageQId CommandM1QHandle;
osMessageQId CommandM2QHandle;
osMessageQId ControllerQHandle;
osMessageQId ControlM1QHandle;
osMessageQId ControlM2QHandle;
osMessageQId ProcessQControlHandle;
osSemaphoreId M1Handle;
osSemaphoreId M2Handle;

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

//Communication Timing
uint8_t Ts = 25; //Sampling time in 1/_X_ ms
uint8_t Td = 3;
//NB: #define configTICK_RATE_HZ ((TickType_t)_X_000) in FreeRTOSConfig

uint8_t RXBufPC[100];
uint8_t RXBufM1[50];
uint8_t RXBufM2[50];

#define PI 3.141592653f

//Motor UART connections
#define M1_UART huart3
#define M2_UART huart2
#define PC_UART huart4

//Packet data size
#define PAYLOAD_TX 21
#define PAYLOAD_MISC 32
#define PAYLOAD_RX 63

//Packet Op-Codes
#define KILL_BRIDGE 0
#define WRITE_ENABLE 1
#define BRIDGE_ENABLE 2
#define CURRENT_COMMAND 20
#define POSITION_COMMAND 22
#define ZERO_POSITION 8
#define GAIN_SET 9
#define GAIN_CHANGE_M1 10
#define GAIN_CHANGE_M2 13
#define CONFIG_SET 16
#define READ_CURRENT 5
#define READ_POSITION 6
#define READ_VELOCITY 7

#define CONTROL_CURRENT_M1 40
#define CONTROL_CURRENT_M2 41

#define START_CONTROL 30

#define TRIGGER_ONESHOT 31
uint8_t TRIGGER = 0;
uint8_t PULLED = 0;
uint8_t SHOT = 0;
uint16_t ELAPSED = 0;

uint8_t FOOT_TRIGGER = 0;

////////////////////////////////////////////////////////////////////////

uint8_t KILL_BRIDGE_DATA[2] = {0x01, 0x00};
uint8_t WRITE_ENABLE_DATA[2] = {0x0F, 0x00};
uint8_t BRIDGE_ENABLE_DATA[2] = {0x00, 0x00};
uint8_t GAIN_SET_0_DATA[4] = {0x00, 0x00, 0x00, 0x00};
uint8_t GAIN_SET_1_DATA[4] = {0x08, 0x00, 0x00, 0x00};
uint8_t ZERO_POSITION_DATA[4] = {0x08, 0x00, 0x00, 0x00};
uint8_t CONFIG_0_SET_DATA[2] = {0x00, 0x00};
uint8_t CONFIG_1_SET_DATA[2] = {0x01, 0x00};

////////////////////////////////////////////////////////////////////////
//TX Packet to PC

//'packed' makes sure compiler won't insert any gaps!
struct __attribute__((__packed__)) TXPacketStruct {
        uint8_t START[2];

        uint8_t M1C[2];
        uint8_t M1P[4];
        uint8_t M1V[4];

        uint8_t M2C[2];
        uint8_t M2P[4];
        uint8_t M2V[4];

        // uint8_t ACCX[2];
        // uint8_t ACCY[2];
        // uint8_t ACCZ[2];
        // uint8_t GYRX[2];
        // uint8_t GYRY[2];
        // uint8_t GYRZ[2];
        // uint8_t TEMP;
        uint8_t MISC[PAYLOAD_MISC];

        uint8_t StatBIT_1 : 1;
        uint8_t StatBIT_2 : 1;
        uint8_t StatBIT_3 : 1;
        uint8_t StatBIT_4 : 1;
        uint8_t StatBIT_5 : 1;
        uint8_t StatBIT_6 : 1;
        uint8_t StatBIT_7 : 1;
        uint8_t StatBIT_8 : 1;

        uint8_t CRCCheck[2];

        uint8_t STOP[2];
};

struct TXPacketStruct PCPacket;
//Transmit pointer PCPacketPTR with sizeof(PCPacket)
uint8_t *PCPacketPTR = (uint8_t*)&PCPacket;

////////////////////////////////////////////////////////////////////////
//RX Packet from PC

struct __attribute__((__packed__)) RXPacketStruct {
        uint8_t START[2];

        uint8_t OPCODE;

        uint8_t M1C[4];
        uint8_t M2C[4];

        uint8_t M1P[4];
        uint8_t M2P[4];

        float r_cmd;
        float s_cmd;

        float k_r;
        float k_s;
        float ki_r;
        float ki_s;

        float kr_s;
        float kr_d;
        float ks_s;
        float ks_d;

        float k_i;

        uint8_t StatBIT_1 : 1;
        uint8_t StatBIT_2 : 1;
        uint8_t StatBIT_3 : 1;
        uint8_t StatBIT_4 : 1;
        uint8_t StatBIT_5 : 1;
        uint8_t StatBIT_6 : 1;
        uint8_t StatBIT_7 : 1;
        uint8_t StatBIT_8 : 1;

        uint8_t TRIGGER;

        uint8_t CRCCheck[2];

        uint8_t STOP[2];
};

struct RXPacketStruct RXPacket;
uint8_t *RXPacketPTR = (uint8_t*)&RXPacket;

uint8_t RX_DATA_VALID = 0;

////////////////////////////////////////////////////////////////////////
//Driver Command Compilation

struct __attribute__((__packed__)) BaseCommandStruct {
        uint8_t START[2];
        uint8_t CB;
        uint8_t INDOFF[2];
        uint8_t LEN;
        uint8_t CRC1[2];
        uint8_t DATA[4];
        uint8_t CRC2[2];
};

uint8_t SNIP;

struct BaseCommandStruct BaseCommand[50];
struct BaseCommandStruct* BaseCommandPTR;

//Used for converting from word to byte array etc.
union {
        uint32_t WORD;
        uint16_t HALFWORD;
        uint8_t BYTE[4];
} WORDtoBYTEBase;

uint32_t CALC_CRCBase;

////////////////////////////////////////////////////////////////////////
//Controller

struct ControlPacketStruct {
        uint8_t M1C[2];
        uint8_t M1P[4];
        uint8_t M1V[4];

        uint8_t M2C[2];
        uint8_t M2P[4];
        uint8_t M2V[4];

        // uint8_t ACCX[2];
        // uint8_t ACCY[2];
        // uint8_t ACCZ[2];
        // uint8_t GYRX[2];
        // uint8_t GYRY[2];
        // uint8_t GYRZ[2];
        // uint8_t TEMP;
        uint8_t MISC[16];
};

struct ControlPacketStruct ControlPacket;

struct __attribute__((__packed__)) ControlLogStruct {
        float I_cmd_0;
        float I_cmd_1;
        float f_r;
        float f_s;
        float r_fbk;
        float s_fbk;
        float r_d_fbk;
        float s_d_fbk;
};

struct ControlLogStruct ControlLogPacket;
uint8_t *ControlLogPacketPTR = (uint8_t*)&ControlLogPacket;

uint8_t START = 0;

union {
        float FLOAT;
        int32_t INT32;
        int16_t INT16;
        uint8_t BYTE[4];
} F2BM1;

union {
        float FLOAT;
        int32_t INT32;
        int16_t INT16;
        uint8_t BYTE[4];
} F2BM2;

////////////////////////////////////////////////////////////////////////
//Binary Semaphores

SemaphoreHandle_t PCRXHandle;
SemaphoreHandle_t PCTXHandle;

SemaphoreHandle_t TXMotorM1Handle;
SemaphoreHandle_t TXMotorM2Handle;
SemaphoreHandle_t RXMotorM1Handle;
SemaphoreHandle_t RXMotorM2Handle;

SemaphoreHandle_t ControlM1Handle;
SemaphoreHandle_t ControlM2Handle;

////////////////////////////////////////////////////////////////////////

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void Error_Handler(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_UART4_Init(void);
static void MX_USART6_UART_Init(void);
void StartDefaultTask(void const * argument);
void StartTXPC(void const * argument);
void StartRXPC(void const * argument);
void StartHeartbeat(void const * argument);
void StartTXMotor1(void const * argument);
void StartTXMotor2(void const * argument);
void StartRXMotor1(void const * argument);
void StartRXMotor2(void const * argument);
void StartController(void const * argument);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

void SetupBinarySemaphores(void);

//Motor driver packet compilation function
int32_t swap_int32( int32_t val );
void BaseCommandCompile(uint8_t n, uint8_t SeqBits, uint8_t ComBits, uint8_t INDOFF1, uint8_t INDOFF2, uint8_t *DATA, uint8_t LEN, uint8_t SNIP);

//Motor driver DMA commands
void TransmitM1_DMA(uint8_t *data, uint8_t size);
void ReceiveM1_DMA(uint8_t *data, uint8_t size);
void TransmitM2_DMA(uint8_t *data, uint8_t size);
void ReceiveM2_DMA(uint8_t *data, uint8_t size);

//Call-back functions
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
void HAL_UART_RxIdleCallback(UART_HandleTypeDef *huart);
void DMA_XFER_CPLT_Callback(DMA_HandleTypeDef *_hdma);

void HAL_UART_EndDMA_RX(UART_HandleTypeDef *huart);

//Kinematics: SI Units and Radians
float *ForwardKinematics(float phi1, float phi2);
float *InverseKinematics(float r, float theta);

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

int main(void)
{

  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  /* MCU Configuration----------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_UART4_Init();
  MX_USART6_UART_Init();

  /* USER CODE BEGIN 2 */
        initCRC(0); //iNemo CRC False
        initCRC(1); //Driver CRC XModem
        SetupBinarySemaphores();
  /* USER CODE END 2 */

  /* USER CODE BEGIN RTOS_MUTEX */
        /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* definition and creation of M1 */
  osSemaphoreDef(M1);
  M1Handle = osSemaphoreCreate(osSemaphore(M1), 1);

  /* definition and creation of M2 */
  osSemaphoreDef(M2);
  M2Handle = osSemaphoreCreate(osSemaphore(M2), 1);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
        /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
        /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* definition and creation of TXPC */
  osThreadDef(TXPC, StartTXPC, osPriorityHigh, 0, 128);
  TXPCHandle = osThreadCreate(osThread(TXPC), NULL);

  /* definition and creation of RXPC */
  osThreadDef(RXPC, StartRXPC, osPriorityHigh, 0, 128);
  RXPCHandle = osThreadCreate(osThread(RXPC), NULL);

  /* definition and creation of Heartbeat */
  osThreadDef(Heartbeat, StartHeartbeat, osPriorityHigh, 0, 128);
  HeartbeatHandle = osThreadCreate(osThread(Heartbeat), NULL);

  /* definition and creation of TXMotor1 */
  osThreadDef(TXMotor1, StartTXMotor1, osPriorityHigh, 0, 128);
  TXMotor1Handle = osThreadCreate(osThread(TXMotor1), NULL);

  /* definition and creation of TXMotor2 */
  osThreadDef(TXMotor2, StartTXMotor2, osPriorityHigh, 0, 128);
  TXMotor2Handle = osThreadCreate(osThread(TXMotor2), NULL);

  /* definition and creation of RXMotor1 */
  osThreadDef(RXMotor1, StartRXMotor1, osPriorityHigh, 0, 128);
  RXMotor1Handle = osThreadCreate(osThread(RXMotor1), NULL);

  /* definition and creation of RXMotor2 */
  osThreadDef(RXMotor2, StartRXMotor2, osPriorityHigh, 0, 128);
  RXMotor2Handle = osThreadCreate(osThread(RXMotor2), NULL);

  /* definition and creation of Controller */
  osThreadDef(Controller, StartController, osPriorityHigh, 0, 500);
  ControllerHandle = osThreadCreate(osThread(Controller), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
        /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* Create the queue(s) */
  /* definition and creation of ProcessQM1 */
  osMessageQDef(ProcessQM1, 1, uint32_t);
  ProcessQM1Handle = osMessageCreate(osMessageQ(ProcessQM1), NULL);

  /* definition and creation of ProcessQM2 */
  osMessageQDef(ProcessQM2, 1, uint32_t);
  ProcessQM2Handle = osMessageCreate(osMessageQ(ProcessQM2), NULL);

  /* definition and creation of ProcessQPC */
  osMessageQDef(ProcessQPC, 1, uint32_t);
  ProcessQPCHandle = osMessageCreate(osMessageQ(ProcessQPC), NULL);

  /* definition and creation of TransmitM1Q */
  osMessageQDef(TransmitM1Q, 3, uint32_t);
  TransmitM1QHandle = osMessageCreate(osMessageQ(TransmitM1Q), NULL);

  /* definition and creation of TransmitM2Q */
  osMessageQDef(TransmitM2Q, 3, uint32_t);
  TransmitM2QHandle = osMessageCreate(osMessageQ(TransmitM2Q), NULL);

  /* definition and creation of ICommandM1Q */
  osMessageQDef(ICommandM1Q, 1, uint32_t);
  ICommandM1QHandle = osMessageCreate(osMessageQ(ICommandM1Q), NULL);

  /* definition and creation of ICommandM2Q */
  osMessageQDef(ICommandM2Q, 1, uint32_t);
  ICommandM2QHandle = osMessageCreate(osMessageQ(ICommandM2Q), NULL);

  /* definition and creation of PCommandM1Q */
  osMessageQDef(PCommandM1Q, 1, uint32_t);
  PCommandM1QHandle = osMessageCreate(osMessageQ(PCommandM1Q), NULL);

  /* definition and creation of PCommandM2Q */
  osMessageQDef(PCommandM2Q, 1, uint32_t);
  PCommandM2QHandle = osMessageCreate(osMessageQ(PCommandM2Q), NULL);

  /* definition and creation of CommandM1Q */
  osMessageQDef(CommandM1Q, 3, uint32_t);
  CommandM1QHandle = osMessageCreate(osMessageQ(CommandM1Q), NULL);

  /* definition and creation of CommandM2Q */
  osMessageQDef(CommandM2Q, 3, uint32_t);
  CommandM2QHandle = osMessageCreate(osMessageQ(CommandM2Q), NULL);

  /* definition and creation of ControllerQ */
  osMessageQDef(ControllerQ, 1, uint32_t);
  ControllerQHandle = osMessageCreate(osMessageQ(ControllerQ), NULL);

  /* definition and creation of ControlM1Q */
  osMessageQDef(ControlM1Q, 1, uint32_t);
  ControlM1QHandle = osMessageCreate(osMessageQ(ControlM1Q), NULL);

  /* definition and creation of ControlM2Q */
  osMessageQDef(ControlM2Q, 1, uint32_t);
  ControlM2QHandle = osMessageCreate(osMessageQ(ControlM2Q), NULL);

  /* definition and creation of ProcessQControl */
  osMessageQDef(ProcessQControl, 1, uint32_t);
  ProcessQControlHandle = osMessageCreate(osMessageQ(ProcessQControl), NULL);

  /* USER CODE BEGIN RTOS_QUEUES */
        /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */


  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
        while (1)
        {
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */

        }
  /* USER CODE END 3 */

}

/** System Clock Configuration
*/
void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;

  __HAL_RCC_PWR_CLK_ENABLE();

  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = 16;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 15, 0);
}

/* UART4 init function */
static void MX_UART4_Init(void)
{

  huart4.Instance = UART4;
  huart4.Init.BaudRate = 500000;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart4) != HAL_OK)
  {
    Error_Handler();
  }

}

/* USART2 init function */
static void MX_USART2_UART_Init(void)
{

  huart2.Instance = USART2;
  huart2.Init.BaudRate = 921600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }

}

/* USART3 init function */
static void MX_USART3_UART_Init(void)
{

  huart3.Instance = USART3;
  huart3.Init.BaudRate = 921600;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }

}

/* USART6 init function */
static void MX_USART6_UART_Init(void)
{

  huart6.Instance = USART6;
  huart6.Init.BaudRate = 500000;
  huart6.Init.WordLength = UART_WORDLENGTH_8B;
  huart6.Init.StopBits = UART_STOPBITS_1;
  huart6.Init.Parity = UART_PARITY_NONE;
  huart6.Init.Mode = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart6) != HAL_OK)
  {
    Error_Handler();
  }

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{
  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);
  /* DMA1_Stream2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream2_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream2_IRQn);
  /* DMA1_Stream3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);
  /* DMA1_Stream4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
  /* DMA1_Stream5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
  /* DMA1_Stream6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);
  /* DMA2_Stream1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);
  /* DMA2_Stream6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream6_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);

}

/** Configure pins as
        * Analog
        * Input
        * Output
        * EVENT_OUT
        * EXTI
*/
static void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct;

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin : Foot_Switch_Pin */
  GPIO_InitStruct.Pin = Foot_Switch_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(Foot_Switch_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : GPIO_MISC1_Pin GPIO_MISC2_Pin */
  GPIO_InitStruct.Pin = GPIO_MISC1_Pin|GPIO_MISC2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_MISC1_Pin|GPIO_MISC2_Pin, GPIO_PIN_RESET);

}

/* USER CODE BEGIN 4 */

void SetupBinarySemaphores(void){
        PCRXHandle = xSemaphoreCreateBinary();
        PCTXHandle = xSemaphoreCreateBinary();

        TXMotorM1Handle = xSemaphoreCreateBinary();
        TXMotorM2Handle = xSemaphoreCreateBinary();
        RXMotorM1Handle = xSemaphoreCreateBinary();
        RXMotorM2Handle = xSemaphoreCreateBinary();

        ControlM1Handle = xSemaphoreCreateBinary();
        ControlM2Handle = xSemaphoreCreateBinary();
}

//! Byte swap int
int32_t swap_int32( int32_t val )
{
        val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF );
        return (val << 16) | ((val >> 16) & 0xFFFF);
}

void BaseCommandCompile(uint8_t n, uint8_t SeqBits, uint8_t ComBits, uint8_t INDOFF1, uint8_t INDOFF2, uint8_t *DATA, uint8_t LEN, uint8_t SNIP_LEN){
        memset(&BaseCommand[n], 0, sizeof(BaseCommand[0]));

        //ComBits = 0x02 for set and 0x01 for read
        //SeqBits = 0bXXXX according to op-code

        BaseCommand[n].START[0] = 0xA5;
        BaseCommand[n].START[1] = 0x3F;
        BaseCommand[n].CB = (SeqBits<<2 | ComBits);
        BaseCommand[n].INDOFF[0] = INDOFF1;
        BaseCommand[n].INDOFF[1] = INDOFF2;
        BaseCommand[n].LEN = LEN;
        CALC_CRCBase = crcCalc(BaseCommand[n].START, 0, 6, 1);
        WORDtoBYTEBase.HALFWORD = CALC_CRCBase;
        BaseCommand[n].CRC1[0] = WORDtoBYTEBase.BYTE[1];
        BaseCommand[n].CRC1[1] = WORDtoBYTEBase.BYTE[0];

        if(DATA != NULL) {
                for(int i = 0; i<LEN*2; i++) {
                        BaseCommand[n].DATA[i] = DATA[i];
                }
                CALC_CRCBase = crcCalc(BaseCommand[n].DATA, 0, LEN*2, 1);
                WORDtoBYTEBase.HALFWORD = CALC_CRCBase;
                BaseCommand[n].CRC2[0] = WORDtoBYTEBase.BYTE[1];
                BaseCommand[n].CRC2[1] = WORDtoBYTEBase.BYTE[0];
        }

        SNIP = SNIP_LEN;

        memcpy(&BaseCommand[n].DATA[4-SNIP], BaseCommand[n].CRC2, 2);

        if(SNIP==2) {
                memset(&BaseCommand[n].CRC2, 0, 2);
        }
        if(SNIP==4) {
                memset(&BaseCommand[n].DATA, 0, 4);
                memset(&BaseCommand[n].CRC2, 0, 2);
        }
}

void TransmitM1_DMA(uint8_t *data, uint8_t size){
        if(HAL_UART_Transmit_DMA(&M1_UART, data, size) != HAL_OK) { Error_Handler(); }
}

void ReceiveM1_DMA(uint8_t *data, uint8_t size){
        if(HAL_UART_Receive_DMA(&M1_UART, data, size) != HAL_OK) { Error_Handler(); }
}

void TransmitM2_DMA(uint8_t *data, uint8_t size){
        if(HAL_UART_Transmit_DMA(&M2_UART, data, size) != HAL_OK) { Error_Handler(); }
}

void ReceiveM2_DMA(uint8_t *data, uint8_t size){
        if(HAL_UART_Receive_DMA(&M2_UART, data, size) != HAL_OK) { Error_Handler(); }
}

//Select Call-backs functions called after Transfer complete
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart){
        __NOP();
}

//void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
//	if(GPIO_Pin == Foot_Switch_Pin){
//		FOOT_TRIGGER = 1;
//	}
//}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
        BaseType_t xHigherPriorityTaskWoken;
        xHigherPriorityTaskWoken = pdFALSE;

        if(huart->Instance == UART4) {
                xSemaphoreGiveFromISR( PCRXHandle, &xHigherPriorityTaskWoken );
        }

//        if(huart->Instance == USART2 && __HAL_USART_GET_FLAG(huart, USART_FLAG_IDLE)) {
//                __HAL_USART_CLEAR_IDLEFLAG(huart);
//                xSemaphoreGiveFromISR( RXMotorM1Handle, &xHigherPriorityTaskWoken );
//        }
//
//        if(huart->Instance == USART3 && __HAL_USART_GET_FLAG(huart, USART_FLAG_IDLE)) {
//                __HAL_USART_CLEAR_IDLEFLAG(huart);
//                xSemaphoreGiveFromISR( RXMotorM2Handle, &xHigherPriorityTaskWoken );
//        }

        /* If xHigherPriorityTaskWoken was set to true you
           we should yield.  The actual macro used here is
           port specific. portYIELD_FROM_ISR */
        portEND_SWITCHING_ISR( xHigherPriorityTaskWoken );
}

//http://www.riuson.com/blog/post/stm32-hal-uart-dma-rx-variable-length
//void HAL_UART_RxIdleCallback(UART_HandleTypeDef *huart){
//        BaseType_t xHigherPriorityTaskWoken;
//        xHigherPriorityTaskWoken = pdFALSE;
//
//        if(huart->Instance == UART4 && __HAL_USART_GET_FLAG(huart, USART_FLAG_IDLE)) {
//                __HAL_USART_CLEAR_IDLEFLAG(huart);
//                xSemaphoreGiveFromISR( PCRXHandle, &xHigherPriorityTaskWoken );
//        }
//
////        if(huart->Instance == USART2 && __HAL_USART_GET_FLAG(huart, USART_FLAG_IDLE)) {
////                __HAL_USART_CLEAR_IDLEFLAG(huart);
////                xSemaphoreGiveFromISR( RXMotorM1Handle, &xHigherPriorityTaskWoken );
////        }
////
////        if(huart->Instance == USART3 && __HAL_USART_GET_FLAG(huart, USART_FLAG_IDLE)) {
////                __HAL_USART_CLEAR_IDLEFLAG(huart);
////                xSemaphoreGiveFromISR( RXMotorM2Handle, &xHigherPriorityTaskWoken );
////        }
//
//        /* If xHigherPriorityTaskWoken was set to true you
//           we should yield.  The actual macro used here is
//           port specific. portYIELD_FROM_ISR */
//        portEND_SWITCHING_ISR( xHigherPriorityTaskWoken );
//}

void HAL_UART_EndDMA_RX(UART_HandleTypeDef *huart){
/* Stop UART DMA Rx request if ongoing */
        uint32_t dmarequest = 0x00U;
        dmarequest = HAL_IS_BIT_SET(huart->Instance->CR3, USART_CR3_DMAR);
        if((huart->RxState == HAL_UART_STATE_BUSY_RX) && dmarequest)
        {
                CLEAR_BIT(huart->Instance->CR3, USART_CR3_DMAR);
                /* Abort the UART DMA Rx channel */
                if(huart->hdmarx != NULL)
                {
                        HAL_DMA_Abort(huart->hdmarx);
                }
                /* Disable RXNE, PE and ERR (Frame error, noise error, overrun error) interrupts */
                CLEAR_BIT(huart->Instance->CR1, (USART_CR1_RXNEIE | USART_CR1_PEIE));
                CLEAR_BIT(huart->Instance->CR3, USART_CR3_EIE);
                /* At end of Rx process, restore huart->RxState to Ready */
                huart->RxState = HAL_UART_STATE_READY;
        }
}

float *ForwardKinematics(float phi1, float phi2){
        //function [r,theta] = fcn(phi1,phi2)

        uint8_t valid = 1;
        static float ret[2];

        //phi1 = (phi1*2*PI)/360.0; //To radians
        //phi2 = (phi2*2*PI)/360.0;

        static float l1 = 0.15; //length of upper linkage in m (measured from center of joint of 5 cm diameter)
        static float l2 = 0.3; //length of lower linkage in m (measured from center of joint of 5 cm diameter)

        ret[0] = fabs(-l1*cosf((phi1 + phi2)/2.0) + sqrt(pow(l2,2) - pow(l1,2)*pow(sinf((phi1 + phi2)/2.0),2))); //r
        ret[1] = (phi1 - phi2)/2.0; //theta

        //ret[1] = (ret[1]*360)/(2.0*PI); //To degrees

//        if(phi1*360/(2*PI) > 250 || phi1*360/(2*PI) < 15) { //162 90
//                valid = 0;
//        }
//
//        if(phi2*360/(2*PI) > 250 || phi2*360/(2*PI) < 15) {
//                valid = 0;
//        }

        if(valid) {
                return ret;
        }
        else{
                return NULL;
        }
}

float *InverseKinematics(float r, float theta){
        //function [phi1,phi2] = fcn(r,theta)

        uint8_t valid = 1;
        static float ret[2];

        theta = (theta*2*PI)/360.0;

        static float l1 = 0.15; //length of upper linkage in m (measured from center of joint of 5 cm diameter)
        static float l2 = 0.3; //length of lower linkage in m (measured from center of joint of 5 cm diameter)

        if (r == 0) {r = 0.000001; }

        //float complex cmp1;
        float cmp1 = (pow(r,2) + pow(l1,2) - pow(l2,2))/(2.0*r*l1);

        //float complex cmp2;
        float cmp2 = (pow(r,2) + pow(l1,2) - pow(l2,2))/(2.0*r*l1);

        ret[0] = fabs(PI - acosf(cmp1) + theta); //phi1
        ret[1] = fabs(PI - acosf(cmp2) - theta); //phi2

        ret[0] = (ret[0]*360)/(2.0*PI);
        ret[1] = (ret[1]*360)/(2.0*PI);

        if(valid) {
                return ret;
        }
        else{
                return NULL;
        }
}

/* USER CODE END 4 */

/* StartDefaultTask function */
void StartDefaultTask(void const * argument)
{

  /* USER CODE BEGIN 5 */
        vTaskSuspend( NULL );
        /* Infinite loop */
        for(;; )
        {
                vTaskDelay(500);
        }
  /* USER CODE END 5 */
}

/* StartTXPC function */
void StartTXPC(void const * argument)
{
  /* USER CODE BEGIN StartTXPC */

        memset(PCPacketPTR, 0, sizeof(PCPacket));

        PCPacket.START[0] = 0x7E;
        PCPacket.START[1] = 0x5B;

        PCPacket.STOP[0] = 0x5D;
        PCPacket.STOP[1] = 0x7E;

        uint32_t CALC_CRC;

        union {
                uint32_t WORD;
                uint16_t HALFWORD;
                uint8_t BYTE[4];
        } WORDtoBYTE;

        uint8_t *pxRxedMessage;

        uint8_t CurrentPCPacket[sizeof(PCPacket)];

        /* Infinite loop */
        for(;; )
        {
                xSemaphoreTake( PCTXHandle,  portMAX_DELAY );

                HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_8);

                if(xQueueReceive( ProcessQM1Handle, &pxRxedMessage, 0 )) {
                        memcpy(PCPacket.M1C, pxRxedMessage, 10);
                }

                if(xQueueReceive( ProcessQM2Handle, &pxRxedMessage, 0 )) {
                        memcpy(PCPacket.M2C, pxRxedMessage, 10);
                }

                if(xQueueReceive( ProcessQControlHandle, &pxRxedMessage, 0 )) {
                        memcpy(PCPacket.MISC, pxRxedMessage, sizeof(ControlLogPacket));
                }

                CALC_CRC = crcCalc(&PCPacket.M1C, 0, PAYLOAD_TX + PAYLOAD_MISC, 0);
                WORDtoBYTE.HALFWORD = CALC_CRC;
                PCPacket.CRCCheck[0] = WORDtoBYTE.BYTE[1];
                PCPacket.CRCCheck[1] = WORDtoBYTE.BYTE[0];

                memcpy(CurrentPCPacket, PCPacketPTR, sizeof(PCPacket));

                HAL_UART_Transmit_DMA(&PC_UART, CurrentPCPacket, sizeof(PCPacket));

                PCPacket.StatBIT_1 = 0;
                PCPacket.StatBIT_2 = 0;
                PCPacket.StatBIT_3 = 0;
                PCPacket.StatBIT_4 = 0;
                PCPacket.StatBIT_5 = 0;
                PCPacket.StatBIT_6 = 0;
                PCPacket.StatBIT_7 = 0;
                PCPacket.StatBIT_8 = 0;

                HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_8);
        }
  /* USER CODE END StartTXPC */
}

/* StartRXPC function */
void StartRXPC(void const * argument)
{
  /* USER CODE BEGIN StartRXPC */

        RXPacket.START[0] = 0x7E;
        RXPacket.START[1] = 0x5B;

        RXPacket.STOP[0] = 0x5D;
        RXPacket.STOP[1] = 0x7E;

        int8_t START_INDEX = 0;
        uint32_t CALC_CRC;

        union {
                uint32_t WORD;
                uint16_t HALFWORD;
                uint8_t BYTE[4];
        } WORDtoBYTE;

        uint8_t rcvdCount;

        HAL_Delay(1000);
        //__HAL_DMA_ENABLE_IT(&hdma_uart4_rx, DMA_IT_TC);
        //HAL_DMA_RegisterCallback(&hdma_uart4_rx, HAL_DMA_XFER_CPLT_CB_ID, DMA_XFER_CPLT_Callback);

//                __HAL_USART_CLEAR_IDLEFLAG(&huart4);
//                __HAL_USART_ENABLE_IT(&huart4, USART_IT_IDLE);
//
//                __HAL_USART_CLEAR_IDLEFLAG(&huart3);
//                __HAL_USART_ENABLE_IT(&huart3, USART_IT_IDLE);

        /* Infinite loop */
        for(;; )
        {
                HAL_UART_Receive_DMA(&PC_UART, RXBufPC, sizeof(RXPacket));
                //HAL_DMA_Start(&hdma_uart4_rx, (uint32_t)(&(UART4->DR)), (uint32_t)(&RXBufPC), sizeof(RXPacket));
                if(xSemaphoreTake( PCRXHandle, portMAX_DELAY ) == pdTRUE) {

                        rcvdCount = sizeof(RXPacket);
                        //rcvdCount = sizeof(RXBufPC) - huart4.hdmarx->Instance->NDTR;
                        //HAL_UART_EndDMA_RX(&huart4);

                        //xQueueReset(TransmitM1QHandle);
                        //xQueueReset(TransmitM2QHandle);

                        START_INDEX = findBytes(RXBufPC, rcvdCount, RXPacket.START, 2, 1);
                        if(START_INDEX>=0) {

                                memcpy(RXPacketPTR, &RXBufPC[START_INDEX], sizeof(RXPacket));
                                RX_DATA_VALID = 0;

                                WORDtoBYTE.BYTE[1] = RXPacket.CRCCheck[0];
                                WORDtoBYTE.BYTE[0] = RXPacket.CRCCheck[1];
                                CALC_CRC = crcCalc(&RXPacket.OPCODE, 0, PAYLOAD_RX, 0);

                                if(WORDtoBYTE.HALFWORD==CALC_CRC) {
                                        RX_DATA_VALID = 1;
                                        START = 0;
                                        TRIGGER = 0;
                                        switch(RXPacket.OPCODE) {
                                        case KILL_BRIDGE:
                                                START = 0;
                                                BaseCommandCompile(RXPacket.OPCODE, 0b0001, 0x02, 0x01, 0x00, KILL_BRIDGE_DATA, 1, 2);
                                                BaseCommandPTR = &BaseCommand[RXPacket.OPCODE];
                                                xQueueSendToBack( CommandM1QHandle, &BaseCommandPTR, ( TickType_t ) 5);
                                                xQueueSendToBack( CommandM2QHandle, &BaseCommandPTR, ( TickType_t ) 5);
                                                break;
                                        case WRITE_ENABLE:
                                                BaseCommandPTR = &BaseCommand[RXPacket.OPCODE];
                                                BaseCommandCompile(RXPacket.OPCODE, 0b0010, 0x02, 0x07, 0x00, WRITE_ENABLE_DATA, 1, 2);
                                                xQueueSendToBack( CommandM1QHandle, &BaseCommandPTR, ( TickType_t ) 5);
                                                xQueueSendToBack( CommandM2QHandle, &BaseCommandPTR, ( TickType_t ) 5);
                                                break;
                                        case BRIDGE_ENABLE:
                                                BaseCommandPTR = &BaseCommand[RXPacket.OPCODE];
                                                BaseCommandCompile(RXPacket.OPCODE, 0b0100, 0x02, 0x01, 0x00, BRIDGE_ENABLE_DATA, 1, 2);
                                                xQueueSendToBack( CommandM1QHandle, &BaseCommandPTR, ( TickType_t ) 5);
                                                xQueueSendToBack( CommandM2QHandle, &BaseCommandPTR, ( TickType_t ) 5);
                                                break;
                                        case CURRENT_COMMAND:
                                                BaseCommandPTR = &BaseCommand[RXPacket.OPCODE];
                                                BaseCommandCompile(RXPacket.OPCODE, 0b0011, 0x02, 0x45, 0x02, RXPacket.M1C, 2, 0);
                                                xQueueOverwrite( ICommandM1QHandle, &BaseCommandPTR);

                                                BaseCommandPTR = &BaseCommand[RXPacket.OPCODE+1];
                                                BaseCommandCompile(RXPacket.OPCODE+1, 0b0011, 0x02, 0x45, 0x02, RXPacket.M2C, 2, 0);
                                                xQueueOverwrite( ICommandM2QHandle, &BaseCommandPTR);
                                                break;
                                        case POSITION_COMMAND:
                                                BaseCommandPTR = &BaseCommand[RXPacket.OPCODE];
                                                BaseCommandCompile(RXPacket.OPCODE, 0b1010, 0x02, 0x45, 0x00, RXPacket.M1P, 2, 0);
                                                xQueueOverwrite( PCommandM1QHandle, &BaseCommandPTR);

                                                BaseCommandPTR = &BaseCommand[RXPacket.OPCODE+1];
                                                BaseCommandCompile(RXPacket.OPCODE+1, 0b1010, 0x02, 0x45, 0x00, RXPacket.M2P, 2, 0);
                                                xQueueOverwrite( PCommandM2QHandle, &BaseCommandPTR);
                                                break;
                                        case ZERO_POSITION:
                                                BaseCommandPTR = &BaseCommand[RXPacket.OPCODE];
                                                BaseCommandCompile(RXPacket.OPCODE, 0b0000, 0x02, 0x01, 0x00, ZERO_POSITION_DATA, 2, 0);
                                                xQueueSendToBack( CommandM1QHandle, &BaseCommandPTR, ( TickType_t ) 5);
                                                xQueueSendToBack( CommandM2QHandle, &BaseCommandPTR, ( TickType_t ) 5);
                                                break;
                                        case GAIN_SET:
                                                if(RXPacket.StatBIT_1 == 0) {
                                                        BaseCommandCompile(RXPacket.OPCODE, 0b0000, 0x02, 0x01, 0x01, GAIN_SET_0_DATA, 2, 0);
                                                }
                                                else{
                                                        BaseCommandCompile(RXPacket.OPCODE, 0b0000, 0x02, 0x01, 0x01, GAIN_SET_1_DATA, 2, 0);
                                                }
                                                BaseCommandPTR = &BaseCommand[RXPacket.OPCODE];
                                                xQueueSendToBack( CommandM1QHandle, &BaseCommandPTR, ( TickType_t ) 5);
                                                xQueueSendToBack( CommandM2QHandle, &BaseCommandPTR, ( TickType_t ) 5);
                                                break;
                                        case GAIN_CHANGE_M1:
                                                //PID Position Loop
                                                BaseCommandPTR = &BaseCommand[RXPacket.OPCODE];
                                                BaseCommandCompile(RXPacket.OPCODE, 0b0000, 0x02, 0x38, 0x00, RXPacket.M1C, 2, 0);
                                                xQueueSendToBack( CommandM1QHandle, &BaseCommandPTR, ( TickType_t ) 5);

                                                BaseCommandPTR = &BaseCommand[RXPacket.OPCODE+1];
                                                BaseCommandCompile(RXPacket.OPCODE+1, 0b0000, 0x02, 0x38, 0x02, RXPacket.M1P, 2, 0);
                                                xQueueSendToBack( CommandM1QHandle, &BaseCommandPTR, ( TickType_t ) 5);

                                                BaseCommandPTR = &BaseCommand[RXPacket.OPCODE+2];
                                                BaseCommandCompile(RXPacket.OPCODE+2, 0b0000, 0x02, 0x38, 0x04, RXPacket.M2C, 2, 0);
                                                xQueueSendToBack( CommandM1QHandle, &BaseCommandPTR, ( TickType_t ) 5);
                                                break;
                                        case GAIN_CHANGE_M2:
                                                //PID Position Loop
                                                BaseCommandPTR = &BaseCommand[RXPacket.OPCODE];
                                                BaseCommandCompile(RXPacket.OPCODE, 0b0000, 0x02, 0x38, 0x00, RXPacket.M1C, 2, 0);
                                                xQueueSendToBack( CommandM2QHandle, &BaseCommandPTR, ( TickType_t ) 5);

                                                BaseCommandPTR = &BaseCommand[RXPacket.OPCODE+1];
                                                BaseCommandCompile(RXPacket.OPCODE+1, 0b0000, 0x02, 0x38, 0x02, RXPacket.M1P, 2, 0);
                                                xQueueSendToBack( CommandM2QHandle, &BaseCommandPTR, ( TickType_t ) 5);

                                                BaseCommandPTR = &BaseCommand[RXPacket.OPCODE+2];
                                                BaseCommandCompile(RXPacket.OPCODE+2, 0b0000, 0x02, 0x38, 0x04, RXPacket.M2C, 2, 0);
                                                xQueueSendToBack( CommandM2QHandle, &BaseCommandPTR, ( TickType_t ) 5);
                                                break;
                                        case CONFIG_SET:
                                                if(RXPacket.StatBIT_2 == 0) {
                                                        BaseCommandCompile(RXPacket.OPCODE, 0b1001, 0x02, 0xD1, 0x00, CONFIG_0_SET_DATA, 1, 2);
                                                }
                                                else{
                                                        BaseCommandCompile(RXPacket.OPCODE, 0b1001, 0x02, 0xD1, 0x00, CONFIG_1_SET_DATA, 1, 2);
                                                }
                                                BaseCommandPTR = &BaseCommand[RXPacket.OPCODE];
                                                xQueueSendToBack( CommandM1QHandle, &BaseCommandPTR, ( TickType_t ) 5);
                                                xQueueSendToBack( CommandM2QHandle, &BaseCommandPTR, ( TickType_t ) 5);
                                                break;
                                        case START_CONTROL:
                                                if(RXPacket.StatBIT_3 == 1) {
                                                        START = 1;
                                                }
                                                else{
                                                        START = 0;
                                                }
                                                break;
                                        case TRIGGER_ONESHOT:
                                                TRIGGER = RXPacket.TRIGGER;
                                                START = 1;
                                        default:
                                                break;
                                        }
                                }
                        }

                }
        }
  /* USER CODE END StartRXPC */
}

/* StartHeartbeat function */
void StartHeartbeat(void const * argument)
{
  /* USER CODE BEGIN StartHeartbeat */

        /* Infinite loop */
        for(;; )
        {
                //Read Current
                BaseCommandCompile(READ_CURRENT, 0b1100, 0x01, 0x10, 0x03, NULL, 1, 4);
                BaseCommandPTR = &BaseCommand[READ_CURRENT];
                xQueueSendToBack( TransmitM1QHandle, &BaseCommandPTR, ( TickType_t ) 5);
                xQueueSendToBack( TransmitM2QHandle, &BaseCommandPTR, ( TickType_t ) 5);

                //Read Position
                BaseCommandCompile(READ_POSITION, 0b1111, 0x01, 0x12, 0x00, NULL, 2, 4);
                BaseCommandPTR = &BaseCommand[READ_POSITION];
                xQueueSendToBack( TransmitM1QHandle, &BaseCommandPTR, ( TickType_t ) 5);
                xQueueSendToBack( TransmitM2QHandle, &BaseCommandPTR, ( TickType_t ) 5);

                //Read Velocity
                BaseCommandCompile(READ_VELOCITY, 0b0101, 0x01, 0x11, 0x02, NULL, 2, 4);
                BaseCommandPTR = &BaseCommand[READ_VELOCITY];
                xQueueSendToBack( TransmitM1QHandle, &BaseCommandPTR, ( TickType_t ) 5);
                xQueueSendToBack( TransmitM2QHandle, &BaseCommandPTR, ( TickType_t ) 5);

                xSemaphoreGive(TXMotorM1Handle);
                xSemaphoreGive(TXMotorM2Handle);

                vTaskDelay(Ts);

                xSemaphoreGive(PCTXHandle);
        }
  /* USER CODE END StartHeartbeat */
}

/* StartTXMotor1 function */
void StartTXMotor1(void const * argument)
{
  /* USER CODE BEGIN StartTXMotor1 */
        uint8_t *pxRxedMessage;
        /* Infinite loop */
        for(;; )
        {
                xSemaphoreTake( TXMotorM1Handle, portMAX_DELAY );
                __HAL_UART_FLUSH_DRREGISTER(&M1_UART);
                memset(RXBufM1, 0, sizeof(RXBufM1));
                HAL_UART_Receive_DMA(&M1_UART, RXBufM1, sizeof(RXBufM1));

                while(uxQueueMessagesWaiting( CommandM1QHandle )) {
                        xQueueReceive( CommandM1QHandle, &( pxRxedMessage ), portMAX_DELAY);
                        TransmitM1_DMA(pxRxedMessage, sizeof(BaseCommand[0]));
                        vTaskDelay(Td);
                }

                if(xQueueReceive( ICommandM1QHandle, &( pxRxedMessage ), 0)) {
                        TransmitM1_DMA(pxRxedMessage, sizeof(BaseCommand[0]));
                        vTaskDelay(Td);
                }

                else if (xQueueReceive( PCommandM1QHandle, &( pxRxedMessage ), 0)) {
                        TransmitM1_DMA(pxRxedMessage, sizeof(BaseCommand[0]));
                        vTaskDelay(Td);
                }

                while(uxQueueMessagesWaiting( TransmitM1QHandle )) {
                        xQueueReceive( TransmitM1QHandle, &( pxRxedMessage ), portMAX_DELAY);
                        TransmitM1_DMA(pxRxedMessage, sizeof(BaseCommand[0]));
                        //while(huart2.gState != HAL_UART_STATE_READY);
                        vTaskDelay(Td);
                        if(uxQueueMessagesWaiting( TransmitM2QHandle )>0) {vTaskDelay(Td); }
                        else{xSemaphoreGive( RXMotorM1Handle ); }
                }
        }
  /* USER CODE END StartTXMotor1 */
}

/* StartTXMotor2 function */
void StartTXMotor2(void const * argument)
{
  /* USER CODE BEGIN StartTXMotor2 */
        uint8_t *pxRxedMessage;
        /* Infinite loop */
        for(;; )
        {
                xSemaphoreTake( TXMotorM2Handle, portMAX_DELAY );
                __HAL_UART_FLUSH_DRREGISTER(&M2_UART);
                memset(RXBufM2, 0, sizeof(RXBufM2));
                HAL_UART_Receive_DMA(&M2_UART, RXBufM2, sizeof(RXBufM2));

                while(uxQueueMessagesWaiting( CommandM2QHandle )) {
                        xQueueReceive( CommandM2QHandle, &( pxRxedMessage ), portMAX_DELAY);
                        TransmitM2_DMA(pxRxedMessage, sizeof(BaseCommand[0]));
                        vTaskDelay(Td);
                }

                if(xQueueReceive( ICommandM2QHandle, &( pxRxedMessage ), 0)) {
                        TransmitM2_DMA(pxRxedMessage, sizeof(BaseCommand[0]));
                        vTaskDelay(Td);
                }

                else if(xQueueReceive( PCommandM2QHandle, &( pxRxedMessage ), 0)) {
                        TransmitM2_DMA(pxRxedMessage, sizeof(BaseCommand[0]));
                        vTaskDelay(Td);
                }

                while(uxQueueMessagesWaiting( TransmitM2QHandle )) {
                        xQueueReceive( TransmitM2QHandle, &( pxRxedMessage ), portMAX_DELAY);
                        TransmitM2_DMA(pxRxedMessage, sizeof(BaseCommand[0]));
                        vTaskDelay(Td);
                        if(uxQueueMessagesWaiting( TransmitM2QHandle )>0) {vTaskDelay(Td); }
                        else{xSemaphoreGive( RXMotorM2Handle ); }
//                        HAL_UART_DMAResume(&huart3);
//                        vTaskSetTimeOutState( &xTimeOut );
//                        while(!__HAL_USART_GET_FLAG(&huart3, USART_FLAG_IDLE) && !xTaskCheckForTimeOut( &xTimeOut, 5 ));
//                        __HAL_USART_CLEAR_IDLEFLAG(&huart3);
//                        HAL_UART_DMAPause(&huart3);
//                        while(huart3.gState != HAL_UART_STATE_READY);

//                        while(huart3.gState != HAL_UART_STATE_READY);
//                        do{
//                            volatile uint32_t tmpreg = 0x00U;
//                            tmpreg = (&huart3)->Instance->SR;
//                            ((void)(tmpreg));
//                          } while(0);
                }

        }
  /* USER CODE END StartTXMotor2 */
}

/* StartRXMotor1 function */
void StartRXMotor1(void const * argument)
{
  /* USER CODE BEGIN StartRXMotor1 */

        uint8_t PCBuf[10] = {0};
        uint8_t *PCBufPTR;


        uint8_t OPCODE;
        uint8_t START_BYTE[2] = {0xA5, 0xFF};
        uint8_t START_SIZE = 2;
        uint8_t DATA_SIZE;
        uint8_t START_INDEX;
        uint32_t CALC_CRC;

        uint8_t INDEX_SIZE = 0;
        uint8_t INDEX[5] = {0};

        union {
                uint32_t WORD;
                uint16_t HALFWORD;
                uint8_t BYTE[4];
        } WORDtoBYTE;

        struct __attribute__((__packed__)) CURRENTStruct {
                uint8_t HEAD[8];
                uint8_t DATA[2];
                uint8_t CRC2[2];
        };

        struct __attribute__((__packed__)) POSITIONStruct {
                uint8_t HEAD[8];
                uint8_t DATA[4];
                uint8_t CRC2[2];
        };

        struct __attribute__((__packed__)) VELOCITYStruct {
                uint8_t HEAD[8];
                uint8_t DATA[4];
                uint8_t CRC2[2];
        };

        struct CURRENTStruct CURRENTrx;
        struct POSITIONStruct POSITIONrx;
        struct VELOCITYStruct VELOCITYrx;
        uint8_t *CURRENTrxPTR = (uint8_t*)&CURRENTrx;
        uint8_t *POSITIONrxPTR = (uint8_t*)&POSITIONrx;
        uint8_t *VELOCITYrxPTR = (uint8_t*)&VELOCITYrx;

        uint8_t rcvdCount;
        /* Infinite loop */
        for(;; )
        {
                //vTaskSuspend(NULL);
                xSemaphoreTake( RXMotorM1Handle, portMAX_DELAY );
                START_BYTE[0] = 0xA5;
                START_BYTE[1] = 0xFF;

                rcvdCount = sizeof(RXBufM1) - M1_UART.hdmarx->Instance->NDTR;
                HAL_UART_EndDMA_RX(&M1_UART);
                //__HAL_UART_FLUSH_DRREGISTER(&huart2);

                if(rcvdCount>0) {

                        INDEX_SIZE = findMultipleBytes(RXBufM1, rcvdCount, START_BYTE, START_SIZE, INDEX, sizeof(INDEX));

                        for(int i=0; i<INDEX_SIZE; i++) {
                                OPCODE = (RXBufM1[INDEX[i]+2] & 0b00111100)>>2;
                                START_INDEX = INDEX[i];
                                switch(OPCODE)
                                {
                                case 0b0011: //Current_Set
                                        break;
                                case 0b1100: //Current_Data
                                        DATA_SIZE = 2;
                                        memcpy(CURRENTrxPTR, &RXBufM1[START_INDEX], rcvdCount);
                                        WORDtoBYTE.BYTE[1] = CURRENTrx.CRC2[0];
                                        WORDtoBYTE.BYTE[0] = CURRENTrx.CRC2[1];
                                        CALC_CRC = crcCalc(CURRENTrx.DATA, 0, DATA_SIZE, 1);
                                        if(WORDtoBYTE.HALFWORD==CALC_CRC) {
                                                appendBytes(PCBuf, 10, 0, CURRENTrx.DATA, 0, DATA_SIZE);
                                                PCPacket.StatBIT_1 = 1;
                                        }
                                        break;
                                case 0b1111: //Position_Data
                                        DATA_SIZE = 4;
                                        memcpy(POSITIONrxPTR, &RXBufM1[START_INDEX], rcvdCount);
                                        WORDtoBYTE.BYTE[1] = POSITIONrx.CRC2[0];
                                        WORDtoBYTE.BYTE[0] = POSITIONrx.CRC2[1];
                                        CALC_CRC = crcCalc(POSITIONrx.DATA, 0, DATA_SIZE, 1);
                                        if(WORDtoBYTE.HALFWORD==CALC_CRC) {
                                                appendBytes(PCBuf, 10, 2, POSITIONrx.DATA, 0, DATA_SIZE);
                                                PCPacket.StatBIT_2 = 1;
                                        }
                                        break;
                                case 0b0101: //Velocity_Data
                                        DATA_SIZE = 4;
                                        memcpy(VELOCITYrxPTR, &RXBufM1[START_INDEX], rcvdCount);
                                        WORDtoBYTE.BYTE[1] = VELOCITYrx.CRC2[0];
                                        WORDtoBYTE.BYTE[0] = VELOCITYrx.CRC2[1];
                                        CALC_CRC = crcCalc(VELOCITYrx.DATA, 0, DATA_SIZE, 1);
                                        if(WORDtoBYTE.HALFWORD==CALC_CRC) {
                                                appendBytes(PCBuf, 10, 2 + 4, VELOCITYrx.DATA, 0, DATA_SIZE);
                                                PCPacket.StatBIT_3 = 1;
                                        }
                                        break;
                                default:
                                        break;
                                }
                        }
                }


                PCBufPTR = &PCBuf;
                if(PCPacket.StatBIT_1 && PCPacket.StatBIT_2 && PCPacket.StatBIT_3) {
                        xQueueOverwrite(ProcessQM1Handle, &PCBufPTR);
                }
                if(PCPacket.StatBIT_1 && PCPacket.StatBIT_2 && PCPacket.StatBIT_3) {
                        xQueueOverwrite(ControlM1QHandle, &PCBufPTR);
                }
        }
  /* USER CODE END StartRXMotor1 */
}

/* StartRXMotor2 function */
void StartRXMotor2(void const * argument)
{
  /* USER CODE BEGIN StartRXMotor2 */

        uint8_t PCBuf[10] = {0};
        uint8_t *PCBufPTR;

        uint8_t OPCODE;
        uint8_t START_BYTE[2] = {0xA5, 0xFF};
        uint8_t START_SIZE = 2;
        uint8_t DATA_SIZE;
        uint8_t START_INDEX;
        uint32_t CALC_CRC;

        uint8_t INDEX_SIZE = 0;
        uint8_t INDEX[5] = {0};

        union {
                uint32_t WORD;
                uint16_t HALFWORD;
                uint8_t BYTE[4];
        } WORDtoBYTE;

        struct __attribute__((__packed__)) CURRENTStruct {
                uint8_t HEAD[8];
                uint8_t DATA[2];
                uint8_t CRC2[2];
        };

        struct __attribute__((__packed__)) POSITIONStruct {
                uint8_t HEAD[8];
                uint8_t DATA[4];
                uint8_t CRC2[2];
        };

        struct __attribute__((__packed__)) VELOCITYStruct {
                uint8_t HEAD[8];
                uint8_t DATA[4];
                uint8_t CRC2[2];
        };

        struct CURRENTStruct CURRENTrx;
        struct POSITIONStruct POSITIONrx;
        struct VELOCITYStruct VELOCITYrx;
        uint8_t *CURRENTrxPTR = (uint8_t*)&CURRENTrx;
        uint8_t *POSITIONrxPTR = (uint8_t*)&POSITIONrx;
        uint8_t *VELOCITYrxPTR = (uint8_t*)&VELOCITYrx;

        uint8_t rcvdCount;
        /* Infinite loop */
        for(;; )
        {
                //vTaskSuspend(NULL);

                xSemaphoreTake( RXMotorM2Handle, portMAX_DELAY );
                START_BYTE[0] = 0xA5;
                START_BYTE[1] = 0xFF;

                rcvdCount = sizeof(RXBufM2) - M2_UART.hdmarx->Instance->NDTR;
                HAL_UART_EndDMA_RX(&M2_UART);
                //__HAL_UART_FLUSH_DRREGISTER(&huart3);

                if(rcvdCount>0) {

                        INDEX_SIZE = findMultipleBytes(RXBufM2, rcvdCount, START_BYTE, START_SIZE, INDEX, sizeof(INDEX));

                        for(int i=0; i<INDEX_SIZE; i++) {
                                OPCODE = (RXBufM2[INDEX[i]+2] & 0b00111100)>>2;
                                START_INDEX = INDEX[i];
                                switch(OPCODE)
                                {
                                case 0b0011: //Current_Set
                                        break;
                                case 0b1100: //Current_Data
                                        DATA_SIZE = 2;
                                        memcpy(CURRENTrxPTR, &RXBufM2[START_INDEX], rcvdCount);
                                        WORDtoBYTE.BYTE[1] = CURRENTrx.CRC2[0];
                                        WORDtoBYTE.BYTE[0] = CURRENTrx.CRC2[1];
                                        CALC_CRC = crcCalc(CURRENTrx.DATA, 0, DATA_SIZE, 1);
                                        if(WORDtoBYTE.HALFWORD==CALC_CRC) {
                                                appendBytes(PCBuf, 10, 0, CURRENTrx.DATA, 0, DATA_SIZE);
                                                PCPacket.StatBIT_4 = 1;
                                                HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_9);
                                        }
                                        break;
                                case 0b1111: //Position_Data
                                        DATA_SIZE = 4;
                                        memcpy(POSITIONrxPTR, &RXBufM2[START_INDEX], rcvdCount);
                                        WORDtoBYTE.BYTE[1] = POSITIONrx.CRC2[0];
                                        WORDtoBYTE.BYTE[0] = POSITIONrx.CRC2[1];
                                        CALC_CRC = crcCalc(POSITIONrx.DATA, 0, DATA_SIZE, 1);
                                        if(WORDtoBYTE.HALFWORD==CALC_CRC) {
                                                appendBytes(PCBuf, 10, 2, POSITIONrx.DATA, 0, DATA_SIZE);
                                                PCPacket.StatBIT_5 = 1;
                                                HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_9);
                                        }
                                        break;
                                case 0b0101: //Velocity_Data
                                        DATA_SIZE = 4;
                                        memcpy(VELOCITYrxPTR, &RXBufM2[START_INDEX], rcvdCount);
                                        WORDtoBYTE.BYTE[1] = VELOCITYrx.CRC2[0];
                                        WORDtoBYTE.BYTE[0] = VELOCITYrx.CRC2[1];
                                        CALC_CRC = crcCalc(VELOCITYrx.DATA, 0, DATA_SIZE, 1);
                                        if(WORDtoBYTE.HALFWORD==CALC_CRC) {
                                                appendBytes(PCBuf, 10, 2 + 4, VELOCITYrx.DATA, 0, DATA_SIZE);
                                                PCPacket.StatBIT_6 = 1;
                                                HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_9);
                                        }
                                        break;
                                default:
                                        break;
                                }
                        }

                }

                PCBufPTR = &PCBuf;
                if(PCPacket.StatBIT_4 && PCPacket.StatBIT_5 && PCPacket.StatBIT_6) {
                        xQueueOverwrite(ProcessQM2Handle, &PCBufPTR);
                        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_9);
                }
                if(PCPacket.StatBIT_4 && PCPacket.StatBIT_5 && PCPacket.StatBIT_6) {
                        xQueueOverwrite(ControlM2QHandle, &PCBufPTR);
                }

        }
  /* USER CODE END StartRXMotor2 */
}

/* StartController function */
void StartController(void const * argument)
{
  /* USER CODE BEGIN StartController */

        union {
                float FLOAT;
                int32_t INT32;
                int16_t INT16;
                uint8_t BYTE[4];
        } F2B;

        float *ret;

        //Negative position feedback
        float Error_r = 0;
        float Error_r_CV = 0;
        float Error_r_PV = 0;

        float Error_s = 0;
        float Error_s_CV = 0;
        float Error_s_PV = 0;

        //Spring feedback
        float r_fbk = 0;
        float r_cmd = 0.3;
        float r_d_fbk = 0;
        float r_d_cmd = 0;

        float theta_fbk = 0;
        float theta_d_fbk = 0;

        float s_fbk = 0;
        float s_cmd = 0;
        float s_d_fbk = 0;
        float s_d_cmd = 0;

        float r_fbk_prev = 0;
        float theta_fbk_prev = 0;

        //Motor feedback
        float I_cmd[2] = {0};
        float I_fbk[2] = {0};

        float phi1 = 0;
        float dphi1 = 0;
        float phi2 = 0;
        float dphi2 = 0;

        //Virtual compliance control
        float JT[2][2] = {0};
        float J[2][2] = {0};
        float F[2] = {0};
        float Tau[2] = {0};

        //Spring damper constants
        float ks_s = 200;
        float kd_s = 30;

        float ks_r = 200; //200
        float kd_r = 30; //30

        //Virtual joint compliance
        float f_j_1 = 0;
        float f_j_2 = 0;
        float ks_j = 0;
        float kd_j = 0;

        float phi1_cmd = 0;
        float phi2_cmd = 0;
        float dphi1_cmd = 0;
        float dphi2_cmd = 0;

        //Weighting
        float k_r = 0;
        float k_s = 0;

        float ki_r = 0;
        float ki_s = 0;

        //Foot forces
        float f_r = 0;
        float f_s = 0;

        //Motor constants
        float k_i = 0.08; //0.119

        //Trajectory
        int i_traj = 0;

        uint8_t valid = 1;
        uint8_t *pxRxedMessage;

        /* Infinite loop */
        for(;; )
        {

                if(uxQueueMessagesWaiting(ControlM1QHandle) && uxQueueMessagesWaiting(ControlM2QHandle)) {

                        //Data from Drivers
                        if(xQueueReceive(ControlM1QHandle, &pxRxedMessage, 0 )) {
                                memcpy(ControlPacket.M1C, pxRxedMessage, 10);
                        }
                        if(xQueueReceive(ControlM2QHandle, &pxRxedMessage, 0 )) {
                                memcpy(ControlPacket.M2C, pxRxedMessage, 10);
                        }

                        //Data from PC
                        if(RX_DATA_VALID) {
                                RX_DATA_VALID = 0;
                                r_cmd = RXPacket.r_cmd;
                                s_cmd = RXPacket.s_cmd;
                                k_r = RXPacket.k_r;
                                k_s = RXPacket.k_s;
                                ki_r = RXPacket.ki_r;
                                ki_s = RXPacket.ki_s;
                                ks_r = RXPacket.kr_s;
                                kd_r = RXPacket.kr_d;
                                ks_s = RXPacket.ks_s;
                                kd_s = RXPacket.ks_d;
                                k_i = RXPacket.k_i;
                        }

                        if(START==1) {
                                valid = 1;
                                HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_8);

                                memcpy(F2B.BYTE, ControlPacket.M1C, 2);
                                I_fbk[0] = F2B.INT16/(pow(2.0,13)/60.0);

                                memcpy(F2B.BYTE, ControlPacket.M1P, 4);
                                phi1 = (F2B.INT32/(4*250.0) - 1)*(-180.0)*(2*PI/360.0);

                                memcpy(F2B.BYTE, ControlPacket.M1V, 4);
                                dphi1 = -(F2B.INT32/(pow(2.0,17)/20000.0))*(1/2000.0)*60.0*(2*PI/60.0);

                                memcpy(F2B.BYTE, ControlPacket.M2C, 2);
                                I_fbk[1] = F2B.INT16/(pow(2.0,13)/60.0);

                                memcpy(F2B.BYTE, ControlPacket.M2P, 4);
                                phi2 = (F2B.INT32/(4*250.0) + 1)*180.0*(2*PI/360.0);

                                memcpy(F2B.BYTE, ControlPacket.M2V, 4);
                                dphi2 = (F2B.INT32/(pow(2.0,17)/20000.0))*(1/2000.0)*60.0*(2*PI/60.0);

                                //Forward kinematic mapping
                                ret = ForwardKinematics(phi1, phi2);
                                if(ret == NULL) {
                                        valid = 0;
                                        START = 0;
                                        BaseCommandCompile(RXPacket.OPCODE, 0b0001, 0x02, 0x01, 0x00, KILL_BRIDGE_DATA, 1, 2);
                                        BaseCommandPTR = &BaseCommand[RXPacket.OPCODE];
                                        xQueueSendToBack( CommandM1QHandle, &BaseCommandPTR, ( TickType_t ) 5);
                                        xQueueSendToBack( CommandM2QHandle, &BaseCommandPTR, ( TickType_t ) 5);
                                }
                                r_fbk = ret[0];
                                theta_fbk = ret[1];

                                //Velocity mapping
//                                r_d_fbk = dphi1*((3*sinf(phi1/2.0 + phi2/2.0))/40 - (9*cosf(phi1/2.0 + phi2/2.0)*sinf(phi1/2.0 + phi2/2.0))/(800*pow(9/100.0 - (9*pow(sinf(phi1/2.0 + phi2/2.0),2)/400.0),0.5)))
//                                          + dphi2*((3*sinf(phi1/2.0 + phi2/2))/40 - (9*cosf(phi1/2.0 + phi2/2.0)*sinf(phi1/2.0 + phi2/2.0))/(800*pow(9/100.0 - (9*pow(sinf(phi1/2.0 + phi2/2.0),2)/400.0),0.5)));
//                                theta_d_fbk = dphi1/2.0 - dphi2/2.0;
//                                r_d_fbk = (r_fbk - r_fbk_prev)/0.005;
//                                theta_d_fbk = (theta_fbk - theta_fbk_prev)/0.005;
//                                r_fbk_prev = r_fbk;
//                                theta_fbk_prev = theta_fbk;

                                //Control
                                //Theta
//                                JT[0][0] = (3*sinf(phi1/2.0 + phi2/2.0))/40.0 - (9*cosf(phi2/2.0 + phi2/2.0)*sinf(phi1/2.0 + phi2/2.0))/(800*pow((9/100.0 - (9*pow(sinf(phi1/2.0 + phi2/2.0),2))/400.0),0.5));
//                                JT[0][1] = 0.5;
//                                JT[1][0] = (3*sinf(phi1/2.0 + phi2/2.0))/40.0 - (9*cosf(phi2/2.0 + phi2/2.0)*sinf(phi1/2.0 + phi2/2.0))/(800*pow((9/100.0 - (9*pow(sinf(phi1/2.0 + phi2/2.0),2))/400.0),0.5));
//                                JT[1][1] = -0.5;

                                //Arc-length
//                                JT[0][0] = (3*sinf(phi1/2.0 + phi2/2.0))/40.0 - (9*cosf(phi1/2.0 + phi2/2.0)*sinf(phi1/2.0 + phi2/2.0))/(800*pow((9/100.0 - (9*pow(sinf(phi1/2.0 + phi2/2.0),2.0))/400.0),(1/2.0)));
//                                JT[0][1] = (((3*sinf(phi1/2.0 + phi2/2.0))/40.0 - (9*cosf(phi1/2.0 + phi2/2.0)*sinf(phi1/2.0 + phi2/2.0))/(800*pow((9/100.0 - (9*pow(sinf(phi1/2.0 + phi2/2.0),2.0))/400.0),(1/2.0))))*(phi1 - phi2))/2.0 - (3*cosf(phi1/2.0 + phi2/2.0))/40 + pow((9/100.0 - (9*pow(sinf(phi1/2.0 + phi2/2.0),2))/400.0),(1/2.0))/2.0;
//                                JT[1][0] = (3*sinf(phi1/2.0 + phi2/2.0))/40.0 - (9*cosf(phi1/2.0 + phi2/2.0)*sinf(phi1/2.0 + phi2/2.0))/(800*pow((9/100.0 - (9*pow(sinf(phi1/2.0 + phi2/2.0),2))/400.0),(1/2.0)));
//                                JT[1][1] = (3*cosf(phi1/2.0 + phi2/2.0))/40.0 + (((3*sinf(phi1/2.0 + phi2/2.0))/40.0 - (9*cosf(phi1/2.0 + phi2/2.0)*sinf(phi1/2.0 + phi2/2.0))/(800*pow((9/100.0 - (9*pow(sinf(phi1/2.0 + phi2/2.0),2.0))/400.0),(1/2.0))))*(phi1 - phi2))/2.0 - pow((9/100.0 - (9*pow(sinf(phi1/2.0 + phi2/2.0),2.0))/400.0),(1/2.0))/2.0;

                                JT[0][0] = sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*(3.0/4.0E1)-cosf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*1.0/sqrt(pow(sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0)),2.0)*(-9.0/4.0E2)+9.0/1.0E2)*(9.0/8.0E2);
                                JT[0][1] = cosf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*(-3.0/4.0E1)+(sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*(3.0/4.0E1)-cosf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*1.0/sqrt(pow(sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0)),2.0)*(-9.0/4.0E2)+9.0/1.0E2)*(9.0/8.0E2))*(phi1-phi2)*(1.0/2.0)+sqrt(pow(sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0)),2.0)*(-9.0/4.0E2)+9.0/1.0E2)*(1.0/2.0);
                                JT[1][0] = sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*(3.0/4.0E1)-cosf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*1.0/sqrt(pow(sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0)),2.0)*(-9.0/4.0E2)+9.0/1.0E2)*(9.0/8.0E2);
                                JT[1][1] = cosf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*(3.0/4.0E1)+(sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*(3.0/4.0E1)-cosf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*1.0/sqrt(pow(sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0)),2.0)*(-9.0/4.0E2)+9.0/1.0E2)*(9.0/8.0E2))*(phi1-phi2)*(1.0/2.0)-sqrt(pow(sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0)),2.0)*(-9.0/4.0E2)+9.0/1.0E2)*(1.0/2.0);

//                                J[0][0] = sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*(3.0/4.0E1)-cosf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*1.0/sqrt(pow(sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0)),2.0)*(-9.0/4.0E2)+9.0/1.0E2)*(9.0/8.0E2);
//                                J[0][1] = sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*(3.0/4.0E1)-cosf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*1.0/sqrt(pow(sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0)),2.0)*(-9.0/4.0E2)+9.0/1.0E2)*(9.0/8.0E2);
//                                J[1][0] = cosf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*(-3.0/4.0E1)+(sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*(3.0/4.0E1)-cosf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*1.0/sqrt(pow(sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0)),2.0)*(-9.0/4.0E2)+9.0/1.0E2)*(9.0/8.0E2))*(phi1-phi2)*(1.0/2.0)+sqrt(pow(sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0)),2.0)*(-9.0/4.0E2)+9.0/1.0E2)*(1.0/2.0);
//                                J[1][1] = cosf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*(3.0/4.0E1)+(sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*(3.0/4.0E1)-cosf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*1.0/sqrt(pow(sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0)),2.0)*(-9.0/4.0E2)+9.0/1.0E2)*(9.0/8.0E2))*(phi1-phi2)*(1.0/2.0)-sqrt(pow(sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0)),2.0)*(-9.0/4.0E2)+9.0/1.0E2)*(1.0/2.0);

                                J[0][0] = sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*(3.0/4.0E1)-cosf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*1.0/sqrt(pow(sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0)),2.0)*(-9.0/4.0E2)+9.0/1.0E2)*(9.0/8.0E2);
                                J[0][1] = sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*(3.0/4.0E1)-cosf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*1.0/sqrt(pow(sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0)),2.0)*(-9.0/4.0E2)+9.0/1.0E2)*(9.0/8.0E2);
                                J[1][0] = cosf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*(-3.0/4.0E1)+(sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*(3.0/4.0E1)-cosf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*1.0/sqrt(pow(sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0)),2.0)*(-9.0/4.0E2)+9.0/1.0E2)*(9.0/8.0E2))*(phi1-phi2)*(1.0/2.0)+sqrt(pow(sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0)),2.0)*(-9.0/4.0E2)+9.0/1.0E2)*(1.0/2.0);
                                J[1][1] = cosf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*(3.0/4.0E1)+(sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*(3.0/4.0E1)-cosf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0))*1.0/sqrt(pow(sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0)),2.0)*(-9.0/4.0E2)+9.0/1.0E2)*(9.0/8.0E2))*(phi1-phi2)*(1.0/2.0)-sqrt(pow(sinf(phi1*(1.0/2.0)+phi2*(1.0/2.0)),2.0)*(-9.0/4.0E2)+9.0/1.0E2)*(1.0/2.0);

                                r_d_fbk = J[0][0]*dphi1 + J[0][1]*dphi2;
                                theta_d_fbk = J[1][0]*dphi1 + J[1][1]*dphi2;

                                //Arc-length = r*theta
                                s_fbk = r_fbk*theta_fbk;
                                s_d_fbk = r_d_fbk*theta_fbk + r_fbk*theta_d_fbk; //chain rule

                                // if(SHOT && r_fbk >= 0.38) {
                                //         r_cmd = 0.3;
                                //
                                //         k_r = 1;
                                //         k_s = 0.1;
                                //         ks_r = 650;
                                //         kd_r = 15;
                                //         ks_s = 400;
                                //         kd_s = 5;
                                //
                                //         SHOT = 0;
                                //         TRIGGER = 1; //DANGEROUS!
                                // }
                                // if(PULLED && (ELAPSED > 1000)) {
                                //         r_cmd = 0.4;
                                //
                                //         k_r = 1;
                                //         k_s = 0.1;
                                //         ks_r = 1726;
                                //         kd_r = 0;
                                //         ks_s = 400;
                                //         kd_s = 5;
                                //
                                //         SHOT = 1;
                                //         PULLED = 0;
                                //         ELAPSED = 0;
                                // }
                                // if(TRIGGER) {
                                //         r_cmd = 0.25;
                                //
                                //         k_r = 1;
                                //         k_s = 0.1;
                                //         ks_r = 650;
                                //         kd_r = 15;
                                //         ks_s = 400;
                                //         kd_s = 5;
                                //
                                //         ELAPSED = 0;
                                //         TRIGGER = 0;
                                //         PULLED = 1;
                                // }
                                // else{
                                //         ELAPSED++;
                                // }


if(TRIGGER){
  i_traj++;

  if(i_traj==4000){
	  TRIGGER = 0;
	  i_traj = 0;
	  goto end;
  }
   r_cmd = r_traj[i_traj];
   s_cmd = r_traj[i_traj]*theta_traj[i_traj];
}
end:

//if(TRIGGER){
//  i_traj++;
//
//  if(i_traj==2002){
//	  TRIGGER = 0;
//	  i_traj = 0;
//	  goto end;
//  }
//   ks_r = ks_traj[i_traj];
//}
//end:

                                //FOOT_TRIGGER = !HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_7);

//                                if(TRIGGER){
//                                	r_cmd = 0.40;
//                                	k_r = 1;
//                                	k_s = 0.1;
//                                	ks_r = 1726;
//									kd_r = 0;
//									ks_s = 300;
//									kd_s = 10;
//									FOOT_TRIGGER = 0;
//                                }
//
//                                if(TRIGGER && r_fbk>=0.4){
//                                	r_cmd = 0.3;
//                                	k_r = 1;
//									k_s = 0.1;
//									ks_r = 650;
//									kd_r = 15;
//									ks_s = 300;
//									kd_s = 10;
//									SHOT = 1;
//                                }

                                //Integral term
                                Error_r_CV = r_cmd - r_fbk;
                                Error_r = Error_r_CV + Error_r_PV;
                                if(Error_r > 1) {Error_r = 1; }
                                if(Error_r < -1) {Error_r = -1; }

                                Error_s_CV = s_cmd - s_fbk;
                                Error_s = Error_s_CV + Error_s_PV;
                                if(Error_s > 1) {Error_s = 1;}
                                if(Error_s < -1) {Error_s = -1;}

                                //Virtual spring dampener
                                f_r = ks_r*(r_fbk - r_cmd) + kd_r*(r_d_fbk - r_d_cmd) - ki_r*(Error_r);
                                f_s = ks_s*(s_fbk - s_cmd) + kd_s*(s_d_fbk - s_d_cmd) - ki_s*(Error_s);
                                Error_r_PV = Error_r_CV;
                                Error_s_PV = Error_s_CV;

                                F[0] = k_r*(f_r);
                                F[1] = k_s*(f_s);

                                Tau[0] = JT[0][0]*F[0] + JT[0][1]*F[1];
                                Tau[1] = JT[1][0]*F[0] + JT[1][1]*F[1];

//                                if(TRIGGER){
//									//Virtual joint compliance
//									ks_j = ks_r;
//									kd_j = kd_r;
//
//									phi1_cmd = 120*(2*PI/360.0);
//									phi2_cmd = 120*(2*PI/360.0);
//
//									f_j_1 = k_r*(ks_j*(phi1 - phi1_cmd) + kd_j*(dphi1 - dphi1_cmd));
//									f_j_2 = k_r*(ks_j*(phi2 - phi2_cmd) + kd_j*(dphi2 - dphi2_cmd));
//
//									Tau[0] = f_j_1;
//									Tau[1] = f_j_2;
//                                }

                                //Motor 1 Control
                                I_cmd[0] = (1/k_i)*Tau[0];
                                if(I_cmd[0] > 58) {I_cmd[0] = 58; }
                                if(I_cmd[0] < -58) {I_cmd[0] = -58; }
                                F2BM1.INT32 = I_cmd[0]*(pow(2.0,15)/60.0);
                                swap_int32( F2BM1.INT32 );
                                BaseCommandPTR = &BaseCommand[CONTROL_CURRENT_M1];
                                BaseCommandCompile(CONTROL_CURRENT_M1, 0b0011, 0x02, 0x45, 0x02, F2BM1.BYTE, 2, 0);
                                if(valid) {xQueueOverwrite( ICommandM1QHandle, &BaseCommandPTR); }

                                //Motor 2 Control
                                I_cmd[1] = -(1/k_i)*Tau[1];
                                if(I_cmd[1] > 58) {I_cmd[1] = 58; }
                                if(I_cmd[1] < -58) {I_cmd[1] = -58; }
                                F2BM2.INT32 = I_cmd[1]*(pow(2.0,15)/60.0);
                                swap_int32( F2BM2.INT32 );
                                BaseCommandPTR = &BaseCommand[CONTROL_CURRENT_M2];
                                BaseCommandCompile(CONTROL_CURRENT_M2, 0b0011, 0x02, 0x45, 0x02, F2BM2.BYTE, 2, 0);
                                if(valid) {xQueueOverwrite( ICommandM2QHandle, &BaseCommandPTR); }

                                //PC Logging
                                ControlLogPacket.I_cmd_0 = I_cmd[0];
                                ControlLogPacket.I_cmd_1 = I_cmd[1];
                                ControlLogPacket.f_r = F[0];
                                ControlLogPacket.f_s = F[1];
                                ControlLogPacket.r_fbk = r_fbk;
                                ControlLogPacket.s_fbk = s_fbk;
                                ControlLogPacket.r_d_fbk = Error_r; //TODO
                                ControlLogPacket.s_d_fbk = Error_s; //TODO
                                xQueueOverwrite(ProcessQControlHandle, &ControlLogPacketPTR);

                                HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_8);
                                xSemaphoreGive( TXMotorM1Handle );
                                xSemaphoreGive( TXMotorM2Handle );
                        }
                }

                //vTaskDelay(Ts);
        }
  /* USER CODE END StartController */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM2 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
/* USER CODE BEGIN Callback 0 */

/* USER CODE END Callback 0 */
  if (htim->Instance == TIM2) {
    HAL_IncTick();
  }
/* USER CODE BEGIN Callback 1 */

/* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler */
        /* User can add his own implementation to report the HAL error return state */
        while(1)
        {
        }
  /* USER CODE END Error_Handler */
}

#ifdef USE_FULL_ASSERT

/**
   * @brief Reports the name of the source file and the source line number
   * where the assert_param error has occurred.
   * @param file: pointer to the source file name
   * @param line: assert_param error line source number
   * @retval None
   */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
        /* User can add his own implementation to report the file name and line number,
           ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */

}

#endif

/**
  * @}
  */

/**
  * @}
*/

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
