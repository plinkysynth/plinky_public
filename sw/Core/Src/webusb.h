#pragma once
#ifdef DEBUG
//#define DEBUG_WU
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
void PumpWebUSB() {
  bool need_tud_task = false;
  while (1) {
    need_tud_task = true;
    if (millis() > wu_lasteventtime + WEB_USB_TIMEOUT) { // timeout!
    reset:
      SetWUState(WU_MAGIC0, wu_hdr.magic, 1);
    }
    int n;
    if (wu_state < WU_SENDHDR) { // receive
      // n = tud_vendor_available();
      // if (n<6) break;
	if (need_tud_task)
      tud_task();

      n = tud_vendor_read(wu_data, wu_len);
    } else { // send
      if (wu_len <= 0) {
        // odd corner case. the state doesnt want to send anything. just pretend the state is done
        goto statedone;
      }
      n = tud_vendor_write_available();
      if (n > wu_len)
        n = wu_len;
      if (n <= 0)
        break;
      n = tud_vendor_write(wu_data, n);
    }
    if (n <= 0)
      break; // nothing to do
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
      if (wu_hdr.cmd<2) {
		if (wu_hdr.idx >= 64 + 8)
			wu_hdr.idx = ((ramsample1_idx - 1) & 7) + 64;
		else if (wu_hdr.idx >= 32)
			wu_hdr.idx = sysparams.curpreset;
	  } else if (wu_hdr.cmd ==2 || wu_hdr.cmd == 3) {
		maxsize= 1024*1024*32;
	  } else if (wu_hdr.cmd ==4 || wu_hdr.cmd == 5) {
		maxsize=sizeof(wavetable);
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
        // TODO: write to spi ram
      } else if (wu_hdr.cmd == 5) {
        // TODO: write to wavetable ram
      }
      goto reset;
    case WU_SENDHDR: // ok we sent them the header; now send them the data
    send_more_data: {
      int len = wu_hdr_len();
      int ofs = wu_hdr_offset();

      u8 *data;
      if (wu_hdr.cmd == 5) { // send wavetable data
        SetWUState(WU_SENDDATA, ((u8 *)&wavetable) + ofs, len);
      } else if (wu_hdr.cmd == 3) { // send spi data
        if (len > 256)
          len = 256;
        wu_hdr.len_32 -= len;
        wu_hdr.offset_32 += len;
        while (spistate)
          ;
        spistate = 255;
        memset(spibigrx, -2, sizeof(spibigrx));
        spi_read256(ofs);
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
        } else {
          goto send_more_data;
        }
      }
      goto reset;
    } // state
  } // while loop
}
