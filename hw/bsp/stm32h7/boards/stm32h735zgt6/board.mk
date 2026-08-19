HWREV ?= 1
STM32H735_SUPPLY ?= PWR_LDO_SUPPLY
STM32H735_USB_INTERNAL_REGULATOR ?= 0
STM32H735_USE_HSE ?= 0

ifneq ($(STM32H735_USE_HSE),0)
ifneq ($(STM32H735_USE_HSE),1)
$(error STM32H735_USE_HSE must be exactly 0 or 1)
endif
endif

# STM32H735_USE_HSE=1 selects a 25 MHz crystal/resonator on PH0/PH1.
CFLAGS += -DSTM32H735xx -DHWREV=$(HWREV) \
          -DSTM32H735_SUPPLY=$(STM32H735_SUPPLY) -DHSE_VALUE=25000000 \
          -DSTM32H735_USB_INTERNAL_REGULATOR=$(STM32H735_USB_INTERNAL_REGULATOR) \
          -DSTM32H735_USE_HSE=$(STM32H735_USE_HSE) \
          -DRAMFUNC_SECTION_NAME="\".RamFunc\""

# The STM32H735 has a single USB HS controller. Its embedded FS PHY is
# presented as TinyUSB root hub port 0.
PORT = 0
FS_PHY_ON_HS_CORE = 1

SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32h735xx.s
LD_FILE = $(BOARD_PATH)/stm32h735zgtx_flash.ld

# For the flash-jlink target.
JLINK_DEVICE = STM32H735ZG

flash: flash-jlink
