/* ###################################################################
**     Filename    : main.c
**     Processor   : S32K1xx
**     Abstract    :
**         Main module.
**         This module contains user's application code.
**     Settings    :
**     Contents    :
**         No public methods
**
** ###################################################################*/
/*!
** @file main.c
** @version 01.00
** @brief
**         Main module.
**         This module contains user's application code.
*/         
/*!
**  @addtogroup main_module main module documentation
**  @{
*/         
/* MODULE main */


/* Including necessary module. Cpu.h contains other modules needed for compiling.*/
#include "Cpu.h"
#include "clockMan1.h"
  volatile int exit_code = 0;
  static void delay(volatile uint32_t cycles)
  {
      while (cycles-- > 0U)
      {
          __asm volatile ("nop");
      }
  }
  static void uart1_init(void)
  {
      /* 打开 PORTC 和 LPUART1 时钟 */
      PCC->PCCn[PCC_PORTC_INDEX] |= PCC_PCCn_CGC_MASK;
      PCC->PCCn[PCC_LPUART1_INDEX] = PCC_PCCn_PCS(3U);
      PCC->PCCn[PCC_LPUART1_INDEX] |= PCC_PCCn_CGC_MASK;

      /* PTC6 = LPUART1_RX，PTC7 = LPUART1_TX */
      PORTC->PCR[6] = PORT_PCR_MUX(2U);
      PORTC->PCR[7] = PORT_PCR_MUX(2U);

      /* 48 MHz 时钟，115200 波特率 */
      LPUART1->CTRL = 0U;
      LPUART1->BAUD = LPUART_BAUD_OSR(15U) | LPUART_BAUD_SBR(26U);
      LPUART1->CTRL = LPUART_CTRL_TE_MASK | LPUART_CTRL_RE_MASK;
  }

  static void uart1_puts(const char *text)
  {
      while (*text != '\0')
      {
          while ((LPUART1->STAT & LPUART_STAT_TDRE_MASK) == 0U)
          {
          }
          LPUART1->DATA = (uint8_t)*text;
          text++;
      }
  }


#define CAN_RX_MB       4U
#define CAN_RX_ID       0x123U

static void flexcan0_init(void)
{
    uint32_t i;

    /* CAN0 物理收发器连接到 PTE4/PTE5 */
    PCC->PCCn[PCC_PORTE_INDEX] |= PCC_PCCn_CGC_MASK;
    PORTE->PCR[4] = PORT_PCR_MUX(5U);   /* PTE4: CAN0_RX */
    PORTE->PCR[5] = PORT_PCR_MUX(5U);   /* PTE5: CAN0_TX */

    /* 打开 FlexCAN0 时钟 */
    PCC->PCCn[PCC_FlexCAN0_INDEX] |= PCC_PCCn_CGC_MASK;

    /* 进入冻结模式，允许配置 */
    CAN0->MCR |= CAN_MCR_MDIS_MASK;
    CAN0->CTRL1 &= ~CAN_CTRL1_CLKSRC_MASK; /* 使用 8 MHz 外部晶振 */
    CAN0->MCR &= ~CAN_MCR_MDIS_MASK;

    while ((CAN0->MCR & CAN_MCR_FRZACK_MASK) == 0U)
    {
    }

    /* 8 MHz 时钟下配置为 500 kbit/s，标准 CAN */
    CAN0->CTRL1 = 0x00DB0006U;

    /* 清空 32 个消息缓冲区 */
    for (i = 0U; i < 128U; i++)
    {
        CAN0->RAMn[i] = 0U;
    }

    /* 接收缓冲区 4：只接收标准 ID 0x123 */
    CAN0->RXIMR[CAN_RX_MB] = 0x1FFFFFFFU;
    CAN0->RXMGMASK = 0x1FFFFFFFU;

    CAN0->RAMn[CAN_RX_MB * 4U] = 0x04000000U;
    CAN0->RAMn[CAN_RX_MB * 4U + 1U] = (CAN_RX_ID << 18U);

    /* 启动 CAN0，最多使用 32 个消息缓冲区 */
    CAN0->MCR = 0x0000001FU;

    while ((CAN0->MCR & CAN_MCR_FRZACK_MASK) != 0U)
    {
    }
}
/* User includes (#include below this line is not maintained by Processor Expert) */

/*! 
  \brief The main function for the project.
  \details The startup initialization sequence is the following:
 * - startup asm routine
 * - main()
*/
int main(void)
{
  /* Write your local variable definition here */

  /*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
  #ifdef PEX_RTOS_INIT
    PEX_RTOS_INIT();                   /* Initialization of the selected RTOS. Macro is defined by the RTOS component. */
  #endif
  /*** End of Processor Expert internal initialization.                    ***/
    /* 初始化系统时钟：让 FIRC 48 MHz 和外设时钟真正工作 */
    CLOCK_SYS_Init(g_clockManConfigsArr,
                   CLOCK_MANAGER_CONFIG_CNT,
                   g_clockManCallbacksArr,
                   CLOCK_MANAGER_CALLBACK_CNT);

    CLOCK_SYS_UpdateConfiguration(0U, CLOCK_MANAGER_POLICY_AGREEMENT);
  /* Write your code here */
  /* For example: for(;;) { } */
    /* 打开 PORTD 外设时钟 */
    PCC->PCCn[PCC_PORTD_INDEX] |= PCC_PCCn_CGC_MASK;

    /* PTD16 配置为普通 GPIO 输出，对应板载绿灯 */
    PORTD->PCR[16] = PORT_PCR_MUX(1U);
    PTD->PDDR |= (1UL << 16);
    uart1_init();
    flexcan0_init();
    uart1_puts("CAN ready, waiting for ID 0x123\r\n");
    /* 不断翻转绿灯状态 */
    for (;;)
    {
        if ((CAN0->IFLAG1 & (1UL << CAN_RX_MB)) != 0U)
        {
            /* 读一次数据寄存器，完成接收锁定流程 */
            (void)CAN0->RAMn[CAN_RX_MB * 4U + 2U];

            /* 清除接收完成标志 */
            CAN0->IFLAG1 = (1UL << CAN_RX_MB);

            /* 收到 ID 0x123：串口打印，绿灯翻转一次 */
            uart1_puts("CAN RX 0x123\r\n");
            PTD->PTOR = (1UL << 16);
        }
    }
  /*** Don't write any code pass this line, or it will be deleted during code generation. ***/
  /*** RTOS startup code. Macro PEX_RTOS_START is defined by the RTOS component. DON'T MODIFY THIS CODE!!! ***/
  #ifdef PEX_RTOS_START
    PEX_RTOS_START();                  /* Startup of the selected RTOS. Macro is defined by the RTOS component. */
  #endif
  /*** End of RTOS startup code.  ***/
  /*** Processor Expert end of main routine. DON'T MODIFY THIS CODE!!! ***/
  for(;;) {
    if(exit_code != 0) {
      break;
    }
  }
  return exit_code;
  /*** Processor Expert end of main routine. DON'T WRITE CODE BELOW!!! ***/
} /*** End of main routine. DO NOT MODIFY THIS TEXT!!! ***/

/* END main */
/*!
** @}
*/
/*
** ###################################################################
**
**     This file was created by Processor Expert 10.1 [05.21]
**     for the NXP S32K series of microcontrollers.
**
** ###################################################################
*/
