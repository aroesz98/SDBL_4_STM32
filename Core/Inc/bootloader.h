/*
 * bootloader.h
 *
 *  Created on: Apr 18, 2022
 *      Author: asz
 */

#ifndef INC_BOOTLOADER_H_
#define INC_BOOTLOADER_H_

#define SECTOR_SIZE 1024
#define APP_ADDRESS (uint32_t)0x08008000

#include "main.h"

FATFS SDFat;
FIL app;
FRESULT fr;

uint8_t pageData[1024];		//SD Card buffer for readed data
uint16_t erase_page = 2;	//Start sector to flash erase (Offset 0x8000)
uint32_t current_page = 32; //Start flashing from page 32 (Offset 0x8000)

typedef void __attribute__((noreturn)) (*pFunction)(void);		//Function pointer typedef

static uint8_t checkAppSize(uint32_t appsize);
static void jmp2addr(uint32_t addr);
static void writeFlashSector(uint32_t currentPage);
static HAL_StatusTypeDef upload_fw(void);

uint8_t checkAppSize(uint32_t appsize)
{
    return ((FLASH_END - APP_ADDRESS) >= appsize) ? HAL_OK : HAL_ERROR;
}

static HAL_StatusTypeDef check4update(void) {
	UINT num;
	uint32_t addr, data, cntr;

    fr = f_open(&app, "app.bin", FA_READ);
    if(fr != FR_OK) {
    	f_close(&app);
    	return HAL_OK;
    }

    addr = APP_ADDRESS;
    cntr = 0;

    do
    {
        data = 0xFFFFFFFF;
        fr   = f_read(&app, &data, 4, &num);
        if(num)
        {
            if(*(uint32_t*)addr == (uint32_t)data)
            {
                addr += 4;
                cntr++;
            }
            else
            {
                f_close(&app);
                return HAL_ERROR;
            }
        }

        if(cntr % 256 == 0)
        {
        	LED_GPIO_Port->ODR ^= LED_Pin;
        }
    } while((fr == FR_OK) && (num > 0));

    f_close(&app);
    return HAL_OK;
}

static void jmp2addr(uint32_t addr)
{
	if(((*(__IO uint32_t*)addr) & 0x2FF80000 ) == 0x20000000)
	{
		FATFS_UnLinkDriver((TCHAR*)USERPath);
		HAL_SPI_DeInit(&hspi3);
		HAL_RCC_DeInit();
		HAL_DeInit();
		pFunction JumpToApplication;
		uint32_t JumpAddress;

		/* Jump to user application */
		JumpAddress = *(__IO uint32_t*) (addr + 4);
		JumpToApplication = (pFunction) JumpAddress;

		/* Initialize user application's Stack Pointer */
		SCB->VTOR = (__IO uint32_t)APP_ADDRESS;
		__set_MSP(*(__IO uint32_t*) addr);
		JumpToApplication();
	}
}

static void writeFlashSector(uint32_t currentPage)
{
	uint32_t pageAddress = 0x8000000 + (currentPage * SECTOR_SIZE);
	uint32_t SectorError;

	FLASH_EraseInitTypeDef EraseInit;
	HAL_FLASH_Unlock();

	if ((currentPage == 32) || (currentPage == 48) || (currentPage == 64) || (currentPage % 128 == 0)) {
		EraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
		EraseInit.VoltageRange  = FLASH_VOLTAGE_RANGE_3;
		EraseInit.Banks = FLASH_BANK_1;
		EraseInit.Sector = erase_page++;
		EraseInit.NbSectors = 1;
		HAL_FLASHEx_Erase(&EraseInit, &SectorError);
	}

	uint32_t dat = 0xFFFFFFFF;
	for (int i = 0; i < SECTOR_SIZE; i += 4) {
		dat = (pageData[i] | pageData[i+1] << 8 | pageData[i+2] << 16 | pageData[i+3] << 24);
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(pageAddress + i), dat);
	}

	LED_GPIO_Port->ODR ^= LED_Pin;
	HAL_FLASH_Lock();
}

static HAL_StatusTypeDef upload_fw(void)
{
    UINT num;
    uint32_t cntr, data, addr;

    /* Open file for programming */
    fr = f_open(&app, "app.bin", FA_READ);
    if(fr != FR_OK) {
    	f_close(&app);
    	return HAL_ERROR;
    }

    /* Check size of application found on SD card */
    if(checkAppSize(f_size(&app)) != HAL_OK)
    {
        f_close(&app);
        return HAL_ERROR;
    }

    /* Step 1: Init Bootloader and Flash */
    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_BSY);
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP);
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPERR);
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_PGAERR);
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_PGPERR);
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_PGSERR);
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_RDERR);
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_WRPERR);
    HAL_FLASH_Lock();

    /* Step 2: Programming */
    cntr = 0;
    LED_GPIO_Port->BSRR = LED_Pin << 16;
    do
    {
        fr   = f_read(&app, pageData, SECTOR_SIZE, &num);
		writeFlashSector(current_page + cntr);
		cntr++;
    } while((fr == FR_OK) && (num > 0));

    f_close(&app);

    /* Step 3: Verify uploaded app */
    fr = f_open(&app, "app.bin", FA_READ);

    addr = APP_ADDRESS;
    cntr = 0;
    do
    {
        data = 0xFFFFFFFF;
        fr   = f_read(&app, &data, 4, &num);
        if(num)
        {
            if(*(uint32_t*)addr == (uint32_t)data)
            {
                addr += 4;
                cntr++;
            }
            else
            {
                f_close(&app);
                return HAL_ERROR;
            }
        }
        if(cntr % 512 == 0)
        {
        	LED_GPIO_Port->ODR ^= LED_Pin;
        }
    } while((fr == FR_OK) && (num > 0));

    LED_GPIO_Port->BSRR = LED_Pin;

    /* Step 4: Close app file */
    f_close(&app);

    /* Step 5: Enjoy */
    return HAL_OK;
}

#endif /* INC_BOOTLOADER_H_ */
