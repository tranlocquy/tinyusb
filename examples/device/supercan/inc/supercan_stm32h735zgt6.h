/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 SuperCAN contributors
 *
 */

#pragma once

#define SC_BOARD_USB_BCD_DEVICE (HWREV << 8)
#define SC_BOARD_USB_MANUFACTURER_STRING "STMicroelectronics"
#ifndef STM32H735_FDCAN_COUNT
	#define STM32H735_FDCAN_COUNT 2
#endif
#if STM32H735_FDCAN_COUNT != 1 && STM32H735_FDCAN_COUNT != 2 && STM32H735_FDCAN_COUNT != 3
	#error STM32H735_FDCAN_COUNT must be 1, 2, or 3
#endif

#define SC_BOARD_CAN_COUNT STM32H735_FDCAN_COUNT
#define SC_BOARD_NAME "STM32H735ZGT6"
#define SC_BOARD_CAN_CLK_HZ 40000000

enum {
	SC_BOARD_DEBUG_DEFAULT,
	LED_CAN0_STATUS_GREEN,
	LED_CAN0_STATUS_RED,
	SC_BOARD_LED_COUNT
};

#define sc_board_led_usb_burst()
#define sc_board_led_can_traffic_burst(index)
SC_RAMFUNC extern void sc_board_led_can_status_set(uint8_t index, int status);
#define sc_board_can_ts_request(index)
#define sc_board_can_ts_wait(index) (TIM2->CNT)

#include <stm32h735xx.h>

#define MSG_BUFFER_SIZE 512
#define SUPERCAN_MCAN 1
#define MCAN_MESSAGE_RAM_CONFIGURABLE 0
#define MCAN_ENABLE_EDGE_FILTERING 0 /* STM32H735 errata ES0491 section 2.22.1 */
#if SC_BOARD_CAN_COUNT == 1
	#define MCAN_HW_RX_FIFO_SIZE 64
#else
	#define MCAN_HW_RX_FIFO_SIZE 32
#endif
#if SC_BOARD_CAN_COUNT == 3
	#define MCAN_HW_TX_FIFO_SIZE 12
#else
	#define MCAN_HW_TX_FIFO_SIZE 32
#endif

#include <supercan_mcan.h>
#include <supercan_stm32h7_fdcan.h>
