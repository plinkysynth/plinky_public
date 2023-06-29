#ifdef EMU
extern u8 _flash[512 * 1024];
#define FLASH_ADDR_256 ((size_t)_flash)
#else
#define FLASH_ADDR_256  (0x08000000 + 256*2048) 
#endif

void jumptobootloader(void) {
#ifndef EMU
	// todo - maybe set a flag in the flash and then use NVIC_SystemReset() which will cause it to jumptobootloader earlier
	// https://community.st.com/s/question/0D50X00009XkeeW/stm32l476rg-jump-to-bootloader-from-software
	typedef void (*pFunction)(void);
	pFunction JumpToApplication;
	HAL_RCC_DeInit();
	HAL_DeInit();
	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	SysTick->VAL = 0;
	__disable_irq();
	__DSB();
	__HAL_SYSCFG_REMAPMEMORY_SYSTEMFLASH();/* Remap is bot visible at once. Execute some unrelated command! */
	__DSB();
	__ISB();
	JumpToApplication = (void (*)(void)) (*((uint32_t*)(0x1FFF0000 + 4)));
	__set_MSP(*(__IO uint32_t*) 0x1FFF0000);
	JumpToApplication();
#else
	while (1);
#endif
}

#ifdef EMU
u8 _flash[512 * 1024];
#define FLASH_TYPEPROGRAM_DOUBLEWORD 0
//#define NOFILE
#ifndef NOFILE
FILE* _flashf;
#else
bool flashinited;
#endif
void openflash(void) {
#ifdef NOFILE
	if (flashinited) return;
	flashinited = true;
	memset(_flash, -1, sizeof(_flash));

#else
	if (_flashf)
		return;
	_flashf = fopen("flashmcu.raw", "rb");
	if (!_flashf) {
		memset(_flash, -1, sizeof(_flash));
		_flashf = fopen("flashmcu.raw", "wb");
		fwrite(_flash, sizeof(_flash), 1, _flashf);
		fclose(_flashf);
	}
	else {
		fread(_flash, sizeof(_flash), 1, _flashf);
		fclose(_flashf);
	}
	_flashf = fopen("flashmcu.raw", "r+b");
#endif
}

int HAL_FLASH_Program(int flags, uint32_t addr, uint64_t val) {
	addr -= (u32)FLASH_ADDR_256;
	if (addr >= sizeof(_flash))
		return 0;
	openflash();
	(*(uint64_t*)&_flash[addr]) = val;
#ifndef NOFILE
	fseek(_flashf, addr, SEEK_SET);
	fwrite(&val, 1, 8, _flashf);
	fflush(_flashf);
#endif
	return 0;
}
void HAL_FLASH_Unlock(void) {
}
void HAL_FLASH_Lock(void) {
#ifndef NOFILE
	if (_flashf) fflush(_flashf);
#endif
}
#pragma pack(push,1)
typedef struct UF2_Block {
	// 32 byte header
	uint32_t magicStart0;
	uint32_t magicStart1;
	uint32_t flags;
	uint32_t targetAddr;
	uint32_t payloadSize;
	uint32_t blockNo;
	uint32_t numBlocks;
	uint32_t familyID; // or fileSize;
	uint8_t data[476];
	uint32_t magicEnd;
} UF2_Block;
static_assert(sizeof(UF2_Block) == 512, "?");
#pragma pack(pop)
void ApplyUF2File(const char* fname) {
	FILE* f = fopen(fname, "rb");
	if (!f) return;
	while (f) {
		UF2_Block blk;
		if (1 != fread(&blk, 512, 1, f)) break;
		if (blk.magicStart0 != 0x0A324655) continue;
		if (blk.magicStart1 != 0x9E5D5157) continue;
		if (blk.magicEnd != 0x0AB16F30) continue;
		if ((blk.flags & 0x00002000) && blk.familyID != 0x00ff6919) continue;
		if (blk.payloadSize > 256) continue;
		if (blk.flags & 0x00000001) continue; // not main flash
		if (blk.targetAddr >= 0x08080000 && blk.targetAddr <= 0x08100000) {
			for (int i = 0; i < blk.payloadSize; i += 8) {
				HAL_FLASH_Program(0, FLASH_ADDR_256 + i + (blk.targetAddr - 0x08080000), *(uint64_t*)&blk.data [ i ]);
			}
		}
	}
	fclose(f);
}

#endif


int flash_erase_page(u8 page) {
#ifndef EMU
	if (0) {
		// BULK ERASE!!!
		FLASH_EraseInitTypeDef EraseInitStruct;
		EraseInitStruct.TypeErase = FLASH_TYPEERASE_MASSERASE;
		EraseInitStruct.Banks = FLASH_BANK_2;
		EraseInitStruct.Page = /*page*/0;
		EraseInitStruct.NbPages = /*1*/ 255;
		uint32_t SECTORError = 0;
		if (HAL_FLASHEx_Erase(&EraseInitStruct, &SECTORError) != HAL_OK) {
			DebugLog("flash erase error %d\r\n", SECTORError);
			return -1;
		}
		else {
			DebugLog("flash erased ok!\r\n");
		}

		__HAL_FLASH_DATA_CACHE_DISABLE();
		__HAL_FLASH_INSTRUCTION_CACHE_DISABLE();
		__HAL_FLASH_DATA_CACHE_RESET();
		__HAL_FLASH_INSTRUCTION_CACHE_RESET();
		__HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
		__HAL_FLASH_DATA_CACHE_ENABLE();
	}
	else {
		// custom page erase
		FLASH_WaitForLastOperation((uint32_t)FLASH_TIMEOUT_VALUE);
		SET_BIT(FLASH->CR, FLASH_CR_BKER); // bank 2
#ifdef HALF_FLASH
		MODIFY_REG(FLASH->CR, FLASH_CR_PNB, ((page & 0x7FU) << FLASH_CR_PNB_Pos));
#else
		MODIFY_REG(FLASH->CR, FLASH_CR_PNB, ((page & 0xFFU) << FLASH_CR_PNB_Pos));
#endif
		SET_BIT(FLASH->CR, FLASH_CR_PER);
		SET_BIT(FLASH->CR, FLASH_CR_STRT);
		FLASH_WaitForLastOperation((uint32_t)FLASH_TIMEOUT_VALUE);
		CLEAR_BIT(FLASH->CR, (FLASH_CR_PER | FLASH_CR_PNB));
	}
#ifndef HALF_FLASH
	u32 *mem=(u32*)(FLASH_ADDR_256+page*2048);
	for (int i=0;i<2048/4;++i) if (mem[i] != 0xffffffff) {
		DebugLog("flash mem page %d failed to erase at address %d - val %08x\r\n", page, i*4, mem[i]);
		break;
	}
#endif
//	DebugLog("flash erase done!\r\n");
#else
	memset(_flash + page * 2048, -1, 2048);
#ifndef NOFILE
	if (_flashf) {
		fseek(_flashf, page*2048, SEEK_SET);
		fwrite(_flash+page*2048, 1, 2048, _flashf);
		fflush(_flashf);
	}
#endif
#endif
	return 0;
}

uint64_t* flash_program_block(void* dst, void* src, int size) {
	DebugLog("program block %08x size %d\r\n", dst, size);
	uint64_t* s = (uint64_t*)src;
	volatile uint64_t* d = (volatile uint64_t*)dst;
	int osize=size;
	while (size >= 8) {
#ifdef EMU
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, (uint32_t)(size_t)(d++), *s++);
#else
		if (1) {
			HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, (uint32_t)(size_t)(d++), *s++);
		} else {
			// this version seems to fail sometimes?
			u32 Address = (uint32_t)(size_t)(d++);

			uint64_t Data = *s++;
			FLASH_WaitForLastOperation((uint32_t)FLASH_TIMEOUT_VALUE);
			SET_BIT(FLASH->CR, FLASH_CR_PG);
			*(__IO uint32_t*)Address = (uint32_t)Data;
			__ISB();
			*(__IO uint32_t*)(Address + 4U) = (uint32_t)(Data >> 32);
		}
#endif
		size -= 8;
		//HAL_Delay(5);
	}
	int fail=0;
	{
		uint64_t* s = (uint64_t*)src;
		volatile uint64_t* d = (volatile uint64_t*)dst;
		for (int i=0;i<osize;i+=8) {
			if (*s!=*d) {
				u32 s0=(*s);
				u32 s1=(*s>>32);
				u32 d0=(*d);
				u32 d1=(*d>>32);
				DebugLog("flash program failed at offset %d - %08x %08x vs dst %08x %08x\r\n", i, s0,s1,d0,d1);
				++fail;
			}
			s++; d++;
		}
	}
	if (fail!=0)
		DebugLog("flash program block failed!\r\n");
	return (uint64_t*)d;
}

