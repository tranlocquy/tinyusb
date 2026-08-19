/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 SuperCAN contributors
 *
 */

#pragma once

#define SC_BOARD_USB_BCD_DEVICE (HWREV << 8)
#define SC_BOARD_USB_MANUFACTURER_STRING "STMicroelectronics"
#define SC_BOARD_CAN_COUNT 1
#define SC_BOARD_NAME "STM32H725ZGT6"
#define SC_BOARD_CAN_CLK_HZ 80000000

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

#include <stm32h725xx.h>

#define MSG_BUFFER_SIZE 512
#define SUPERCAN_MCAN 1
#define MCAN_MESSAGE_RAM_CONFIGURABLE 0
#define MCAN_HW_RX_FIFO_SIZE 64
#define MCAN_HW_TX_FIFO_SIZE 32

#include <supercan_mcan.h>
#include <supercan_stm32h7_fdcan.h>
