HWREV ?= 1
STM32H725_SUPPLY ?= PWR_LDO_SUPPLY
STM32H725_USB_INTERNAL_REGULATOR ?= 0

# HSE is not enabled by this BSP; the value satisfies the shared HAL config.
CFLAGS += -DSTM32H725xx -DHWREV=$(HWREV) \
          -DSTM32H725_SUPPLY=$(STM32H725_SUPPLY) -DHSE_VALUE=25000000 \
          -DSTM32H725_USB_INTERNAL_REGULATOR=$(STM32H725_USB_INTERNAL_REGULATOR) \
          -DRAMFUNC_SECTION_NAME="\".RamFunc\""

# The STM32H725 has a single USB HS controller.  Its embedded FS PHY is
# presented as TinyUSB root hub port 0.
PORT = 0
FS_PHY_ON_HS_CORE = 1

SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32h725xx.s
LD_FILE = $(BOARD_PATH)/stm32h725zgtx_flash.ld

# For the flash-jlink target.
JLINK_DEVICE = STM32H725ZG

flash: flash-jlink
