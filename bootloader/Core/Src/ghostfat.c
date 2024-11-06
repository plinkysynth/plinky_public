// nb Ive hacked up this file incredibly, but I started from the UF2 version
// so heres the license:
/*
 The MIT License (MIT)

 Copyright (c) 2018 Microsoft Corp.

 Permission is hereby granted, free of charge, to any person obtaining a
 copy of this software and associated documentation files (the "Software"),
 to deal in the Software without restriction, including without limitation
 the rights to use, copy, modify, merge, publish, distribute, sublicense,
 and/or sell copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 DEALINGS IN THE SOFTWARE.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "main.h"

#undef FLASH_PAGE_SIZE
#define FLASH_PAGE_SIZE 4096 // the mcu does 2k, but the SPI does 4k. lets go with 4k
#define USER_FLASH_START 65536
#define USER_FLASH_END (512*1024)
#define UF2_FAMILY 0x00ff6919
#define VALID_FLASH_ADDR(addr, sz) (USER_FLASH_START <= (addr) && (addr) + (sz) <= USER_FLASH_END)


void target_flash_lock(void) {
	HAL_FLASH_Lock();
}

void target_flash_unlock(void) {
	HAL_FLASH_Unlock();
}

void scb_reset_system(void) {
	HAL_NVIC_SystemReset();
}

#include "uf2.h"

typedef struct {
	char name[8];
	char ext[3];
	uint8_t attrs;
	uint8_t reserved;
	uint8_t createTimeFine;
	uint16_t createTime;
	uint16_t createDate;
	uint16_t lastAccessDate;
	uint16_t highStartCluster;
	uint16_t updateTime;
	uint16_t updateDate;
	uint16_t startCluster;
	uint32_t size;
} __attribute__((packed)) DirEntry;

void DebugLog(const char *fmt, ...);
#define DBG(msg,...) DebugLog(msg "\r\n", __VA_ARGS__)

struct UF2File {
	const char name[11];
	const char *content;
	int size;
};

const char infoUf2File[] = //
		"UF2 Bootloader v1.0.3 Plinky\r\n"
				"Model: Plinky Synth v1.0.3\r\n"
				"Board-ID: STM32L476-Plinky-103\r\n";

const char indexFile[] = //
		"<!doctype html>\n"
				"<html>"
				"<body>"
				"<script>\n"
				"location.replace(\"https://plinkysynth.com/fw\");\n"
				"</script>"
				"</body>"
				"</html>\n";

// ae - rewrite this part completely, I dont like the define mess
// the sizes here are as reported by FAT, so for UF2 files they are double the size on the actual flash chip
static const struct UF2File info[] = {
		{ .name = "INDEX   HTM", .content = indexFile, 			.size = sizeof(indexFile) - 1 },
		{ .name = "INFO_UF2TXT", .content = infoUf2File, 		.size = sizeof(infoUf2File) - 1 },
		{ .name = "BOOTLOADUF2", .content = (void*) DELAY_BUF, 	.size = 128 * 1024 },
		{ .name = "CURRENT UF2", .content = (void*) 0x08010000, .size = (1024 - 128) * 1024 },
		{ .name = "PRESETS UF2", .content = (void*) 0x08080000, .size = 1024 * 1024- 4 * 1024 },
		{ .name = "CALIB   UF2", .content = (void*) 0x080FF800, .size = 4 * 1024 },
		{ .name = "SAMPLE0 UF2", .content = (void*) 0x40000000,	.size = 8 * 1024 * 1024 },
		{ .name = "SAMPLE1 UF2", .content = (void*) 0x40400000, .size = 8 * 1024 * 1024 },
		{ .name = "SAMPLE2 UF2", .content = (void*) 0x40800000,	.size = 8 * 1024 * 1024 },
		{ .name = "SAMPLE3 UF2", .content = (void*) 0x40c00000, .size = 8 * 1024 * 1024 },
		{ .name = "SAMPLE4 UF2", .content = (void*) 0x41000000,	.size = 8 * 1024 * 1024 },
		{ .name = "SAMPLE5 UF2", .content = (void*) 0x41400000, .size = 8 * 1024 * 1024 },
		{ .name = "SAMPLE6 UF2", .content = (void*) 0x41800000,	.size = 8 * 1024 * 1024 },
		{ .name = "SAMPLE7 UF2", .content = (void*) 0x41c00000, .size = 8 * 1024 * 1024 },
};

static inline uint32_t mix(uint32_t a,uint32_t b,uint32_t c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8); \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12);  \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
  return c;
}
uint32_t get_serialno(void) {
	uint32_t 	uid0=HAL_GetUIDw0 ();
	uint32_t 	uid1=HAL_GetUIDw1 ();
	uint32_t 	uid2=HAL_GetUIDw2 ();
	return mix(uid0,uid1,uid2);
}

#define NUM_FILES 14
#define FIRST_UF2_FILE 2

// these dont count the MBR!
#define RESERVED_SECTORS 1
#define ROOT_DIR_SECTORS 32
#define SECTORS_PER_FAT 129

#define SECTORS_PER_CLUSTER 8

#define START_FAT0 RESERVED_SECTORS // 1
#define START_FAT1 (START_FAT0 + SECTORS_PER_FAT) // 130+1
#define START_ROOTDIR (START_FAT1 + SECTORS_PER_FAT) // 259+1
#define START_CLUSTERS (START_ROOTDIR + ROOT_DIR_SECTORS) // 260+1

const static uint8_t boot_sector[] = {
	0xeb, 0x3c, 0x90, // jump
	'M', 'S', 'W', 'I', 'N', '4', '.', '1',  
	0x00, 0x02, // sector size 512
	SECTORS_PER_CLUSTER, // sectors per cluster
	0x01, 0x00, // reserved clusters (minus the MBR!)
	
	0x02, // number of FATs // 16
	0x00, 0x02, // root directory entries - 512 (32 sectors)
	0x00, 0x00, // total sectors (16 bit)
	0xf8, // media descriptor
	0x81, 0x00, // sectors per FAT
	0x01, 0x00, // sectors per track
	0x01, 0x00, // number of heads
	0x01,0,0,0, // hidden sectors

	0xff, 0xff, 0x03, 0x00, // total sectors (32 bit) // 32
	0, // drive number
	0, // reserved
	0x29, // extended boot signature
	0, 0, 0, 0, // volume serial number // 39
	'P', 'L', 'I', 'N', 'K', 

	'Y', ' ', ' ', ' ', ' ', ' ', // volume label
	'F', 'A', 'T', '1', '6', ' ', ' ', ' ', // filesystem identifier
	0xeb, 0xfe
};
static_assert(sizeof(boot_sector)==0x40,"boot sector size");


static uint32_t ms;
#ifdef FLASH_PAGE_SIZE
#define NO_CACHE 0xffffffff
static uint32_t flashAddr = NO_CACHE;
static uint8_t flashBuf[FLASH_PAGE_SIZE] __attribute__((aligned(4)));
static uint32_t lastFlush;

#ifdef NEW_PINOUT
#define SPI_PORT GPIOE
#define SPI_CS0_PIN_ GPIO_PIN_1
#define SPI_CS1_PIN_ GPIO_PIN_0
#else
#define SPI_PORT GPIOD
#define SPI_CS0_PIN_ GPIO_PIN_0
#define SPI_CS1_PIN_ GPIO_PIN_0
#endif
typedef unsigned char u8;
typedef uint32_t u32;
u8 cspin = SPI_CS0_PIN_;
volatile u8 dummy;
extern SPI_HandleTypeDef hspi2;
#define CHECKRV(spirv, msg) if (spirv!=0 ) DebugLog("SPI ERROR %d " msg "\r\n", spirv);
void spidelay(void) {
	for (int i = 0; i < 10; ++i)
		dummy++;
}
u8 spirxbuf[256 + 4];
u8 spibigtx[256 + 4];
void spi_setcs(void) {
	SPI_PORT->BSRR = SPI_CS1_PIN_ | SPI_CS0_PIN_;
	SPI_PORT->BRR = cspin;
	spidelay();
}
void spi_clearcs(void) {
	SPI_PORT->BSRR = SPI_CS1_PIN_ | SPI_CS0_PIN_;
	spidelay();
}
void spi_setchip(u32 addr) {
	cspin = ((addr >> 24) & 1) ? SPI_CS1_PIN_ : SPI_CS0_PIN_;
}
int spi_command(u8 *txbuf, int cmdlen) {
	spi_setcs();
	int spirv = HAL_SPI_TransmitReceive(&hspi2, (uint8_t*) txbuf, (uint8_t*) spirxbuf, cmdlen, -1);
	spi_clearcs();
	return spirv;
}
int spi_writeenable(void) {
	u8 spitxbuf[1] = { 6 };
	int spirv = spi_command(spitxbuf, 1);
	CHECKRV(spirv, "spi_writeenable");
	return spirv;
}
int spi_readid(void) {
	u8 spitxbuf[6] = { 0x90, 0, 0, 0, 0, 0 };
	int spirv = spi_command(spitxbuf, 6);
	CHECKRV(spirv, "spi_readid");
	return (spirv == 0) ? (spirxbuf[4] + (spirxbuf[5] << 8)) : -1;
}
int spi_readstatus(void) {
	u8 spitxbuf[2] = { 5, 0 };
	int spirv = spi_command(spitxbuf, 2);
	CHECKRV(spirv, "spi_readstatus1");
	return (spirv == 0) ? (spirxbuf[1]) : -1;
}

int spi_waitnotbusy(const char *msg) {
	int spirv = 0;
	int i = HAL_GetTick();
	u8 spitxbuf[1] = { 5 };
	spi_setcs();
	spirv = HAL_SPI_TransmitReceive(&hspi2, (uint8_t*) spitxbuf, (uint8_t*) spirxbuf, 1, -1);
	CHECKRV(spirv, "spi_waitnotbusy1");
	spirxbuf[0] = 0xff;
	while (spirxbuf[0] & 1) {
		spirxbuf[0] = 0;
		spirv = HAL_SPI_TransmitReceive(&hspi2, (uint8_t*) spitxbuf, (uint8_t*) spirxbuf, 1, -1);
		CHECKRV(spirv, "spi_waitnotbusy2");
		if (spirv)
			break;
	}
	spi_clearcs();
	int t = HAL_GetTick() - i;
	if (t > 10)
		DebugLog("flash write/erase operation [%s] took %dms\r\n", msg, t);
	return spirv;
}

int spi_read256(u32 addr, u8 *dst) {
	spi_setchip(addr);
	spibigtx[0] = 3;
	spibigtx[1] = addr >> 16;
	spibigtx[2] = addr >> 8;
	spibigtx[3] = addr >> 0;
	int spirv = spi_command(spibigtx, 4 + 256);
	CHECKRV(spirv, "spi_read256");
	memcpy(dst, spirxbuf + 4, 256);
	return spirv;
}
int spi_read4k(u32 addr, u8 *dst) {
	for (int i = 0; i < 4096; i += 256, addr += 256, dst += 256) {
		int spirv = spi_read256(addr, dst);
		if (spirv)
			return spirv;
	}
	return 0;
}
int spi_write4k(u32 addr, u8 *src) {
	spi_setchip(addr);
	spi_writeenable();
	spibigtx[0] = 0x20;
	spibigtx[1] = addr >> 16;
	spibigtx[2] = addr >> 8;
	spibigtx[3] = addr >> 0;
	int spirv = spi_command(spibigtx, 4);
	CHECKRV(spirv, "spi_erase");
	if (spirv != 0)
		return spirv;
	spi_waitnotbusy("erase");
	for (int p = 0; p < 4096; p += 256, addr += 256, src += 256) {
		spi_writeenable();
		spibigtx[0] = 2;
		spibigtx[1] = addr >> 16;
		spibigtx[2] = addr >> 8;
		spibigtx[3] = addr >> 0;
		memcpy(spibigtx + 4, src, 256);
		spirv = spi_command(spibigtx, 4 + 256);
		CHECKRV(spirv, "spi_write");
		if (spirv == 0)
			spirv = spi_waitnotbusy("write");
		if (spirv)
			break;
	}
	return spirv;
}

void flash_program_array(void *addr, void *srcbuf, int size_bytes) {
	FLASH_EraseInitTypeDef EraseInitStruct;
	int page = (((size_t) addr) & 0xffffff) / 2048;
	if (page < 32 || page >= 512) // protect bootloader!
		return;
	EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
	EraseInitStruct.Banks = (page >= 256) ? FLASH_BANK_2 : FLASH_BANK_1;
	EraseInitStruct.Page = /*page*/page & 255;
	EraseInitStruct.NbPages = (size_bytes + 2047) / 2048;
	uint32_t SECTORError = 0;
	if (HAL_FLASHEx_Erase(&EraseInitStruct, &SECTORError) != HAL_OK) {
		DebugLog("flash %d erase error %d\r\n", page, SECTORError);
		return;
	} else {
		DebugLog("flash %d erased ok!\r\n", page);
	}
	__HAL_FLASH_DATA_CACHE_DISABLE();
	__HAL_FLASH_INSTRUCTION_CACHE_DISABLE();
	__HAL_FLASH_DATA_CACHE_RESET();
	__HAL_FLASH_INSTRUCTION_CACHE_RESET();
	__HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
	__HAL_FLASH_DATA_CACHE_ENABLE();
	uint64_t *s = (uint64_t*) srcbuf;
	volatile uint64_t *d = (volatile uint64_t*) addr;
	for (; size_bytes > 0; size_bytes -= 8) {
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, (uint32_t) (size_t) (d++), *s++);
	}
}

void flushFlash(void) {
	lastFlush = ms;
	if (flashAddr == NO_CACHE)
		return;
	DBG("Flush at %x", flashAddr);
	if ((size_t) flashAddr & 0x40000000) {
		DBG("Write SPI flush at %x", flashAddr);
		spi_write4k((size_t) flashAddr, flashBuf);
	} else if (flashAddr >= 0x08010000 && flashAddr < 0x08100000) {
		if (memcmp(flashBuf, (void*) flashAddr, FLASH_PAGE_SIZE) != 0) {
			DBG("MCU Write flush at %x", flashAddr);
			target_flash_unlock();
			flash_program_array((void*) flashAddr, (void*) flashBuf,
			FLASH_PAGE_SIZE);
			target_flash_lock();
		}
	} else if (flashAddr >= DELAY_BUF && flashAddr <= DELAY_BUF + 65536) {
		memcpy((void*) flashAddr, flashBuf, FLASH_PAGE_SIZE);
		u32 sector = (flashAddr & 65535) / FLASH_PAGE_SIZE;
		((char*) REVERB_BUF)[sector] = 1; // mark this sector as having been updated
		((uint32_t*) REVERB_BUF)[64] = 0xa738ea75; // leave a magic number there

		DBG("bootloader flush at %x sector %d", flashAddr, sector);
	}
	flashAddr = NO_CACHE;
}

int clustersize(int bytes) {
	return (bytes+SECTORS_PER_CLUSTER*512-1)/(SECTORS_PER_CLUSTER*512);
}

void flash_write(uint32_t dst, const uint8_t *src, int len) {
	uint32_t newAddr = dst & ~(FLASH_PAGE_SIZE - 1);
	if (newAddr != flashAddr) {
		flushFlash();
		flashAddr = newAddr;
		if ((size_t) flashAddr & 0x40000000)
			spi_read4k((size_t) flashAddr, flashBuf);
		else
			memcpy(flashBuf, (void*) newAddr, FLASH_PAGE_SIZE);
	}
	memcpy(flashBuf + (dst & (FLASH_PAGE_SIZE - 1)), src, len);
}
#else
void flushFlash(void) {}
#endif

// called roughly every 1ms
void ghostfat_1ms() {
	ms++;
#ifdef FLASH_PAGE_SIZE
	if (lastFlush && ms - lastFlush > 100) {
		flushFlash();
	}
#endif
}

static void padded_memcpy(char *dst, const char *src, int len) {
	for (int i = 0; i < len; ++i) {
		if (*src)
			*dst = *src++;
		else
			*dst = ' ';
		dst++;
	}
}


int read_block(uint32_t block_no, uint8_t *data) {
	memset(data, 0, 512);
	if (block_no == 0) {
		// MBR - including partition table
		*(uint32_t*)(data+0x1b8) = get_serialno();
		data[0x1c2] = 0xe; // FAT16
		data[0x1c6] = 0x1;
		data[0x1ca] = 0xff; // disk size in sectors
		data[0x1cb] = 0xff;
		data[0x1cc] = 0x03;
		data[0x1fe] = 0x55;
		data[0x1ff] = 0xaa;
		return 0;
	}
	block_no--; // skip mbr
	uint32_t sectionIdx = block_no;
	if (block_no == 0) {
		// Boot sector
		memcpy(data, boot_sector,sizeof(boot_sector));
		*(uint32_t*)(data+39) = get_serialno();
		data[0x1fe] = 0x55;
		data[0x1ff] = 0xaa;
	} else if (block_no < START_ROOTDIR) {
		sectionIdx -= START_FAT0;
		if (sectionIdx >= SECTORS_PER_FAT)
			sectionIdx -= SECTORS_PER_FAT;
		if (sectionIdx == 0) {
			data[0] = 0xf8;
			data[1] = 0xff;
			data[2] = 0xff;
			data[3] = 0xff;
		}
		int basecluster = sectionIdx * 256;
		int first_cluster=2;
		for (int f = 0; f < NUM_FILES; ++f) {
			int c0 = first_cluster - basecluster;
			int num_clusters=clustersize(info[f].size);
			first_cluster+=num_clusters;
			int c1 = c0 + num_clusters;
			int last = c1 - 1;
			if (c0 < 0)
				c0 = 0;
			if (c1 > 256)
				c1 = 256;
			for (int i = c0; i < c1; ++i)
				((uint16_t*) (void*) data)[i] = (i == last) ? 0xffff : i + basecluster + 1;
		}
	} else if (block_no < START_CLUSTERS) {
		sectionIdx -= START_ROOTDIR;
	// 19 Jul 2019 10:23:00
#define PLINKY_TIME_FRAC 100
#define PLINKY_TIME ((10u << 11u) | (23u << 5u) | (00u >> 1u))
#define PLINKY_DATE ((39u << 9u) | (7u << 5u) | (19u))
		if (sectionIdx == 0) {
			DirEntry *d = (void*) data;
			padded_memcpy(d->name, "PLINKY     ", 11);
			d->attrs = 0x28;
			d->createTimeFine = PLINKY_TIME_FRAC;
			d->createTime = d->updateTime = PLINKY_TIME;
			d->createDate = d->updateDate = PLINKY_DATE;
			int first_cluster=2;
			for (int i = 0; i < NUM_FILES; ++i) {
				d++;
				const struct UF2File *inf = &info[i];
				d->size = inf->size;
				d->startCluster = first_cluster;
				d->attrs = 0;	
				d->createTimeFine = PLINKY_TIME_FRAC;
				d->createTime = d->updateTime = PLINKY_TIME;
				d->createDate = d->updateDate = PLINKY_DATE;
				first_cluster+=clustersize(d->size);
				padded_memcpy(d->name, inf->name, 11);
			}
		}
	} else {
		sectionIdx -= START_CLUSTERS;
		int first_cluster=2;
		for (int f = 0; f < NUM_FILES; ++f) {
			int sector_in_file = sectionIdx - (first_cluster - 2) * SECTORS_PER_CLUSTER;
			first_cluster+=clustersize(info[f].size);
			if (sector_in_file < 0)
				continue;
			if (f < FIRST_UF2_FILE) { // non-uf2 file - return data raw
				int sizeleft = info[f].size - sector_in_file * 512;
				if (sizeleft <= 0)
					continue;
				if (sizeleft > 512)
					sizeleft = 512;
				memcpy(data, info[f].content + sector_in_file * 512, sizeleft);
			} else { // uf2 file - a 512 byte sector only serves 256 bytes of payload
				if (sector_in_file >= info[f].size / 512)
					continue;
				const char *addr = info[f].content + (sector_in_file * 256);
				UF2_Block *bl = (void*) data;
				bl->magicStart0 = UF2_MAGIC_START0;
				bl->magicStart1 = UF2_MAGIC_START1;
				bl->magicEnd = UF2_MAGIC_END;
				bl->blockNo = sector_in_file;
				bl->numBlocks = info[f].size / 512;
				bl->targetAddr = (size_t) addr;
				bl->payloadSize = 256;
				
				if ((size_t)addr & 0x40000000) {
					// read spi
					spi_read256((size_t) addr, bl->data);
				} else {
					if ((size_t)addr>=0x08000000 && (size_t)addr<0x08080000) { // code region
						bl->flags |= UF2_FLAG_FAMILYID_PRESENT; // family ID present
						bl->familyID = UF2_FAMILY; // STM32L4xx
					}
					memcpy(bl->data, (void*) addr, bl->payloadSize);
				}
			}

			break;
		}
	}
	return 0;
}

int write_block(uint32_t lba, const uint8_t *copy_from) {
	const UF2_Block *bl = (const void*) copy_from;
	if (!is_uf2_block(bl) || !UF2_IS_MY_FAMILY(bl)) {
		return 0;
	}
	if ((bl->flags & UF2_FLAG_NOFLASH) || bl->payloadSize > 256 || (bl->targetAddr & 0xff)) {
		DBG("Skip block at %x", bl->targetAddr);
		return 0;
	}
	for (int f = FIRST_UF2_FILE; f < NUM_FILES; ++f) {
		int sector_in_file = (bl->targetAddr - (size_t) info[f].content) / 256;
		//if (sector_in_file!=bl->blockNo)
		//	   continue;
		if (sector_in_file < 0 || sector_in_file >= info[f].size / 512)
			continue;
		flash_write(bl->targetAddr, bl->data, bl->payloadSize);
		break;
	}
	return 0;
}

