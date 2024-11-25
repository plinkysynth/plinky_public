#pragma once
#ifdef DEBUG
// #define DEBUG_WU
#endif
#ifdef DEBUG_WU
#define WUDebugLog DebugLog
#else
#define WUDebugLog(...)
#endif

#ifdef EMU
static inline uint32_t tud_vendor_write_available(void) { return 0; }
static inline uint32_t tud_vendor_available(void) { return 0; }
static inline uint32_t tud_vendor_read(void *buffer, uint32_t bufsize) { return 0; }
static inline uint32_t tud_vendor_write(const void *buffer, uint32_t bufsize) { return 0; }
#else
uint32_t tud_vendor_n_write_available(uint8_t itf);
uint32_t tud_vendor_n_available(uint8_t itf);
uint32_t tud_vendor_n_read(uint8_t itf, void *buffer, uint32_t bufsize);
uint32_t tud_vendor_n_write(uint8_t itf, void const *buffer, uint32_t bufsize);
static inline uint32_t tud_vendor_write_available(void) { return tud_vendor_n_write_available(0); }
static inline uint32_t tud_vendor_available(void) { return tud_vendor_n_available(0); }
static inline uint32_t tud_vendor_read(void *buffer, uint32_t bufsize) { return tud_vendor_n_read(0, buffer, bufsize); }
static inline uint32_t tud_vendor_write(void const *buffer, uint32_t bufsize) { return tud_vendor_n_write(0, buffer, bufsize); }

#endif

extern const short wavetable[17][1031];

void flash_program_array(void *addr, void *srcbuf, int size_bytes) {
  FLASH_EraseInitTypeDef EraseInitStruct;
  int page = (((size_t)addr) & 0xffffff) / 2048;
  if (page < 32 || page >= 512) // protect bootloader!
    return;
  HAL_FLASH_Unlock();
  EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
  EraseInitStruct.Banks = (page >= 256) ? FLASH_BANK_2 : FLASH_BANK_1;
  EraseInitStruct.Page = /*page*/ page & 255;
  EraseInitStruct.NbPages = (size_bytes + 2047) / 2048;
  uint32_t SECTORError = 0;
  if (HAL_FLASHEx_Erase(&EraseInitStruct, &SECTORError) != HAL_OK) {
    HAL_FLASH_Lock();
    DebugLog("flash %d erase error %d\r\n", page, SECTORError);
    return;
  } else {
    DebugLog("flash %d erased ok!\r\n", page);
  }
  __HAL_FLASH_DATA_CACHE_DISABLE();
  __HAL_FLASH_INSTRUCTION_CACHE_DISABLE();
  __HAL_FLASH_DATA_CACHE_RESET();
  __HAL_FLASH_INSTRUCTION_CACHE_RESET();
  uint64_t *s = (uint64_t *)srcbuf;
  volatile uint64_t *d = (volatile uint64_t *)addr;
  for (; size_bytes > 0; size_bytes -= 8) {
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, (uint32_t)(size_t)(d++), *s++);
  }
  __HAL_FLASH_DATA_CACHE_DISABLE();
  __HAL_FLASH_INSTRUCTION_CACHE_DISABLE();
  __HAL_FLASH_DATA_CACHE_RESET();
  __HAL_FLASH_INSTRUCTION_CACHE_RESET();
  __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
  __HAL_FLASH_DATA_CACHE_ENABLE();
  HAL_FLASH_Lock();
}

/* webusb wire format. 10 byte header, then data.
u32 magic = 0xf30fabca
u8 cmd // 0 = get, 1=set
u8 idx // 0
u8 idx2 // 0
u8 idx3 // 0
u16 datalen // in bytes, not including this header, 0 for get, 1552 for set
*/
typedef struct WebUSBHeader { // 10 byte header
  u8 magic[4];
  u8 cmd, idx;
  union {
    struct {
      u16 offset_16;
      u16 len_16;
    };
    struct { // these are valid if magic3 is 0xcc
      u32 offset_32;
      u32 len_32;
    };
  };
} __attribute__((packed)) WebUSBHeader;
const static u8 wu_magic[4] = {0xf3, 0x0f, 0xab, 0xca};
const static u8 wu_magic_ext[4] = {0xf3, 0x0f, 0xab, 0xcb}; // 32 bit version
#define WEB_USB_TIMEOUT 500
enum { // state machine ticks thru these in order, more or less
  WU_MAGIC0,
  WU_MAGIC1,
  WU_MAGIC2,
  WU_MAGIC3,
  WU_RECVHDR,
  WU_RECVDATA,
  WU_SENDHDR,
  WU_SENDDATA,
};
u8 wu_state = WU_MAGIC0; // current state
WebUSBHeader wu_hdr;     // header of current command
int is_wu_hdr_32bit(void) { return wu_hdr.magic[3] == wu_magic_ext[3]; }
int wu_hdr_len(void) { return is_wu_hdr_32bit() ? wu_hdr.len_32 : wu_hdr.len_16; }
int wu_hdr_offset(void) { return is_wu_hdr_32bit() ? wu_hdr.offset_32 : wu_hdr.offset_16; }
u8 *wu_data = (u8 *)&wu_hdr; // buffer where we are reading/writing atm
int wu_len = 1;              // how much left to read/write before state transition
int wu_lasteventtime;        // for timeout detection
void SetWUState(u8 state, u8 *data, int len) {
  wu_state = state;
  wu_data = data;
  wu_len = len;
}

extern u8 erasepos;

void draw_webusb_ui(int o) {
  clear();
  fdrawstr(0, 0, F_20, "writing %d...",o);
  invertrectangle(0, 0, (erasepos * 2) & 127, 32);
  flip();
}
void draw_webusb_ui2() {
  draw_webusb_ui(0);
  tud_task(); // keep the host happy
}

extern bool web_serial_connected;
volatile bool want_webusb_on_audio_thread = true; // to get fast midi response we want to be on audio thread if possible

void PumpWebUSB(bool calling_from_audio_thread) {
  if (want_webusb_on_audio_thread != calling_from_audio_thread)
    return;
  if (!web_serial_connected) {
    SetWUState(WU_MAGIC0, wu_hdr.magic, 1);
    want_webusb_on_audio_thread = true;
    tud_task();
    return;
  }
  int usb_started = millis();
  int timeout = (calling_from_audio_thread) ? 1 : 250;
  while (millis() < usb_started + timeout) {
    if (calling_from_audio_thread && wu_state!=WU_MAGIC0) {
      want_webusb_on_audio_thread = false;
      return;
    }
    if (millis() > wu_lasteventtime + WEB_USB_TIMEOUT) { // timeout!
    reset:
      g_disable_fx = 0;
      spistate = 0;
      SetWUState(WU_MAGIC0, wu_hdr.magic, 1);
    }
    int n;
    static int totalwritten = 0;
    tud_task();
    if (wu_state < WU_SENDHDR) { // receive
                                 // n = tud_vendor_available();
                                 // if (n<6) break;

// CFG_TUD_VENDOR_RX_BUFSIZE should match the constant below
#define CFG_TUD_VENDOR_RX_BUFSIZE 128
      n = tud_vendor_read(wu_data, (wu_len > CFG_TUD_VENDOR_RX_BUFSIZE) ? CFG_TUD_VENDOR_RX_BUFSIZE : wu_len);
    } else { // send
      if (wu_len <= 0) {
        // odd corner case. the state doesnt want to send anything. just pretend the state is done
        goto statedone;
      }
      n = tud_vendor_write_available();
      if (n > wu_len)
        n = wu_len;
      if (n <= 0) {
        if (wu_len > 0 && n == 0 && !calling_from_audio_thread)
          continue; // used to be break! this makes us send as fast as we can I think.
        break;
      }
      n = tud_vendor_write(wu_data, n);
      if (n > 0)
        totalwritten += n;
    }
    if (n <= 0) {
    	if (calling_from_audio_thread) break; // nothing to do
      continue;
    }
#ifdef DEBUG_WU
    WUDebugLog("%d (%d): ", wu_state, wu_len);
    for (int i = 0; i < n; ++i)
      WUDebugLog("%02x ", wu_data[i]);
    WUDebugLog("\r\n");
#endif
    wu_lasteventtime = millis();
    wu_len -= n;
    wu_data += n;
    if (wu_len > 0) {
      if (calling_from_audio_thread)
        break;
      continue; // used to be break!
    }
    // ok! end of a state. whats next
  statedone:
    switch (wu_state) {
    case WU_MAGIC0:
    case WU_MAGIC1:
    case WU_MAGIC2:
    case WU_MAGIC3: {
      u8 m = wu_hdr.magic[wu_state];
      if (m != wu_magic[wu_state] && m != wu_magic_ext[wu_state]) {
        WUDebugLog("magic fail %02x %02x\r\n", m, wu_magic[wu_state]);
        // this resyncs to the incoming message!
        wu_hdr.magic[0] = m;
        wu_len = 1;
        wu_state = (m == wu_magic[0]) ? WU_MAGIC1 : WU_MAGIC0; // if we got the first byte, we can tick into next state!
        wu_data = wu_hdr.magic + wu_state;
        continue;
      }
      wu_state++;
      wu_len = 1;
      if (wu_state == WU_RECVHDR) { // time to get rest of header
        WUDebugLog("get header!\r\n");
        wu_len = 10 - 4;
        if (is_wu_hdr_32bit())
          wu_len = 14 - 4;
      }
      continue;
    }
    case WU_RECVHDR: { // ok we got the header. what do they want:
      WUDebugLog("got header %d %d!\r\n", wu_hdr.cmd, wu_hdr_len());
      if (wu_hdr.cmd >= 6)
        goto reset; // invalid cmd?
      int maxsize = (wu_hdr.idx >= 64) ? sizeof(SampleInfo) : sizeof(Preset);
      if (wu_hdr.cmd < 2) {
        if (wu_hdr.idx >= 64 + 8)
          wu_hdr.idx = ((ramsample1_idx - 1) & 7) + 64;
        else if (wu_hdr.idx >= 32)
          wu_hdr.idx = sysparams.curpreset;
      } else if (wu_hdr.cmd == 2 || wu_hdr.cmd == 3) {
        maxsize = 1024 * 1024 * 32;
      } else if (wu_hdr.cmd == 4 || wu_hdr.cmd == 5) {
        maxsize = sizeof(wavetable);
      }
      if (wu_hdr_len() + wu_hdr_offset() > maxsize || wu_hdr_len() < 0 || wu_hdr_offset() < 0)
        goto reset;
      if (wu_hdr.cmd == 4) {
        // read wavetable ram
        wu_hdr.cmd = 5;
        int ofs = wu_hdr_offset();
        int len = wu_hdr_len();
        wu_hdr.len_32 = len;
        wu_hdr.offset_32 = ofs;
        wu_hdr.magic[3] = wu_magic_ext[3]; // 32 bit mode
        SetWUState(WU_SENDHDR, (u8 *)&wu_hdr, is_wu_hdr_32bit() ? 14 : 10);
      } else if (wu_hdr.cmd == 2) {
        // read sample ram
        wu_hdr.cmd = 3;
        wu_hdr.idx &= 7;
        int ofs = wu_hdr_offset();
        int len = wu_hdr_len();
        wu_hdr.len_32 = len;
        wu_hdr.offset_32 = ofs;
        wu_hdr.magic[3] = wu_magic_ext[3]; // 32 bit mode
        SetWUState(WU_SENDHDR, (u8 *)&wu_hdr, is_wu_hdr_32bit() ? 14 : 10);
      } else if (wu_hdr.cmd == 3) {
        // write sample spi ram
        g_disable_fx = 1;
        while (g_disable_fx<2);
        int ofs = wu_hdr_offset();
        int len = wu_hdr_len();
        wu_hdr.len_32 = len;
        wu_hdr.offset_32 = ofs;
        wu_hdr.magic[3] = wu_magic_ext[3]; // 32 bit mode
        if (len >= 65536)
          len = 65536;
        SetWUState(WU_RECVDATA, ((u8 *)delaybuf), len);
      } else if (wu_hdr.cmd == 5) {
        // write wavetable ram
        g_disable_fx = 1;
        while (g_disable_fx<2);
        memcpy(delaybuf, wavetable, sizeof(wavetable));
        SetWUState(WU_RECVDATA, ((u8 *)delaybuf) + wu_hdr_offset() + wu_hdr.idx * WAVETABLE_SIZE * 2, wu_hdr_len());
      } else if (wu_hdr.cmd == 0) {
        wu_hdr.cmd = 1;
        if (wu_hdr_len() == 0) {
          int ofs = wu_hdr_offset();
          wu_hdr.len_16 = maxsize - ofs;
          wu_hdr.offset_16 = ofs;
          wu_hdr.magic[3] = wu_magic[3]; // 16 bit mode
        }
        SetWUState(WU_SENDHDR, (u8 *)&wu_hdr, is_wu_hdr_32bit() ? 14 : 10);
      } else if (wu_hdr.cmd == 1) {
        // they asked to set!
        if (wu_hdr.idx >= 64 && wu_hdr.idx < 64 + 8) {
          cur_sample1 = wu_hdr.idx - 64 + 1;
          CopySampleToRam(false);
          ramsample1_idx = cur_sample1;
          SetWUState(WU_RECVDATA, ((u8 *)&ramsample) + wu_hdr_offset(), wu_hdr_len());
        } else {
          SetPreset(wu_hdr.idx, false);
          SetWUState(WU_RECVDATA, ((u8 *)&rampreset) + wu_hdr_offset(), wu_hdr_len());
        }
      } else
        goto reset;
      continue;
    }
    case WU_RECVDATA:
      if (wu_hdr.cmd == 1) {
        // finished receiving preset. mark it dirty
        if (wu_hdr.idx >= 64) {
          ramtime[GEN_SAMPLE] = millis();
        } else
          ramtime[GEN_PRESET] = millis();
      } else if (wu_hdr.cmd == 3) {
        // write to spi ram.
        while (spistate != 0 && spistate != 255)
          ;
        spistate = 255;

        int ofs = wu_hdr_offset();
        int idx = wu_hdr.idx;
        int len = wu_hdr_len();
        if (len >= 65536)
          len = 65536;
        // program spi!
        int addr = ofs + idx * 4 * 1024 * 1024;
        erasepos = (addr / 65536) & 63; // for oled ui
        spi_erase64k(addr, draw_webusb_ui2);
        for (int o = 0; o < len; o += 256) {
          tud_task(); // keep the host happy
          s16 *src = (s16 *)(delaybuf) + o/2;
          s16 *dst = (s16 *)(spibigtx + 4);
          // int peak = 0;
          // for (int i = 0; i < 256 / 2; ++i) {
          //   s16 smp = *src++;
          //   *dst++ = smp;
          //   peak = maxi(peak, abs(smp));
          // }
          //  setwaveform4(s, ((addr+o) / 2 / 1024), peak / 1024);
          memcpy(dst, src, 256);
          if (spi_write256(addr + o) != 0) {
            DebugLog("flash write fail\n");
            ShowMessage(F_20_BOLD, "write fail", "");
            goto reset;
          }
          if (!(o&8191)) {
        	  draw_webusb_ui(o);
          }
        }
        wu_hdr.offset_32 += len;
        wu_hdr.len_32 -= len;
        if (wu_hdr.len_32 > 0) {
          // more to do
          len = wu_hdr.len_32;
          if (len >= 65536)
            len = 65536;
          usb_started = millis();
          wu_lasteventtime = millis();
          SetWUState(WU_RECVDATA, ((u8 *)delaybuf), len);
          continue;
        } else {
        	// finished writing spi ram!
          spistate = 0;
          g_disable_fx = 0;
        }
      } else if (wu_hdr.cmd == 5) {
        // write to wavetable ram
        flash_program_array((void *)wavetable, (void *)delaybuf, (sizeof(wavetable) + 7) & ~7);
        memset(delaybuf, 0, sizeof(delaybuf));
        g_disable_fx = 0;
      }
      goto reset;
    case WU_SENDHDR: // ok we sent them the header; now send them the data
    send_more_data: {
      int len = wu_hdr_len();
      int ofs = wu_hdr_offset();
      int idx = wu_hdr.idx;
      u8 *data;
      if (wu_hdr.cmd == 5) { // send wavetable data
        SetWUState(WU_SENDDATA, ((u8 *)&wavetable) + ofs + idx * WAVETABLE_SIZE * 2, len);
      } else if (wu_hdr.cmd == 3) { // send spi data
        if (len > 256)
          len = 256;
        wu_hdr.len_32 -= len;
        wu_hdr.offset_32 += len;
        while (spistate != 0 && spistate != 255)
          ;
        spistate = 255;
        // enable_audio = EA_OFF;
        memset(spibigrx, -2, sizeof(spibigrx));
        spi_read256(ofs + idx * 4 * 1024 * 1024);
        SetWUState(WU_SENDDATA, spibigrx + 4, len);
      } else if (wu_hdr.cmd == 1) {
        if (wu_hdr.idx == ((ramsample1_idx - 1) & 7) + 64)
          data = (u8 *)&ramsample;
        else if (wu_hdr.idx >= 64 && wu_hdr.idx < 64 + 8)
          data = (u8 *)GetSavedSampleInfo(wu_hdr.idx - 64);
        else
          data = (wu_hdr.idx == sysparams.curpreset) ? (u8 *)&rampreset : (u8 *)GetSavedPreset(wu_hdr.idx);
        SetWUState(WU_SENDDATA, data + wu_hdr_offset(), wu_hdr_len());
      }
      continue;
    }
    case WU_SENDDATA: // ok we finished sending our data. nothing to do except get ready for more.
      if (wu_hdr.cmd == 3) {
        if (wu_hdr.len_32 <= 0) {
          spistate = 0;
          // enable_audio = EA_PLAY;
        } else {
          goto send_more_data;
        }
      }
      goto reset;
    } // state
  } // while loop
  if (!calling_from_audio_thread && wu_state == WU_MAGIC0)
    want_webusb_on_audio_thread = true;
}
