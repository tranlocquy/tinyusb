/* SPDX-License-Identifier: MIT */

#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif

#define LED_PORT              GPIOE
#define LED_PIN               GPIO_PIN_1
#define LED_STATE_ON          1

#define BUTTON_PORT           GPIOC
#define BUTTON_PIN            GPIO_PIN_13
#define BUTTON_STATE_ACTIVE   1

#define UART_DEV              USART3
#define UART_CLK_EN           __HAL_RCC_USART3_CLK_ENABLE
#define UART_GPIO_PORT        GPIOD
#define UART_GPIO_AF          GPIO_AF7_USART3
#define UART_TX_PIN           GPIO_PIN_8
#define UART_RX_PIN           GPIO_PIN_9

/* This target is device-only and does not require PA9 VBUS sensing. */
#define OTG_FS_VBUS_SENSE     0
#define OTG_HS_VBUS_SENSE     0

/*
 * STM32H725 exposes the internal full-speed PHY through the USB1 OTG HS
 * controller.  The pinned TinyUSB STM32H7 family BSP names root hub port 0
 * after USB2 OTG FS, so provide the aliases it expects.
 */
#define OTG_HS_USE_FS_PHY 1

#define USB_OTG_FS                           USB_OTG_HS
#define GPIO_AF10_OTG2_HS                    GPIO_AF10_OTG1_HS
#define __HAL_RCC_USB2_OTG_FS_CLK_ENABLE     __HAL_RCC_USB1_OTG_HS_CLK_ENABLE

//--------------------------------------------------------------------+
// RCC Clock
//--------------------------------------------------------------------+
static inline void board_stm32h7_clock_init(void)
{
  RCC_OscInitTypeDef osc = {0};
  RCC_ClkInitTypeDef clk = {0};
  RCC_PeriphCLKInitTypeDef periph_clk = {0};

  /*
   * The default build assumes the customary LDO hardware configuration:
   * VDDLDO is supplied and VCAP is decoupled per the datasheet. A board wired
   * for the internal SMPS must override STM32H725_SUPPLY at build time with
   * the HAL supply mode that matches its schematic.
   */
  if (HAL_PWREx_ConfigSupply(STM32H725_SUPPLY) != HAL_OK)
  {
    while (1) {}
  }

  /*
   * VOS2 permits a 300 MHz CPU and 150 MHz AXI/AHB clock on STM32H725.
   * The clocks below are therefore comfortably inside their limits.
   */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);
  while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /* HSI64 / 4 * 15 / 2 = 120 MHz PLL1 P output (SYSCLK). */
  osc.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_HSI48;
  osc.HSIState = RCC_HSI_DIV1;
  osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  osc.HSI48State = RCC_HSI48_ON;
  osc.PLL.PLLState = RCC_PLL_ON;
  osc.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  osc.PLL.PLLM = 4;
  osc.PLL.PLLN = 15;
  osc.PLL.PLLP = 2;
  osc.PLL.PLLQ = 5;
  osc.PLL.PLLR = 2;
  osc.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  osc.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  osc.PLL.PLLFRACN = 0;

  if (HAL_RCC_OscConfig(&osc) != HAL_OK)
  {
    while (1) {}
  }

  clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                  RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_PCLK1 |
                  RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1;
  clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clk.SYSCLKDivider = RCC_SYSCLK_DIV1;
  clk.AHBCLKDivider = RCC_HCLK_DIV1;
  clk.APB3CLKDivider = RCC_APB3_DIV2;
  /* VOS2 limits APB clocks to 75 MHz; FDCAN also uses a 60 MHz kernel clock. */
  clk.APB1CLKDivider = RCC_APB1_DIV2;
  clk.APB2CLKDivider = RCC_APB2_DIV2;
  clk.APB4CLKDivider = RCC_APB4_DIV2;

  /* At VOS2, a 120 MHz AXI/flash clock requires two wait states. */
  if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK)
  {
    while (1) {}
  }

  periph_clk.PeriphClockSelection = RCC_PERIPHCLK_USB;
  periph_clk.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
  if (HAL_RCCEx_PeriphCLKConfig(&periph_clk) != HAL_OK)
  {
    while (1) {}
  }

  /*
   * Default: VDD33USB is supplied externally and the internal regulator stays
   * off. Set STM32H725_USB_INTERNAL_REGULATOR=1 only when VDD50USB/VDD33USB
   * follow ST's internal-regulator wiring; enabling it against an external
   * 3.3 V source would create supply contention.
   */
#if STM32H725_USB_INTERNAL_REGULATOR
  if (HAL_PWREx_EnableUSBReg() != HAL_OK)
  {
    while (1) {}
  }
#endif

  /* The CSI clock is required while the I/O compensation cell is enabled. */
  __HAL_RCC_CSI_ENABLE();
  while (!__HAL_RCC_GET_FLAG(RCC_FLAG_CSIRDY)) {}
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  HAL_EnableCompensationCell();
}

static inline void board_stm32h7_post_init(void)
{
  /* No external ULPI PHY reset is needed. */
}

#ifdef __cplusplus
}
#endif

#endif
