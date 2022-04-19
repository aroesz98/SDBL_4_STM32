# SDBL_4_STM32
MicroSD Bootloader for STM32

Hello. It's my first bootloader for STM32.

For my own needs I wrote it to make software update easier without connecting MCU to computer.
If you want to use this bootloader you need at first create new project.

Than go to "STM32F401CCUX_FLASH.ld" or "STM32F411CEUX_FLASH.ld" and replace this piece of code:

    FLASH    (rx)    : ORIGIN = 0x8000000,   LENGTH = 256K
    
to:
    
    FLASH    (rx)    : ORIGIN = 0x8008000,   LENGTH = 224K
    
SPI3 is used by default. If you want to choose other spi/sd_cs_pin change your project configuration and go to sd_drv.h.
Make sure if your SPI line and SD_CS pin are correct.
