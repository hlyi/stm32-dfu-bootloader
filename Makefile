CROSS_COMPILE ?= arm-none-eabi-
CC = $(CROSS_COMPILE)gcc
OBJCOPY = $(CROSS_COMPILE)objcopy
GIT_VERSION := $(shell git describe --abbrev=8 --dirty --always --tags)

# Config bits
BOOTLOADER_SIZE = 4
FLASH_SIZE ?= 128
FLASH_BASE_ADDR = 0x08000000
FLASH_BOOTLDR_PAYLOAD_SIZE_KB = $(shell echo $$(($(FLASH_SIZE) - $(BOOTLOADER_SIZE))))

# Default config
#CONFIG ?= -DWINUSB_SUPPORT -DENABLE_CHECKSUM -DENABLE_WATCHDOG=20
CONFIG ?= -DWINUSB_SUPPORT -DENABLE_GPIO_DFU_BOOT -DGPIO_DFU_BOOT_PORT=GPIOB -DGPIO_DFU_BOOT_PIN=2 -DGPIO_DFU_BOOT_PIN_NOPD -DENABLE_DFU_UPLOAD -DUSE_BACKUP_REGS -DENABLE_LED_STATUS -DGPIO_LED_STATUS_PORT=GPIOC -DGPIO_LED_STATUS_PIN=13 -DENABLE_SHORT_POLL
#CONFIG ?= -DWINUSB_SUPPORT -DENABLE_GPIO_DFU_BOOT -DGPIO_DFU_BOOT_PORT=GPIOB -DGPIO_DFU_BOOT_PIN=11 -DENABLE_CH32F103 -DENABLE_DFU_UPLOAD -DUSE_BACKUP_REGS -DENABLE_LED_STATUS -DGPIO_LED_STATUS_PORT=GPIOC -DGPIO_LED_STATUS_PIN=13 -DENABLE_SHORT_POLL -DENABLE_USB_INT_PULLUP

# For GPIO DFU booting:  -DENABLE_GPIO_DFU_BOOT -DGPIO_DFU_BOOT_PORT=GPIOB -DGPIO_DFU_BOOT_PIN=2
# To protect bootloader from accidental writes: -DENABLE_WRITEPROT
# To protect your payload from DFU reads: -DENABLE_SAFEWRITE
# To pull up resistor on some bluepill board is too weak, to not enable internal pulldown resistor: -DGPIO_DFU_BOOT_PIN_NOPD
# To support ENABLE_CH32F103 (requires additional USB initialization and fast flash programming, require addtional 168bytes): -DENABLE_CH32F103
# To use backup register intead of RAM for boot signature (requires additional 36 bytes): -DUSE_BACKUP_REGS
# To enable status LED, define port and pin (require additional 104 bytes): -DENABLE_LED_STATUS -DGPIO_LED_STATUS_PORT=GPIOC -DGPIO_LED_STATUS_PIN=13
# To reduce Poll Timeout: -DENABLE_SHORT_POLL
# To enable USB internal pull up register (only for CH32F103): -DENABLE_USB_INT_PULLUP

# Can be overriden with custom VID/PID
USB_VID ?= 0xdead
USB_PID ?= 0xca5d

CFLAGS = -Os -g3 -std=c11 -Wall -pedantic -Werror \
	-ffunction-sections -fdata-sections -Wno-overlength-strings \
	-mcpu=cortex-m3 -mthumb -DSTM32F1 -fno-builtin-memcpy  \
	-fno-builtin-strlen -pedantic -DVERSION=\"$(GIT_VERSION)\" \
	-DUSB_PID=$(USB_PID) -DUSB_VID=$(USB_VID) \
	-flto $(CONFIG)

LDFLAGS = -g3 -ffunction-sections -fdata-sections \
	-Wl,-Tstm32f103.ld -nostartfiles -lc -lnosys \
	-mthumb -mcpu=cortex-m3 -Wl,-gc-sections -flto \
	-Wl,--print-memory-usage

all:	bootloader-dfu-fw.bin

# DFU bootloader firmware
bootloader-dfu-fw.elf: init.o main.o usb.o
	$(CC) $^ -o $@ $(LDFLAGS) -Wl,-Ttext=$(FLASH_BASE_ADDR) -Wl,-Map,bootloader-dfu-fw.map

%.bin: %.elf
	$(OBJCOPY) -O binary $^ $@

%.o: %.c | flash_config.h
	$(CC) -c $< -o $@ $(CFLAGS)

flash_config.h:
	echo "#define FLASH_BASE_ADDR $(FLASH_BASE_ADDR)" > flash_config.h
	echo "#define FLASH_SIZE_KB $(FLASH_SIZE)" >> flash_config.h
	echo "#define FLASH_BOOTLDR_PAYLOAD_SIZE_KB $(FLASH_BOOTLDR_PAYLOAD_SIZE_KB)" >> flash_config.h
	echo "#define FLASH_BOOTLDR_SIZE_KB $(BOOTLOADER_SIZE)" >> flash_config.h

clean:
	-rm -f *.elf *.o *.bin *.map flash_config.h

