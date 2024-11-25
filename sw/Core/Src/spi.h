#define AVG_GRAINBUF_SAMPLE_SIZE 68 // 2 extra for interpolation, 2 extra for SPI address at the start
#define GRAINBUF_BUDGET (AVG_GRAINBUF_SAMPLE_SIZE*32)
#define MAX_SPI_STATE 32

#define MAX_SAMPLE_LEN (1024*1024*2) // max sample length in samples

int grainpos[32];
s16 grainbuf[GRAINBUF_BUDGET];
s16 grainbufend[32]; // for each of the 32 grain fetches, where does it end in the grainbuf?
u8 spibigtx[256 + 4];
u8 spibigrx[256 + 4];
#define EXPANDER_ZERO 0x800
#define EXPANDER_RANGE 0x7ff
#define EXPANDER_MAX 0xfff
u16 expander_out[4] = { EXPANDER_ZERO,EXPANDER_ZERO,EXPANDER_ZERO,EXPANDER_ZERO };

int spi_readgrain_dma(int gi);
volatile u8 spistate=0;
extern volatile uint8_t g_disable_fx;
volatile uint8_t g_disable_fx = 0; // if you set this to 1, it will then tick to 2 and we can guaruntee that fx wont be used

u32 startspi;
u8 alexdmamode;
volatile u32 spiduration;
volatile u8 dummy;
static inline void spidelay(void) {
	for (int i = 0; i < 10; ++i) dummy++;

}

void resetspistate(void) {
	spistate = 0;
	spiduration = RDTSC() - startspi;
	alexdmamode = 0;
#ifndef EMU
	  GPIOA->BSRR = 1<<8; // DAC cs high
#endif
}

void spi_update_dac(int dacchan);


void spi_read_done(void) {
	if (spistate >= MAX_SPI_STATE) {
#ifndef EMU
		GPIOA->BSRR = 1<<8; // DAC cs high
#endif
		if (spistate == MAX_SPI_STATE+4) {
			resetspistate();
		} else {
			int dacchan = spistate-MAX_SPI_STATE;
			if (dacchan<4 && dacchan>=0)
				spi_update_dac(dacchan);
		}
	}
	else {
		spi_readgrain_dma(spistate);
	}
}

#ifdef EMU
extern s16 _flashram[8 * MAX_SAMPLE_LEN];
s16 _flashram[8* MAX_SAMPLE_LEN];
#ifndef WASM
FILE* flashfile = 0;
#endif
void spi_wait(void){}
int spi_writeenable(void) { return 0; }
int spi_readid(void) { return 0; }
void spi_setchip(uint32_t addr) {}
int spi_waitnotbusy(const char *msg, void(*callback)(void)) { return 0;  }
int spi_erase64k(u32 addr, void (*callback)(void)) {
	memset(_flashram + addr / 2, -1, 65536);
#ifndef WASM
	if (flashfile) {
		fseek(flashfile, addr, 0);
		fwrite(_flashram + addr / 2, 1, 65536, flashfile);
		fflush(flashfile);
	}
#endif
	for (int i = 0; i < 16; ++i) {
		HAL_Delay(2);
		if (callback) callback();
	}
	return 0;
}
void spiopen(void) {
#ifndef WASM
	if (!flashfile) {
		flashfile = fopen("flashspi.raw", "r+b");
		if (!flashfile) {
			flashfile = fopen("flashspi.raw", "wb");
			if (flashfile) {
				u8 zero[1024] = {0};
				for (int i = 0; i < (MAX_SAMPLE_LEN*16)/1024; ++i)
					fwrite(zero, 1, 1024, flashfile);
				fclose(flashfile);
				flashfile = fopen("flashspi.raw", "r+b");
			}
		}
		if (flashfile) {
			fseek(flashfile, 0, 0);
			fread(_flashram, 16, MAX_SAMPLE_LEN, flashfile);
		}
	}
#endif
}

int spi_write256(u32 addr) {
	spiopen();
	memcpy(_flashram+addr/2,spibigtx+4,256);
#ifndef WASM
	if (flashfile) {
		fseek(flashfile, addr, 0);
		fwrite(_flashram + addr / 2, 1, 256, flashfile);
		fflush(flashfile);
	}
#endif
	return 0;
}
int spi_read256(u32 addr) {
	memcpy(spibigrx+4, _flashram + addr / 2, 256);
	return 0;
}


int spi_readgrain_dma(int gi) {
	spiopen();
	spistate = gi;
	if (spistate == 0)
		startspi = RDTSC();
	++spistate;
	int start = 0;
	if (gi) start = grainbufend[gi - 1];
	int len = grainbufend[gi] - start;
	u32 addr = grainpos[gi];
	
	//for (int i = 0; i < len; ++i)
	//	grainbuf[start + i] = ((addr + i - 2) * 256);

	if (len>2)
		memcpy(((u8*)&grainbuf[start])+4,_flashram+(addr&(MAX_SAMPLE_LEN*8-1)),len*2-4);
	spi_read_done();
	return 0;
}
#else

volatile bool spidone = false;

void spi_wait(void) {
	while (!spidone)
		;
	spidone = false;
}

#define CHECKRV(spirv, msg) if (spirv!=0 ) DebugLog("SPI ERROR %d " msg "\r\n", spirv);

#ifdef NEW_PINOUT
#define SPI_PORT GPIOE
#define SPI_CS0_PIN_ GPIO_PIN_1
#define SPI_CS1_PIN_ GPIO_PIN_0
#else
#define SPI_PORT GPIOD
#define SPI_CS0_PIN_ GPIO_PIN_0
#define SPI_CS1_PIN_ GPIO_PIN_0
#endif
u8 curspipin_=SPI_CS0_PIN_;
void spi_setchip(u32 addr) {
	curspipin_ = (addr&(1<<24)) ? SPI_CS1_PIN_ : SPI_CS0_PIN_;
}
void spi_setexpanderdac(void) {
	curspipin_ = 0;
	hspi2.Instance->CR1 &= ~(1 | 64);
	hspi2.Instance->CR1 |= 64;
	SPI_PORT->BSRR = SPI_CS1_PIN_ | SPI_CS0_PIN_;
	 GPIOA->BRR = 1<<8; // dac cs low
}
static inline void spi_assert_cs(void) {
	SPI_PORT->BSRR = SPI_CS1_PIN_ | SPI_CS0_PIN_;	
	hspi2.Instance->CR1 &= ~( 64);
	hspi2.Instance->CR1 |= 1 | 64;

	SPI_PORT->BRR = curspipin_;	
}
static inline void spi_release_cs(void) {
	SPI_PORT->BSRR = SPI_CS1_PIN_ | SPI_CS0_PIN_;	
}
int spi_writeenable(void) {
	u8 spitxbuf[1] = { 6 };
	u8 spirxbuf[1];
	spi_assert_cs();
	spidelay();
	int spirv = HAL_SPI_TransmitReceive(&hspi2, (uint8_t*) spitxbuf, (uint8_t*) spirxbuf, 1, -1);
	CHECKRV(spirv,"spi_writeenable");
	spi_release_cs();
	spidelay();
//	if (spirv==0) spi_wait();
	return spirv;
}

int spi_readid(void) {
	u8 spitxbuf[6] = { 0x90, 0, 0, 0, 0, 0 };
	u8 spirxbuf[6] = { 0 };
	spi_assert_cs();
	spidelay();
	int spirv = HAL_SPI_TransmitReceive(&hspi2, (uint8_t*) spitxbuf, (uint8_t*) spirxbuf, 6, -1);
	CHECKRV(spirv,"spi_readid");
	spi_release_cs();
	spidelay();

//	if (spirv==0) spi_wait();
	return (spirv == 0) ? (spirxbuf[4] + (spirxbuf[5] << 8)) : -1;
}

int spi_readstatus(void) {
	u8 spitxbuf[2] = { 5, 0 };
	u8 spirxbuf[2] = { 0 };
		spi_assert_cs();
	spidelay();
	int spirv = HAL_SPI_TransmitReceive(&hspi2, (uint8_t*) spitxbuf, (uint8_t*) spirxbuf, 2, -1);
	CHECKRV(spirv,"spi_readstatus1");
	spi_release_cs();
	spidelay();
	return (spirv == 0) ? (spirxbuf[1]) : -1;
}

int spi_waitnotbusy(const char *msg, void(*callback)(void)) {
	int spirv = 0;
//	spi_wait();
	int i = millis();
	u8 spitxbuf[1] = { 5 };
	u8 spirxbuf[1] = { 23 };
		spi_assert_cs();

	spirv = HAL_SPI_TransmitReceive(&hspi2, (uint8_t*) spitxbuf, (uint8_t*) spirxbuf, 1, -1);
	CHECKRV(spirv,"spi_waitnotbusy1");
	spirxbuf[0] = 0xff;
	while (spirxbuf[0] & 1) {

		if (callback)
			callback();

		spirxbuf[0] = 0;
		spirv=HAL_SPI_TransmitReceive(&hspi2, (uint8_t*) spitxbuf, (uint8_t*) spirxbuf, 1, -1);
		CHECKRV(spirv,"spi_waitnotbusy2");
//		DebugLog("%02x ", spirxbuf[0]);
		if (spirv) break;
	}
	spi_release_cs();
	spidelay();
	int t = millis() - i;
	if (t > 10)
		DebugLog("flash write/erase operation [%s] took %dms\r\n", msg, t);
	return spirv;
}
int spi_read256(u32 addr);

int spi_erase64k(u32 addr, void (*callback)(void)) {
	spi_setchip(addr);
	spi_writeenable();
	u8 spitxbuf[4] = { 0xd8, addr >> 16, addr >> 8, addr };
	u8 spirxbuf[4];
	DebugLog("spi erase %d\r\n", addr);
	spi_assert_cs();
	spidelay();
	int spirv = HAL_SPI_TransmitReceive(&hspi2, (uint8_t*) spitxbuf, (uint8_t*) spirxbuf, 4, -1);
	CHECKRV(spirv,"spi_erase1");
	spi_release_cs();
	spidelay();
	if (spirv == 0)
		spirv = spi_waitnotbusy("erase", callback);
	else {
		DebugLog("HAL_SPI_TransmitReceive returned %d in erase\r\n", spirv);
	}
	/*
	memset(spibigrx, 0, sizeof(spibigrx));
	spi_read256(addr);
	for (int i = 0; i < 256; ++i) if (spibigrx[i + 4] != (u8)(255)) {
		DebugLog("erase fail at %d\r\n", addr+i);
	}
	*/
	return spirv;
}

int spi_write256(u32 addr) {
	spi_setchip(addr);
	spi_writeenable();
	spibigtx[0] = 2;
	spibigtx[1] = addr >> 16;
	spibigtx[2] = addr >> 8;
	spibigtx[3] = addr >> 0;
		spi_assert_cs();

	spidelay();
	int spirv = HAL_SPI_TransmitReceive(&hspi2, (uint8_t*) spibigtx, (uint8_t*) spibigrx, 4 + 256, -1);
	CHECKRV(spirv,"spi_write256");

	spi_release_cs();
	spidelay();

	if (spirv == 0)
		spirv = spi_waitnotbusy("write", 0);

	if (0) {
		spi_read256(addr);
		if (memcmp(spibigrx+4,spibigtx+4,256)!=0) {
			DebugLog("spi didnt read what I wrote? %d\r\n", addr);
		}
	}

	return spirv;
}

int spi_read256(u32 addr) {
	spibigtx[0] = 3;
	spibigtx[1] = addr >> 16;
	spibigtx[2] = addr >> 8;
	spibigtx[3] = addr >> 0;
	spi_setchip(addr);
		spi_assert_cs();

	spidelay();
	int spirv = HAL_SPI_TransmitReceive(&hspi2, (uint8_t*) spibigtx, (uint8_t*) spibigrx, 4 + 256, -1);
	CHECKRV(spirv,"spi_read256");
	spi_release_cs();
	spidelay();
	return spirv;
}



static void DMA_SetConfig(DMA_HandleTypeDef *hdma, uint32_t SrcAddress, uint32_t DstAddress, uint32_t DataLength) {
	hdma->DmaBaseAddress->IFCR = (DMA_ISR_GIF1 << (hdma->ChannelIndex & 0x1CU));	/* Clear all flags */
	hdma->Instance->CNDTR = DataLength;
	if ((hdma->Init.Direction) == DMA_MEMORY_TO_PERIPH) {
		hdma->Instance->CPAR = DstAddress;
		hdma->Instance->CMAR = SrcAddress;
	} else {
		hdma->Instance->CPAR = SrcAddress;
		hdma->Instance->CMAR = DstAddress;
	}
}

void setup_spi_alex_dma(uint32_t txaddr, uint32_t rxaddr, int len) { // len is in 8 bit words
	// ALEX DMA MODE! fewer interrupts; simpler code.
	alexdmamode = 1;
	CLEAR_BIT(hspi2.Instance->CR2, SPI_CR2_LDMATX | SPI_CR2_LDMARX); // Reset the threshold bit
	SET_BIT(hspi2.Instance->CR2, SPI_RXFIFO_THRESHOLD); // Set RX Fifo threshold according the reception data length: 8bit
	// config rx dma - transfer complete callback
	__HAL_DMA_DISABLE(hspi2.hdmarx);
	DMA_SetConfig(hspi2.hdmarx, (uint32_t) &hspi2.Instance->DR, rxaddr, len);
	__HAL_DMA_DISABLE_IT(hspi2.hdmarx, (DMA_IT_HT | DMA_IT_TE));
	__HAL_DMA_ENABLE_IT(hspi2.hdmarx, (DMA_IT_TC));
	__HAL_DMA_ENABLE(hspi2.hdmarx);
	SET_BIT(hspi2.Instance->CR2, SPI_CR2_RXDMAEN); // Enable Rx DMA Request

	// config tx dma - no interrupts
	__HAL_DMA_DISABLE(hspi2.hdmatx);
	DMA_SetConfig(hspi2.hdmatx, txaddr, (uint32_t) &hspi2.Instance->DR, len);
	__HAL_DMA_DISABLE_IT(hspi2.hdmatx, (DMA_IT_HT | DMA_IT_TE | DMA_IT_TC));
	__HAL_DMA_ENABLE(hspi2.hdmatx);
	if ((hspi2.Instance->CR1 & SPI_CR1_SPE) != SPI_CR1_SPE)
		__HAL_SPI_ENABLE(&hspi2);  // Enable SPI peripheral
	SET_BIT(hspi2.Instance->CR2, SPI_CR2_TXDMAEN);
}

static u16 daccmd, dacdummy;
void spi_update_dac(int dacchan) {
	spistate=MAX_SPI_STATE+dacchan + 1; // the NEXT state
	
	int data = expander_out[dacchan & 3];

	 daccmd = (2<<14) + ((dacchan&3)<<12) + (data & 0xfff);
	 daccmd=(daccmd>>8) | (daccmd<<8);
	 spi_setexpanderdac(); // assert cs for dac
	 setup_spi_alex_dma((uint32_t)&daccmd, (uint32_t)&dacdummy, 2);
}



int spi_readgrain_dma(int gi) {
	spi_release_cs();
	u32 addr;
again:
	addr = grainpos[gi] * 2;
	spibigtx[0] = 3;
	spibigtx[1] = addr >> 16;
	spibigtx[2] = addr >> 8;
	spibigtx[3] = addr >> 0;
	spistate = gi;
	if (spistate==0)
		startspi = RDTSC();
	++spistate;
	int start = 0;
	if (gi) start = grainbufend[gi - 1];
	int len = grainbufend[gi] - start;

	if (len<=2) {
		if (spistate==MAX_SPI_STATE) {
			spi_update_dac(0);
//			resetspistate();
			return 0;
		}
		++gi;
		goto again;
	}


	if (0) {
		for (int i=0;i<len;++i)

			grainbuf[start+i]=((addr/2+i-2)*256);
		if (spistate==MAX_SPI_STATE) {
			spi_update_dac(0);
//			resetspistate();
			return 0;
		}
		++gi; goto again;
	}
	spi_setchip(addr);
	spi_assert_cs();

	setup_spi_alex_dma((uint32_t) spibigtx, (uint32_t) (grainbuf+start), len * 2);

	return 0;
}


void alexdmadone(void) {
	// replacement irq handler for the HAL guff.
	DMA_HandleTypeDef *hdma = hspi2.hdmarx; /*!< SPI Rx DMA Handle parameters             */
	uint32_t flag_it = hdma->DmaBaseAddress->ISR;
	uint32_t source_it = hdma->Instance->CCR;
	if (((flag_it & (DMA_FLAG_TC1 << (hdma->ChannelIndex & 0x1CU))) != 0U) && ((source_it & DMA_IT_TC) != 0U)) {
		__HAL_DMA_DISABLE_IT(hdma, DMA_IT_TE | DMA_IT_TC | DMA_IT_HT);
		hdma->DmaBaseAddress->IFCR = (DMA_ISR_TCIF1 << (hdma->ChannelIndex & 0x1CU));		/* Clear the transfer complete flag */
		spi_read_done();
	}
}

/*
 void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
 {
 spidone=true;
 }
 */
#endif
