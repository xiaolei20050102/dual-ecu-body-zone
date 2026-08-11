/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "tim.h"
#include "motor_driver.h"
#include "motor_port_stm32.h"

#include "encoder_driver.h"
#include "encoder_port_stm32.h"

#include "limit_driver.h"
#include "limit_port_stm32.h"

#include "usart.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
MotorHandle_t g_motor;


static EncoderHandle_t g_encoder;
static EncoderSnapshot_t g_encoder_snapshot;

static LimitHandle_t g_limit;
static LimitSnapshot_t g_limit_snapshot;
/* USER CODE END Variables */
/* Definitions for ControlTask */
osThreadId_t ControlTaskHandle;
const osThreadAttr_t ControlTask_attributes = {
  .name = "ControlTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for SafetyTask */
osThreadId_t SafetyTaskHandle;
const osThreadAttr_t SafetyTask_attributes = {
  .name = "SafetyTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartControlTask(void *argument);
void StartSafetyTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);
void vApplicationMallocFailedHook(void);

/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
   /* Run time stack overflow checking is performed if
   configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook function is
   called if a stack overflow is detected. */
    (void)xTask;
    (void)pcTaskName;

    /* 最后一道安全动作：PWM=0，TB6612 失能 */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0U);
    HAL_GPIO_WritePin(STBY_GPIO_Port, STBY_Pin, GPIO_PIN_RESET);
    /*屏蔽掉可屏蔽的中断*/
    taskDISABLE_INTERRUPTS();
    for (;;)//此时的for就不是任务了，不会进入时间片，他只是死循环
    {
    }	
}
/* USER CODE END 4 */

/* USER CODE BEGIN 5 */
void vApplicationMallocFailedHook(void)
{
   /* vApplicationMallocFailedHook() will only be called if
   configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h. It is a hook
   function that will get called if a call to pvPortMalloc() fails.
   pvPortMalloc() is called internally by the kernel whenever a task, queue,
   timer or semaphore is created. It is also called by various parts of the
   demo application. If heap_1.c or heap_2.c are used, then the size of the
   heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
   FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
   to query the size of free heap space that remains (although it does not
   provide information on how the remaining heap might be fragmented). */
	
    /* 最后一道安全动作：PWM=0，TB6612 失能 */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0U);
    HAL_GPIO_WritePin(STBY_GPIO_Port, STBY_Pin, GPIO_PIN_RESET);
  /*屏蔽掉可屏蔽的中断*/
    taskDISABLE_INTERRUPTS();//此时的for就不是任务了，不会进入时间片，他只是死循环
    for (;;)
    {
    }	
}
/* USER CODE END 5 */
	
/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of ControlTask */
  ControlTaskHandle = osThreadNew(StartControlTask, NULL, &ControlTask_attributes);

  /* creation of SafetyTask */
  SafetyTaskHandle = osThreadNew(StartSafetyTask, NULL, &SafetyTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartControlTask */
/**
  * @brief  Function implementing the ControlTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartControlTask */
void StartControlTask(void *argument)
{
  /* USER CODE BEGIN StartControlTask */
  (void) argument;
  //uint16_t encoder_test_count = 0U;
  MotorStatus_t ret_status_motor = MOTOR_STATUS_OK;
  EncoderStatus_t ret_status_encoder = ENCODER_STATUS_OK;
  LimitStatus_t ret_status_limit = LIMIT_STATUS_OK;

  static bool limit_state_logged = false;
  static LimitSwitchState_t last_limit_state = LIMIT_STATE_NONE;

  ret_status_limit = Limit_Init(&g_limit,&g_limit_port_stm32_ops,(void *)&g_limit_port_stm32_config);
  if (LIMIT_STATUS_OK != ret_status_limit)
  {
      for (;;)
      {
          osDelay(1000U);
      }
  }  
  HAL_UART_Transmit(&huart1,
                  (uint8_t *)"[BOOT] Limit task started\r\n",
                  sizeof("[BOOT] Limit task started\r\n") - 1U,
                  100U);
  ret_status_encoder = Encoder_Init(&g_encoder,&g_encoder_port_stm32_ops,(void*)&g_encoder_port_stm32_config);
  if (ENCODER_STATUS_OK != ret_status_encoder)  
  {
      for (;;)
      {
          osDelay(1000U);
      }
  }

  ret_status_motor = Motor_Init(&g_motor,&g_motor_port_stm32_ops,(void*)&g_motor_port_stm32_config);
  
  if (MOTOR_STATUS_OK != ret_status_motor)
  {
      for (;;)
      {
          osDelay(1000U);
      }
  }

  // Motor_SetDirection(&g_motor,MOTOR_DIR_REVERSE);

  // Motor_SetDuty(&g_motor,30);

  // Motor_Enable(&g_motor);
  /* Infinite loop */
  for(;;)
  {

      // osDelay(10000);

      // Motor_Stop(&g_motor);
      // osDelay(1000);
      // Motor_SetDirection(&g_motor,MOTOR_DIR_REVERSE);
      // Motor_SetDuty(&g_motor,30);

      // osDelay(10000);

      // Motor_Stop(&g_motor);
      // osDelay(1000);
      // Motor_SetDirection(&g_motor,MOTOR_DIR_FORWARD);
      // Motor_SetDuty(&g_motor,30);

    // ret_status_encoder = Encoder_Update(&g_encoder);
    // if (ENCODER_STATUS_OK != ret_status_encoder)  
    // {   
    //     Motor_Stop(&g_motor);
    //     for (;;)
    //     {
    //         osDelay(1000U);
    //     }
    // }    

    // ret_status_encoder = Encoder_GetSnapshot(&g_encoder,&g_encoder_snapshot);
    // if (ENCODER_STATUS_OK != ret_status_encoder)  
    // {   
    //     Motor_Stop(&g_motor);
    //     for (;;)
    //     {
    //         osDelay(1000U);
    //     }
    // }         
    // encoder_test_count++;

    // if (encoder_test_count >= 200U)
    // {
    //     Motor_Stop(&g_motor);

    //     for (;;)
    //     {
    //         osDelay(1000U);
    //     }
    // }    
    ret_status_limit = Limit_Update(&g_limit);
    if (LIMIT_STATUS_OK != ret_status_limit)
    {
        for (;;)
        {
            osDelay(1000U);
        }
    }

    ret_status_limit = Limit_GetSnapshot(&g_limit, &g_limit_snapshot);
    if (LIMIT_STATUS_OK != ret_status_limit)
    {
        for (;;)
        {
            osDelay(1000U);
        }
    }    

    if ((!limit_state_logged) || (g_limit_snapshot.state != last_limit_state))
    {
        last_limit_state = g_limit_snapshot.state;
        limit_state_logged = true;
        switch (g_limit_snapshot.state)
        {
            case LIMIT_STATE_NONE:
                HAL_UART_Transmit(&huart1, (uint8_t *)"[LIMIT] NONE\r\n",
                                  sizeof("[LIMIT] NONE\r\n") - 1U, 10U);
                break;

            case LIMIT_STATE_OPEN_ACTIVE:
                HAL_UART_Transmit(&huart1, (uint8_t *)"[LIMIT] OPEN_ACTIVE\r\n",
                                  sizeof("[LIMIT] OPEN_ACTIVE\r\n") - 1U, 10U);
                break;

            case LIMIT_STATE_CLOSE_ACTIVE:
                HAL_UART_Transmit(&huart1, (uint8_t *)"[LIMIT] CLOSE_ACTIVE\r\n",
                                  sizeof("[LIMIT] CLOSE_ACTIVE\r\n") - 1U, 10U);
                break;

            case LIMIT_STATE_CONFLICT:
                HAL_UART_Transmit(&huart1, (uint8_t *)"[LIMIT] CONFLICT\r\n",
                                  sizeof("[LIMIT] CONFLICT\r\n") - 1U, 10U);
                break;

            default:
                break;
        }
        
    }    
   
    osDelay(10U);
  }
  /* USER CODE END StartControlTask */
}

/* USER CODE BEGIN Header_StartSafetyTask */
/**
* @brief Function implementing the SafetyTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartSafetyTask */
void StartSafetyTask(void *argument)
{
  /* USER CODE BEGIN StartSafetyTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartSafetyTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

