
STM32F103 DFU bootloader
========================

This is a tiny bootloader (under 4KB) for STM32F103 (although it probably
works for similar devices). It enables user to flash devices over USB
with any arbitrary payloads. It features some minimal payload checking
to ensure use apps are valid before booting them.

**_This branch has a few enchancements_**: _support status LED and fast flash programming for CH32F103X chip._

Features
--------

* Small size, ideally under 4KB to fit on the first four pages.
* RDP protection configurable at build time.
* Reboot into DFU mode support (by writing tag to RAM + reset).
* Watchdog support for failsafe.
* Total wipe on DFU downloads (avoid partial FW updates).
* Optional upload enable (to prevent firmware/data reads).
* Firmware checksum checking.
* **_Status LED_**
* **_Fast Flash programming for CH32F10x (such as CH32F103)_**


Reboot into bootloader
----------------------

One can reboot into bootloader (in DFU mode) by simply writing the magic
0xDEADBEEFCC00FFEE value to the last 8 bytes of RAM and triggering a full
system reset. This will make the bootloader start DFU mode instead of
loading the (valid) payload present in flash.

Protections
-----------

Bootloader might enable RDP (Readout protection) that will prevent debugger
over SWIO from reading data. This protection can be removed but will cause
all user flash (except the DFU bootloader) to be deleted, that's cause the
first 4KB are always write protected. It can also disable SWIO GPIOs to
prevent any debuggers from attaching to the device once booted.
The booloader also features some DFU proectections. It is possible to
disable firmware read by disabling UPLOAD commands. In order to prevent
data read it is possible to prevent partial writes, since what could allow
a small firmware being uploaded to extract data from flash. With this
protection enabled the bootloader will wipe all the blocks as soon as
an erase/write command is issued.

Force DFU mode
--------------

The bootloader can be configured to detect a GPIO condition on boot and
abort boot to go into DFU mode. The pin will be configured as an internal
pulldown and the user will need to pull it up to force DFU mode, which
will be read right after reset (there's some small delay to ensure the
pin is read correctly).

The firmware can optionally enable the Internal Watchdog on a configurable
period of 1 to 26 seconds. If the user app does not reset the watchdog
before the period is due it will reset the system and enter DFU mode.

Firmware format and checksum
----------------------------

The use firmware should be build and linked at an offset of 0x1000 (4KB)
so it can safely boot as a payload. The bootloader will check some stuff
before declaring the payload valid:

 * Stack points to somewhere in the RAM range (0x20000000).
 * The firmware contains its size at offset 0x20 (as a LE uint32).
 * The firmware 32bit XOR checksum is zero (can use offset 0x1C for that).

If these conditions are met, provided no other triggers to boot into DFU
are present, the bootloader will point VTOR to the user app and boot it.


Config flags
------------

* ENABLE_DFU_UPLOAD: Enables DFU upload commands, this is, enables reading
  flash memory (only within the user app boundaries) via DFU.
* ENABLE_SAFEWRITE: Ensures the user flash is completely erased before any
  DFU write/erase command is executed, to ensure no payloads are written
  that could lead to user data exfiltration.
* ENABLE_CHECKSUM: Forces the user app image to have a valid checksum to
  boot it, on failure it will fallback to DFU mode.
* ENABLE_WRITEPROT: Protects the first 4KB of flash against writes.
  Essentially prevents any user app from overwriting the bootloader area.
* ENABLE_PROTECTIONS: Disables JTAG at startup before jumping to user code
  and also ensures RDP protection is enabled before booting. It will update
  option bytes if that is not met and force a reset (should only happen the
  first time, after that RDP is enabled and can only be disabled via JTAG).
  This also protects the bootloader (first 4KB) like ENABLE_WRITEPROT does,
  making these two options incompatible.
* ENABLE_GPIO_DFU_BOOT: Enables DFU mode on pulling up a certain GPIO.
  You need to define GPIO_DFU_BOOT_PORT and GPIO_DFU_BOOT_PIN to either
  GPIOA, GPIOB, .. GPIOE and 0 .. 15 to indicate which port to enable and
  what pin to read from. **_When GPIO_DFU_BOOT_PIN_NOPD is defined, the pin doesn't enable internal pulldown. Some bluepill boards have a very weak external resistor for pull up/down, internal pulldown can override the external setting._**
* ENABLE_PINRST_DFU_BOOT: Enables DFU mode when a reset from the NRST pin
  occurs.
* **_ENABLE_LED_STATUS_**: Enable Status LED
  * GPIO_LED_STATUS_PORT and GPIO_LED_STATUS_PIN have to be specified for the LED status pin. Assuming that the status LED is driven through open-drain type.
  * Slow blinking when DFU is idle, Fast blinking when upload/download is in progress, fastest blinking when error
* **_ENABLE_CH32F103_**: Support CH32F103, such as CH32F103C8, which is used in some blue-pill clones
  * Support CH32F103 fast flash programming. should be more than 10x faster
  * Add additional USB initialization code to avoid enumeration problem when cold boots.
* **_ENABLE_SHORT_POLL_**: Reduce poll timeout value, it can speed up download speed significantly on some devices.
* **_ENABLE_USB_INT_PULLUP_**: Enable internal 1.5k pullup resistor for USB. Only valid for CH32F103
* **_USE_BACKUP_REGS_**: Use backup registers instead of using signature pattern at the end of SRAM. 

By default all flags are set except for DFU upload, so it's most secure.

