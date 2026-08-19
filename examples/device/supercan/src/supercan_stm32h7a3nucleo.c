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
// targets. Their SuperCAN GPIO, FDCAN message RAM, timer and LED layouts match;
// the FDCAN kernel-clock selector is handled per RCC generation below.


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
	LOG("failed to configure 80 MHz FDCAN clock\n");
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

	// HSI64 / M4 * N15 / Q3 = 80 MHz. P and R are unused and kept at their
	// lowest valid divider while only the Q output is enabled.
	__HAL_RCC_PLL2CLKOUT_DISABLE(RCC_PLL2_DIVP | RCC_PLL2_DIVQ | RCC_PLL2_DIVR);
	__HAL_RCC_PLL2_CONFIG(4, 15, 1, 3, 1);
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

// controller and hardware specific setup of i/o pins for CAN
static void can_init(void)
{
	/* FDCAN1_RX PD0, FDCAN1_TX PD1 */
	const uint32_t GPIO_MODE_AF_FDCAN = GPIO_AF9_FDCAN1;

	/* CAN RAM Layout:
	 	- RX fifo (base)
		- TX fifo
		- TX event fifo
	*/
	uint32_t const FDCAN1_RX_FIFO_OFFSET = 0;
	uint32_t const FDCAN1_TX_FIFO_OFFSET = FDCAN1_RX_FIFO_OFFSET + sizeof(struct mcan_rx_fifo_element) * MCAN_HW_RX_FIFO_SIZE;
	uint32_t const FDCAN1_TXE_FIFO_OFFSET = FDCAN1_TX_FIFO_OFFSET + sizeof(struct mcan_tx_fifo_element) * MCAN_HW_TX_FIFO_SIZE;
	MCanX* const can0 = (MCanX*)FDCAN1;

	LOG("FDCAN1 offset RX=%08lx TX=%08lx TXE=%08lx\n",
		FDCAN1_RX_FIFO_OFFSET, FDCAN1_TX_FIFO_OFFSET, FDCAN1_TXE_FIFO_OFFSET);


	mcan_can_init();

	mcan_cans[0].m_can = can0;
	mcan_cans[0].interrupt_id = FDCAN1_IT0_IRQn;
	mcan_cans[0].led_traffic = 0;
	mcan_cans[0].led_status_green = LED_CAN0_STATUS_GREEN;
	mcan_cans[0].led_status_red = LED_CAN0_STATUS_RED;
	mcan_cans[0].hw_tx_fifo_ram = (struct mcan_tx_fifo_element *)(SRAMCAN_BASE + FDCAN1_TX_FIFO_OFFSET);
	mcan_cans[0].hw_txe_fifo_ram = (struct mcan_txe_fifo_element *)(SRAMCAN_BASE + FDCAN1_TXE_FIFO_OFFSET);
	mcan_cans[0].hw_rx_fifo_ram = (struct mcan_rx_fifo_element *)(SRAMCAN_BASE + FDCAN1_RX_FIFO_OFFSET);



	// enable clock to GPIO block D
	RCC->AHB4ENR |= RCC_AHB4ENR_GPIODEN;


	// alternate function to CAN
	GPIOD->AFR[0] =
		(GPIOD->AFR[0] & ~(GPIO_AFRL_AFSEL0 | GPIO_AFRL_AFSEL1))
		| (GPIO_MODE_AF_FDCAN << GPIO_AFRL_AFSEL0_Pos)
		| (GPIO_MODE_AF_FDCAN << GPIO_AFRL_AFSEL1_Pos);

	// switch mode to alternate function
	GPIOD->MODER =
		(GPIOD->MODER & ~(
			GPIO_MODER_MODE0
			| GPIO_MODER_MODE1))
		| (GPIO_MODE_AF_PP << GPIO_MODER_MODE0_Pos)
		| (GPIO_MODE_AF_PP << GPIO_MODER_MODE1_Pos);



	// setup PLL2 to provide 80 MHz from 64 Mhz HSI
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

	m_can_init_begin(can0);
	m_can_conf_begin(can0);

	// Setup clock calibration unit (CCU) for bypass.
	// Must have INIT, CCE set
	FDCAN_CCU->CCFG = FDCANCCU_CCFG_SWR;

	// set bypass with divider of 1
	FDCAN_CCU->CCFG = FDCANCCU_CCFG_BCC;
	LOG("CCU CCFG=%08lx\n", FDCAN_CCU->CCFG);
	can0->ILE.reg = MCANX_ILE_EINT0;

	// tx fifo
	can0->TXBC.reg = MCANX_TXBC_TBSA(FDCAN1_TX_FIFO_OFFSET) | MCANX_TXBC_TFQS(MCAN_HW_TX_FIFO_SIZE);
	// tx event fifo
	can0->TXEFC.reg = MCANX_TXEFC_EFSA(FDCAN1_TXE_FIFO_OFFSET) | MCANX_TXEFC_EFS(MCAN_HW_TX_FIFO_SIZE);
	// rx fifo0
	can0->RXF0C.reg = MCANX_RXF0C_F0SA(FDCAN1_RX_FIFO_OFFSET) | MCANX_RXF0C_F0S(MCAN_HW_RX_FIFO_SIZE);

	// configure for max message size
	can0->TXESC.reg = MCANX_TXESC_TBDS_DATA64;
	//  | MCANX_RXF0C_F0OM; // FIFO 0 overwrite mode
	can0->RXESC.reg = MCANX_RXESC_RBDS_DATA64 + MCANX_RXESC_F0DS_DATA64;


	m_can_conf_end(can0);


	NVIC_SetPriority(mcan_cans[0].interrupt_id, SC_ISR_PRIORITY);

	LOG("M_CAN CAN release %u.%u.%u (%lx)\n", mcan_cans[0].m_can->CREL.bit.REL, mcan_cans[0].m_can->CREL.bit.STEP, mcan_cans[0].m_can->CREL.bit.SUBSTEP, mcan_cans[0].m_can->CREL.reg);
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
	struct mcan_can *can = &mcan_cans[index];

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

SC_RAMFUNC void TIM2_IRQHandler(void)
{
	LOG("SR=%04x\n", TIM2->SR);

	// clear interrupts
	TIM2->SR = 0;
}

#endif // #if STM32H7A3NUCLEO || STM32H725ZGT6
