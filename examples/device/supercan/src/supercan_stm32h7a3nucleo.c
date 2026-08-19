/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2023 Jean Gressmann <jean@0x42.de>
 * Copyright (c) 2026 SuperCAN contributors
 *
 */

#include <supercan_board.h>

#if STM32H7A3NUCLEO || STM32H725ZGT6

#include <supercan_debug.h>
#include <leds.h>
#include <tusb.h>
#include <bsp/board.h>

#include <m_can.h>

#include <tusb.h>
#include <stm32h7xx_hal.h> // for stm32h7xx_hal_cortex.h to have NVIC_PRIORITYGROUP_4

// Shared application implementation for the H7A3 Nucleo and STM32H725ZGT6
// targets. The H725 adds two more FDCAN channels and partitions the fixed shared
// message RAM between the enabled controllers. Clock selection is handled per RCC
// generation below.


#define PORT_SHIFT 4
#define PIN_MASK 15
#define MAKE_PIN(port, pin) (((port) << PORT_SHIFT) | (pin))

#define PIN_PB00 MAKE_PIN(1, 0)
#define PIN_PB14 MAKE_PIN(1, 14)

#define PIN_PE01 MAKE_PIN(4, 1)


// NOTE: If you are using CMSIS, the registers can also be
// accessed through CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk
#define HALT_IF_DEBUGGING()                              \
  do {                                                   \
    if ((*(volatile uint32_t *)0xE000EDF0) & (1 << 0)) { \
      __asm("bkpt 1");                                   \
    }                                                    \
} while (0)

typedef struct __attribute__((packed)) ContextStateFrame {
  uint32_t r0;
  uint32_t r1;
  uint32_t r2;
  uint32_t r3;
  uint32_t r12;
  uint32_t lr;
  uint32_t return_address;
  uint32_t xpsr;
} sContextStateFrame;

#define HARDFAULT_HANDLING_ASM(_x)               \
  __asm volatile(                                \
      "tst lr, #4 \n"                            \
      "ite eq \n"                                \
      "mrseq r0, msp \n"                         \
      "mrsne r0, psp \n"                         \
      "b my_fault_handler_c \n"                  \
                                                 )

// Disable optimizations for this function so "frame" argument
// does not get optimized away
__attribute__((optimize("O0")))
void my_fault_handler_c(sContextStateFrame *frame)
{
	(void)frame;
  // If and only if a debugger is attached, execute a breakpoint
  // instruction so we can take a look at what triggered the fault
  HALT_IF_DEBUGGING();

  // Logic for dealing with the exception. Typically:
  //  - log the fault which occurred for postmortem analysis
  //  - If the fault is recoverable,
  //    - clear errors and return back to Thread Mode
  //  - else
  //    - reboot system
  while (1);
}

void HardFault_Handler(void)
{
  HARDFAULT_HANDLING_ASM();
}

void BusFault_Handler(void)
{
	HARDFAULT_HANDLING_ASM();
}

void MemMang_Handler(void)
{
	HARDFAULT_HANDLING_ASM();
}

#if STM32H725ZGT6
static bool pll2_wait_ready(bool ready)
{
	// PLL lock/unlock is specified in microseconds. A core-clock-scaled bounded
	// wait prevents a broken oscillator from hanging startup forever.
	uint32_t timeout = SystemCoreClock / 100;

	while (((RCC->CR & RCC_CR_PLL2RDY) != 0) != ready) {
		if (--timeout == 0) {
			return false;
		}
	}

	return true;
}

__attribute__((noreturn)) static void fdcan_clock_failed(void)
{
	LOG("failed to configure 60 MHz FDCAN clock\n");
	NVIC_SystemReset();
	while (1);
}

static void fdcan_clock_init(void)
{
	// PLL2 shares PLLSRC with PLL1. The STM32H725ZGT6 BSP selects HSI64.
	if (__HAL_RCC_GET_PLL_OSCSOURCE() != RCC_PLLSOURCE_HSI) {
		fdcan_clock_failed();
	}

	__HAL_RCC_PLL2_DISABLE();
	if (!pll2_wait_ready(false)) {
		fdcan_clock_failed();
	}

	// HSI64 / M4 * N15 / Q4 = 60 MHz. P and R are unused and kept at their
	// lowest valid divider while only the Q output is enabled.
	__HAL_RCC_PLL2CLKOUT_DISABLE(RCC_PLL2_DIVP | RCC_PLL2_DIVQ | RCC_PLL2_DIVR);
	__HAL_RCC_PLL2_CONFIG(4, 15, 1, 4, 1);
	__HAL_RCC_PLL2_VCIRANGE(RCC_PLL2VCIRANGE_3);
	__HAL_RCC_PLL2_VCORANGE(RCC_PLL2VCOWIDE);
	__HAL_RCC_PLL2FRACN_DISABLE();
	__HAL_RCC_PLL2FRACN_CONFIG(0);
	__HAL_RCC_PLL2CLKOUT_ENABLE(RCC_PLL2_DIVQ);

	__HAL_RCC_PLL2_ENABLE();
	if (!pll2_wait_ready(true)) {
		fdcan_clock_failed();
	}

	// For STM32H725xx this macro selects PLL2Q through D2CCIP1R.FDCANSEL.
	__HAL_RCC_FDCAN_CONFIG(RCC_FDCANCLKSOURCE_PLL2);
}
#endif

enum {
	FDCAN_RX_FIFO_BYTES = sizeof(struct mcan_rx_fifo_element) * MCAN_HW_RX_FIFO_SIZE,
	FDCAN_TX_FIFO_BYTES = sizeof(struct mcan_tx_fifo_element) * MCAN_HW_TX_FIFO_SIZE,
	FDCAN_TXE_FIFO_BYTES = sizeof(struct mcan_txe_fifo_element) * MCAN_HW_TX_FIFO_SIZE,

	FDCAN1_RX_FIFO_OFFSET = 0,
	FDCAN1_TX_FIFO_OFFSET = FDCAN1_RX_FIFO_OFFSET + FDCAN_RX_FIFO_BYTES,
	FDCAN1_TXE_FIFO_OFFSET = FDCAN1_TX_FIFO_OFFSET + FDCAN_TX_FIFO_BYTES,
	FDCAN1_RAM_END_OFFSET = FDCAN1_TXE_FIFO_OFFSET + FDCAN_TXE_FIFO_BYTES,
#if STM32H725ZGT6
	STM32H725_SRAMCAN_BYTES = 0x2800,
#endif
#if STM32H725ZGT6 && SC_BOARD_CAN_COUNT > 1
	FDCAN2_RX_FIFO_OFFSET = FDCAN1_RAM_END_OFFSET,
	FDCAN2_TX_FIFO_OFFSET = FDCAN2_RX_FIFO_OFFSET + FDCAN_RX_FIFO_BYTES,
	FDCAN2_TXE_FIFO_OFFSET = FDCAN2_TX_FIFO_OFFSET + FDCAN_TX_FIFO_BYTES,
	FDCAN2_RAM_END_OFFSET = FDCAN2_TXE_FIFO_OFFSET + FDCAN_TXE_FIFO_BYTES,
#if SC_BOARD_CAN_COUNT > 2
	FDCAN3_RX_FIFO_OFFSET = FDCAN2_RAM_END_OFFSET,
	FDCAN3_TX_FIFO_OFFSET = FDCAN3_RX_FIFO_OFFSET + FDCAN_RX_FIFO_BYTES,
	FDCAN3_TXE_FIFO_OFFSET = FDCAN3_TX_FIFO_OFFSET + FDCAN_TX_FIFO_BYTES,
	FDCAN3_RAM_END_OFFSET = FDCAN3_TXE_FIFO_OFFSET + FDCAN_TXE_FIFO_BYTES,
	FDCAN_RAM_END_OFFSET = FDCAN3_RAM_END_OFFSET,
#else
	FDCAN_RAM_END_OFFSET = FDCAN2_RAM_END_OFFSET,
#endif
#else
	FDCAN_RAM_END_OFFSET = FDCAN1_RAM_END_OFFSET,
#endif
};

#if STM32H725ZGT6
_Static_assert(SC_BOARD_CAN_COUNT == 1 || SC_BOARD_CAN_COUNT == 2 || SC_BOARD_CAN_COUNT == 3,
	"STM32H725ZGT6 requires one, two, or three CAN channels");
_Static_assert(FDCAN_RAM_END_OFFSET <= STM32H725_SRAMCAN_BYTES, "FDCAN message RAM exceeds SRAMCAN");
_Static_assert((FDCAN_RAM_END_OFFSET & 3) == 0, "FDCAN message RAM must be word aligned");
#if SC_BOARD_CAN_COUNT == 1
_Static_assert(MCAN_HW_RX_FIFO_SIZE == 64, "Single-FDCAN RX FIFO must contain 64 elements");
_Static_assert(MCAN_HW_TX_FIFO_SIZE == 32, "Single-FDCAN TX and TX event FIFOs must contain 32 elements");
_Static_assert(FDCAN_RAM_END_OFFSET == 0x1c00, "Unexpected single-FDCAN message RAM layout");
#elif SC_BOARD_CAN_COUNT == 2
_Static_assert(MCAN_HW_RX_FIFO_SIZE == 32, "Dual-FDCAN RX FIFOs must contain 32 elements");
_Static_assert(MCAN_HW_TX_FIFO_SIZE == 32, "Dual-FDCAN TX and TX event FIFOs must contain 32 elements");
_Static_assert(FDCAN2_RX_FIFO_OFFSET == FDCAN1_RAM_END_OFFSET, "FDCAN message RAM regions must be contiguous");
_Static_assert(FDCAN_RAM_END_OFFSET == 0x2600, "Unexpected dual-FDCAN message RAM layout");
_Static_assert(((FDCAN1_RX_FIFO_OFFSET | FDCAN1_TX_FIFO_OFFSET | FDCAN1_TXE_FIFO_OFFSET
	| FDCAN2_RX_FIFO_OFFSET | FDCAN2_TX_FIFO_OFFSET | FDCAN2_TXE_FIFO_OFFSET
	| FDCAN_RAM_END_OFFSET) & 3) == 0, "FDCAN message RAM sections must be word aligned");
#else
_Static_assert(MCAN_HW_RX_FIFO_SIZE == 32, "Triple-FDCAN RX FIFOs must contain 32 elements");
_Static_assert(MCAN_HW_TX_FIFO_SIZE == 12, "Triple-FDCAN TX and TX event FIFOs must contain 12 elements");
_Static_assert(FDCAN2_RX_FIFO_OFFSET == FDCAN1_RAM_END_OFFSET,
	"FDCAN1 and FDCAN2 message RAM regions must be contiguous");
_Static_assert(FDCAN3_RX_FIFO_OFFSET == FDCAN2_RAM_END_OFFSET,
	"FDCAN2 and FDCAN3 message RAM regions must be contiguous");
_Static_assert(FDCAN1_RAM_END_OFFSET == 0x0cc0, "Unexpected FDCAN1 message RAM layout");
_Static_assert(FDCAN2_RAM_END_OFFSET == 0x1980, "Unexpected FDCAN2 message RAM layout");
_Static_assert(FDCAN_RAM_END_OFFSET == 0x2640, "Unexpected triple-FDCAN message RAM layout");
_Static_assert(((FDCAN1_RX_FIFO_OFFSET | FDCAN1_TX_FIFO_OFFSET | FDCAN1_TXE_FIFO_OFFSET
	| FDCAN2_RX_FIFO_OFFSET | FDCAN2_TX_FIFO_OFFSET | FDCAN2_TXE_FIFO_OFFSET
	| FDCAN3_RX_FIFO_OFFSET | FDCAN3_TX_FIFO_OFFSET | FDCAN3_TXE_FIFO_OFFSET
	| FDCAN_RAM_END_OFFSET) & 3) == 0, "FDCAN message RAM sections must be word aligned");
#endif
#endif

struct fdcan_channel_config {
	MCanX *m_can;
	IRQn_Type interrupt_id;
	uint32_t rx_fifo_offset;
	uint32_t tx_fifo_offset;
	uint32_t txe_fifo_offset;
	uint8_t led_status_green;
	uint8_t led_status_red;
};

static const struct fdcan_channel_config fdcan_channels[] = {
	{
		.m_can = (MCanX *)FDCAN1,
		.interrupt_id = FDCAN1_IT0_IRQn,
		.rx_fifo_offset = FDCAN1_RX_FIFO_OFFSET,
		.tx_fifo_offset = FDCAN1_TX_FIFO_OFFSET,
		.txe_fifo_offset = FDCAN1_TXE_FIFO_OFFSET,
		.led_status_green = LED_CAN0_STATUS_GREEN,
		.led_status_red = LED_CAN0_STATUS_RED,
	},
#if STM32H725ZGT6 && SC_BOARD_CAN_COUNT > 1
	{
		.m_can = (MCanX *)FDCAN2,
		.interrupt_id = FDCAN2_IT0_IRQn,
		.rx_fifo_offset = FDCAN2_RX_FIFO_OFFSET,
		.tx_fifo_offset = FDCAN2_TX_FIFO_OFFSET,
		.txe_fifo_offset = FDCAN2_TXE_FIFO_OFFSET,
		// SC_BOARD_LED_COUNT is an invalid LED index used as a no-LED sentinel.
		.led_status_green = SC_BOARD_LED_COUNT,
		.led_status_red = SC_BOARD_LED_COUNT,
	},
#endif
#if STM32H725ZGT6 && SC_BOARD_CAN_COUNT > 2
	{
		.m_can = (MCanX *)FDCAN3,
		.interrupt_id = FDCAN3_IT0_IRQn,
		.rx_fifo_offset = FDCAN3_RX_FIFO_OFFSET,
		.tx_fifo_offset = FDCAN3_TX_FIFO_OFFSET,
		.txe_fifo_offset = FDCAN3_TXE_FIFO_OFFSET,
		.led_status_green = SC_BOARD_LED_COUNT,
		.led_status_red = SC_BOARD_LED_COUNT,
	},
#endif
};

_Static_assert(TU_ARRAY_SIZE(fdcan_channels) == SC_BOARD_CAN_COUNT,
	"FDCAN channel table does not match SC_BOARD_CAN_COUNT");

// controller and hardware specific setup of i/o pins for CAN
static void can_init(void)
{
	const uint32_t gpio_af_fdcan1 = GPIO_AF9_FDCAN1;

#if STM32H725ZGT6
	/* DS13311, STM32H725ZGT6 LQFP144:
	 *   FDCAN1_RX PB8 (pin 136), FDCAN1_TX PB9 (pin 137)
	 *   FDCAN2_RX PB5 (pin 132), FDCAN2_TX PB6 (pin 133), when enabled
	 *   FDCAN3_RX PG10 (pin 123), FDCAN3_TX PG9 (pin 122), when enabled. */
#if SC_BOARD_CAN_COUNT > 1
	const uint32_t gpio_af_fdcan2 = GPIO_AF9_FDCAN2;
#endif
#if SC_BOARD_CAN_COUNT > 2
	const uint32_t gpio_af_fdcan3 = GPIO_AF2_FDCAN3;
#endif

	RCC->AHB4ENR |= RCC_AHB4ENR_GPIOBEN;
#if SC_BOARD_CAN_COUNT > 2
	RCC->AHB4ENR |= RCC_AHB4ENR_GPIOGEN;
#endif

#if SC_BOARD_CAN_COUNT > 1
	GPIOB->AFR[0] =
		(GPIOB->AFR[0] & ~(GPIO_AFRL_AFSEL5 | GPIO_AFRL_AFSEL6))
		| (gpio_af_fdcan2 << GPIO_AFRL_AFSEL5_Pos)
		| (gpio_af_fdcan2 << GPIO_AFRL_AFSEL6_Pos);
#endif

	GPIOB->AFR[1] =
		(GPIOB->AFR[1] & ~(GPIO_AFRH_AFSEL8 | GPIO_AFRH_AFSEL9))
		| (gpio_af_fdcan1 << GPIO_AFRH_AFSEL8_Pos)
		| (gpio_af_fdcan1 << GPIO_AFRH_AFSEL9_Pos);

	GPIOB->MODER =
		(GPIOB->MODER & ~(GPIO_MODER_MODE8 | GPIO_MODER_MODE9))
		| (GPIO_MODE_AF_PP << GPIO_MODER_MODE8_Pos)
		| (GPIO_MODE_AF_PP << GPIO_MODER_MODE9_Pos);

#if SC_BOARD_CAN_COUNT > 1
	GPIOB->MODER =
		(GPIOB->MODER & ~(GPIO_MODER_MODE5 | GPIO_MODER_MODE6))
		| (GPIO_MODE_AF_PP << GPIO_MODER_MODE5_Pos)
		| (GPIO_MODE_AF_PP << GPIO_MODER_MODE6_Pos);
#endif
#if SC_BOARD_CAN_COUNT > 2
	GPIOG->AFR[1] =
		(GPIOG->AFR[1] & ~(GPIO_AFRH_AFSEL9 | GPIO_AFRH_AFSEL10))
		| (gpio_af_fdcan3 << GPIO_AFRH_AFSEL9_Pos)
		| (gpio_af_fdcan3 << GPIO_AFRH_AFSEL10_Pos);

	GPIOG->MODER =
		(GPIOG->MODER & ~(GPIO_MODER_MODE9 | GPIO_MODER_MODE10))
		| (GPIO_MODE_AF_PP << GPIO_MODER_MODE9_Pos)
		| (GPIO_MODE_AF_PP << GPIO_MODER_MODE10_Pos);
#endif
#else
	/* NUCLEO-H7A3ZI-Q: FDCAN1_RX PD0 and FDCAN1_TX PD1. */
	RCC->AHB4ENR |= RCC_AHB4ENR_GPIODEN;

	GPIOD->AFR[0] =
		(GPIOD->AFR[0] & ~(GPIO_AFRL_AFSEL0 | GPIO_AFRL_AFSEL1))
		| (gpio_af_fdcan1 << GPIO_AFRL_AFSEL0_Pos)
		| (gpio_af_fdcan1 << GPIO_AFRL_AFSEL1_Pos);

	GPIOD->MODER =
		(GPIOD->MODER & ~(
			GPIO_MODER_MODE0
			| GPIO_MODER_MODE1))
		| (GPIO_MODE_AF_PP << GPIO_MODER_MODE0_Pos)
		| (GPIO_MODE_AF_PP << GPIO_MODER_MODE1_Pos);
#endif

	// Configure the target-specific PLL2 FDCAN kernel clock from HSI64.
#if STM32H725ZGT6
	fdcan_clock_init();
#else
	RCC->CR &= ~RCC_CR_PLL2ON;

	RCC->PLLCFGR =
		(RCC->PLLCFGR & ~(
			RCC_PLLCFGR_DIVQ2EN
			| RCC_PLLCFGR_DIVP2EN
			| RCC_PLLCFGR_DIVR2EN
			| RCC_PLLCFGR_PLL2RGE
			| RCC_PLLCFGR_PLL2VCOSEL))
		| (0x3 << RCC_PLLCFGR_PLL2RGE_Pos); // 8-16 MHz input range

	RCC->PLLCKSELR =
		(RCC->PLLCKSELR & ~(RCC_PLLCKSELR_DIVM2))
		| (4 << RCC_PLLCKSELR_DIVM2_Pos); // M=4: 64->16 Mhz

	RCC->PLL2DIVR =
		(2 << RCC_PLL2DIVR_Q2_Pos) // Q=3
		| (14 << RCC_PLL2DIVR_N2_Pos) // N=15: 16 * 15 / 3 = 80 MHz
		;

	// enable PLL2 Q
	RCC->PLLCFGR |= RCC_PLLCFGR_DIVQ2EN;

	// enable PLL2
	RCC->CR |= RCC_CR_PLL2ON;

	// set clock for FDCAN to PLL2 Q
	RCC->CDCCIP1R =
		(RCC->CDCCIP1R &
		~(RCC_CDCCIP1R_FDCANSEL))
		| (0x2 << RCC_CDCCIP1R_FDCANSEL_Pos);
#endif

	// enable clock
	RCC->APB1HENR |= RCC_APB1HENR_FDCANEN;
	RCC->APB1HLPENR |= RCC_APB1HLPENR_FDCANLPEN;

	LOG("M_CAN CCU release %lx\n", FDCAN_CCU->CREL);

	mcan_can_init();

	for (size_t i = 0; i < TU_ARRAY_SIZE(fdcan_channels); ++i) {
		const struct fdcan_channel_config *config = &fdcan_channels[i];
		struct mcan_can *can = &mcan_cans[i];

		can->m_can = config->m_can;
		can->interrupt_id = config->interrupt_id;
		can->led_traffic = SC_BOARD_DEBUG_DEFAULT;
		can->led_status_green = config->led_status_green;
		can->led_status_red = config->led_status_red;
		can->hw_tx_fifo_ram = (struct mcan_tx_fifo_element *)(SRAMCAN_BASE + config->tx_fifo_offset);
		can->hw_txe_fifo_ram = (struct mcan_txe_fifo_element *)(SRAMCAN_BASE + config->txe_fifo_offset);
		can->hw_rx_fifo_ram = (struct mcan_rx_fifo_element *)(SRAMCAN_BASE + config->rx_fifo_offset);

		LOG("FDCAN%u offset RX=%08lx TX=%08lx TXE=%08lx\n",
			(unsigned)i + 1,
			(unsigned long)config->rx_fifo_offset,
			(unsigned long)config->tx_fifo_offset,
			(unsigned long)config->txe_fifo_offset);

		m_can_init_begin(can->m_can);
		m_can_conf_begin(can->m_can);
	}

#if STM32H725ZGT6
	/* AN5348 requires every allocated FDCAN message-RAM word to be
	 * initialized before use. All selected controllers are held in INIT here. */
	volatile uint32_t *message_ram = (volatile uint32_t *)SRAMCAN_BASE;
	for (size_t i = 0; i < FDCAN_RAM_END_OFFSET / sizeof(*message_ram); ++i) {
		message_ram[i] = 0;
	}
	__DSB();
#endif

	// Setup shared clock calibration unit (CCU) for bypass.
	// FDCAN1 owns the CCU write protection and is in INIT/CCE here.
	FDCAN_CCU->CCFG = FDCANCCU_CCFG_SWR;
	FDCAN_CCU->CCFG = FDCANCCU_CCFG_BCC;
	LOG("CCU CCFG=%08lx\n", FDCAN_CCU->CCFG);

	for (size_t i = 0; i < TU_ARRAY_SIZE(fdcan_channels); ++i) {
		const struct fdcan_channel_config *config = &fdcan_channels[i];
		struct mcan_can *channel = &mcan_cans[i];
		MCanX *can = channel->m_can;

		// Route all enabled sources to this controller's interrupt line 0.
		can->ILS = 0;
		can->ILE.reg = MCANX_ILE_EINT0;

		// tx fifo
		can->TXBC.reg = MCANX_TXBC_TBSA(config->tx_fifo_offset) | MCANX_TXBC_TFQS(MCAN_HW_TX_FIFO_SIZE);
		// tx event fifo
		can->TXEFC.reg = MCANX_TXEFC_EFSA(config->txe_fifo_offset) | MCANX_TXEFC_EFS(MCAN_HW_TX_FIFO_SIZE);
		// rx fifo0
		can->RXF0C.reg = MCANX_RXF0C_F0SA(config->rx_fifo_offset) | MCANX_RXF0C_F0S(MCAN_HW_RX_FIFO_SIZE);

		// configure for max message size
		can->TXESC.reg = MCANX_TXESC_TBDS_DATA64;
		//  | MCANX_RXF0C_F0OM; // FIFO 0 overwrite mode
		can->RXESC.reg = MCANX_RXESC_RBDS_DATA64 + MCANX_RXESC_F0DS_DATA64;

		m_can_conf_end(can);
		NVIC_SetPriority(channel->interrupt_id, SC_ISR_PRIORITY);

		LOG("M_CAN%u release %u.%u.%u (%lx)\n",
			(unsigned)i + 1,
			can->CREL.bit.REL,
			can->CREL.bit.STEP,
			can->CREL.bit.SUBSTEP,
			can->CREL.reg);
	}
}

static inline void counter_1mhz_init(void)
{
	// enable clock
	RCC->APB1LENR |= RCC_APB1LENR_TIM2EN;

	// 1 MHz
	TIM2->PSC = (SystemCoreClock / 1000000UL) - 1; /* yes, minus one */

	/* reset value of TIM2->ARR is 0xffffffff which is what we want */

	// clear counter to load prescaler (RM0444 Rev 5 p. 689/1390)
	TIM2->EGR |= TIM_EGR_UG;


	// // 1 second reload
	// TIM2->ARR = 1000000UL;

	// TIM2->DIER = TIM_DIER_UIE;

	// NVIC_SetPriority(TIM2_IRQn, SC_ISR_PRIORITY);
	// NVIC_EnableIRQ(TIM2_IRQn);

	// start timer
	TIM2->CR1 =
		TIM_CR1_URS /* only under/overflow, DMA */
		| TIM_CR1_CEN;
}

struct led {
	uint8_t port_pin_mux;
};

#define LED_STATIC_INITIALIZER(name, mux) \
	{ mux }


static const struct led leds[] = {
	LED_STATIC_INITIALIZER("debug", PIN_PE01), // yellow
	LED_STATIC_INITIALIZER("can0_green", PIN_PB00), // green
	LED_STATIC_INITIALIZER("can0_red", PIN_PB14), // red
};

static inline void leds_init(void)
{
	// enable clock to GPIO block B, E
	RCC->AHB4ENR |= RCC_AHB4ENR_GPIOBEN | RCC_AHB4ENR_GPIOEEN;

	// disable output
	GPIOB->BSRR =
		GPIO_BSRR_BR0 |
		GPIO_BSRR_BR14;

	GPIOE->BSRR =
		GPIO_BSRR_BR1;

  // output type is push-pull on reset

  // switch mode to output
  GPIOB->MODER =
  	(GPIOB->MODER & ~(
		GPIO_MODER_MODE0 |
		GPIO_MODER_MODE14))
	| (GPIO_MODE_OUTPUT_PP << GPIO_MODER_MODE0_Pos)
	| (GPIO_MODE_OUTPUT_PP << GPIO_MODER_MODE14_Pos);

  GPIOE->MODER =
  	(GPIOE->MODER & ~(
		GPIO_MODER_MODE1))
	| (GPIO_MODE_OUTPUT_PP << GPIO_MODER_MODE1_Pos);
}

extern void sc_board_led_set(uint8_t index, bool on)
{
	SC_DEBUG_ASSERT(index < TU_ARRAY_SIZE(leds));

	unsigned mux = leds[index].port_pin_mux;
	unsigned port = mux >> PORT_SHIFT;
	unsigned pin = mux & PIN_MASK;

	GPIO_TypeDef *gpio = (GPIO_TypeDef *)(GPIOA_BASE + (0x00000400UL * port));

	gpio->BSRR = UINT32_C(1) << (pin + (!on) * 16);
}

extern void sc_board_leds_on_unsafe(void)
{
	for (size_t i = 0; i < TU_ARRAY_SIZE(leds); ++i) {
		sc_board_led_set(i, 1);
	}
}


extern void sc_board_init_begin(void)
{
	board_init();

	LOG("CPU @ %lu Mhz\n", SystemCoreClock);

	leds_init();
	can_init();
	counter_1mhz_init();
}

extern void sc_board_init_end(void)
{
	led_blink(0, 2000);
	NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
}

SC_RAMFUNC extern void sc_board_led_can_status_set(uint8_t index, int status)
{
	SC_DEBUG_ASSERT(index < TU_ARRAY_SIZE(mcan_cans));
	if (index >= TU_ARRAY_SIZE(mcan_cans)) {
		return;
	}

	struct mcan_can *can = &mcan_cans[index];

	// A channel may be present without dedicated physical status LEDs.
	if (can->led_status_green >= TU_ARRAY_SIZE(leds)
		|| can->led_status_red >= TU_ARRAY_SIZE(leds)) {
		return;
	}

	switch (status) {
	case SC_CAN_LED_STATUS_DISABLED:
		led_set(can->led_status_green, 0);
		led_set(can->led_status_red, 0);
		break;
	case SC_CAN_LED_STATUS_ENABLED_OFF_BUS:
		led_set(can->led_status_green, 1);
		led_set(can->led_status_red, 0);
		break;
	case SC_CAN_LED_STATUS_ENABLED_ON_BUS_PASSIVE:
		led_blink(can->led_status_green, SC_CAN_LED_BLINK_DELAY_PASSIVE_MS);
		led_set(can->led_status_red, 0);
		break;
	case SC_CAN_LED_STATUS_ENABLED_ON_BUS_ACTIVE:
		led_blink(can->led_status_green, SC_CAN_LED_BLINK_DELAY_ACTIVE_MS);
		led_set(can->led_status_red, 0);
		break;
	case SC_CAN_LED_STATUS_ENABLED_ON_BUS_ERROR_PASSIVE:
		led_set(can->led_status_green, 0);
		led_blink(can->led_status_red, SC_CAN_LED_BLINK_DELAY_PASSIVE_MS);
		break;
	case SC_CAN_LED_STATUS_ENABLED_ON_BUS_ERROR_ACTIVE:
		led_set(can->led_status_green, 0);
		led_blink(can->led_status_red, SC_CAN_LED_BLINK_DELAY_ACTIVE_MS);
		break;
	case SC_CAN_LED_STATUS_ENABLED_ON_BUS_BUS_OFF:
		led_set(can->led_status_green, 0);
		led_set(can->led_status_red, 1);
		break;
	default:
		led_blink(can->led_status_green, SC_CAN_LED_BLINK_DELAY_ACTIVE_MS / 2);
		led_blink(can->led_status_red, SC_CAN_LED_BLINK_DELAY_ACTIVE_MS / 2);
		break;
	}
}

__attribute__((noreturn)) extern void sc_board_reset(void)
{
	NVIC_SystemReset();
	__unreachable();
}

extern uint32_t sc_board_identifier(void)
{
	uint32_t *id_ptr = (uint32_t *)UID_BASE;
	uint32_t id = 0;

	// 96 bit unique device ID
	id ^= *id_ptr++;
	id ^= *id_ptr++;
	id ^= *id_ptr++;

	return id;
}

SC_RAMFUNC void FDCAN1_IT0_IRQHandler(void)
{
	// LOG("FDCAN1_IT0 int\n");

	mcan_can_int(0);
}

#if STM32H725ZGT6 && SC_BOARD_CAN_COUNT > 1
SC_RAMFUNC void FDCAN2_IT0_IRQHandler(void)
{
	// LOG("FDCAN2_IT0 int\n");

	mcan_can_int(1);
}
#endif

#if STM32H725ZGT6 && SC_BOARD_CAN_COUNT > 2
SC_RAMFUNC void FDCAN3_IT0_IRQHandler(void)
{
	// LOG("FDCAN3_IT0 int\n");

	mcan_can_int(2);
}
#endif

SC_RAMFUNC void TIM2_IRQHandler(void)
{
	LOG("SR=%04x\n", TIM2->SR);

	// clear interrupts
	TIM2->SR = 0;
}

#endif // #if STM32H7A3NUCLEO || STM32H725ZGT6
