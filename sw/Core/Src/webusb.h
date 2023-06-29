#pragma once
#ifdef DEBUG
#define DEBUG_WU
#endif
#ifdef DEBUG_WU
#define WUDebugLog DebugLog
#else
#define WUDebugLog(...)
#endif

#ifdef EMU
static inline uint32_t tud_vendor_write_available(void) { return 0; }
static inline uint32_t tud_vendor_available(void) { return 0; }
static inline uint32_t tud_vendor_read(void* buffer, uint32_t bufsize) { return 0; }
static inline uint32_t tud_vendor_write(const void* buffer, uint32_t bufsize) { return 0; }
#else
uint32_t tud_vendor_n_write_available (uint8_t itf);
uint32_t tud_vendor_n_available (uint8_t itf);
uint32_t tud_vendor_n_read (uint8_t itf, void* buffer, uint32_t bufsize);
uint32_t tud_vendor_n_write (uint8_t itf, void const* buffer, uint32_t bufsize);
static inline uint32_t tud_vendor_write_available (void)
{
  return tud_vendor_n_write_available(0);
}
static inline uint32_t tud_vendor_available (void)
{
  return tud_vendor_n_available(0);
}
static inline uint32_t tud_vendor_read (void* buffer, uint32_t bufsize)
{
  return tud_vendor_n_read(0, buffer, bufsize);
}
static inline uint32_t tud_vendor_write (void const* buffer, uint32_t bufsize)
{
  return tud_vendor_n_write(0, buffer, bufsize);
}

#endif
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
	u8 cmd,idx;
	u16 offset;
	u16 len;
} WebUSBHeader;
const static u8 wu_magic[4] = { 0xf3,0x0f,0xab,0xca }; // we expect magic to be these
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
WebUSBHeader wu_hdr; // header of current command
u8* wu_data=(u8*)&wu_hdr; // buffer where we are reading/writing atm
int wu_len=1; // how much left to read/write before state transition
int wu_lasteventtime; // for timeout detection
void SetWUState(u8 state, u8* data, int len) { wu_state = state; wu_data = data; wu_len = len;  }
void PumpWebUSB() {
	while (1) {
		if (millis() > wu_lasteventtime + WEB_USB_TIMEOUT) { // timeout!
		reset:		
			SetWUState(WU_MAGIC0, wu_hdr.magic, 1);
		}
		int n;
		if (wu_state < WU_SENDHDR) { // receive
			//n = tud_vendor_available();
			//if (n<6) break;
			n = tud_vendor_read(wu_data, wu_len);
		}
		else { // send
			if (wu_len <= 0) {
				// odd corner case. the state doesnt want to send anything. just pretend the state is done
				goto statedone;
			}
			n = tud_vendor_write_available();
			if (n > wu_len) n = wu_len;
			if (n <= 0) break;
			n = tud_vendor_write(wu_data, n);
		}
		if (n <= 0) break; // nothing to do
#ifdef DEBUG_WU
		WUDebugLog("%d (%d): ", wu_state,wu_len);
		for (int i=0;i<n;++i) WUDebugLog("%02x ", wu_data[i]);
		WUDebugLog("\r\n");
#endif
		wu_lasteventtime = millis();
		wu_len -= n;
		wu_data += n;
		if (wu_len > 0) break; // not done with this state yet. next time!
		// ok! end of a state. whats next
statedone:
		switch (wu_state) {
		case WU_MAGIC0: case WU_MAGIC1: case WU_MAGIC2: case WU_MAGIC3: {
			u8 m = wu_hdr.magic[wu_state];
			if (m != wu_magic[wu_state]) {
				WUDebugLog("magic fail %02x %02x\r\n",m, wu_magic[wu_state]);
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
				wu_len = 10-4;
			}
			continue; }
		case WU_RECVHDR: // ok we got the header. what do they want:
			WUDebugLog("got header %d %d!\r\n", wu_hdr.cmd,wu_hdr.len);
			if (wu_hdr.cmd > 1) goto reset; // invalid cmd?
			if (wu_hdr.cmd == 0) {
				// they asked to get a preset!
				if (wu_hdr.len + wu_hdr.offset > sizeof(Preset) ) goto reset;
				if (wu_hdr.offset >= sizeof(Preset)) goto reset;
				if (wu_hdr.idx >= 32) wu_hdr.idx = sysparams.curpreset;
				wu_hdr.cmd = 1;
				if (wu_hdr.len == 0) 
					wu_hdr.len = sizeof(Preset) - wu_hdr.offset;
				SetWUState(WU_SENDHDR, (u8*)&wu_hdr, 10);
			}
			else {
				// they asked to set!
				if (wu_hdr.len + wu_hdr.offset > sizeof(Preset) || wu_hdr.len<=0) goto reset;
				if (wu_hdr.offset >= sizeof(Preset)) goto reset;
				if (wu_hdr.idx >= 32) wu_hdr.idx = sysparams.curpreset; // idx>32, just use current
				// switch to the specified preset ready to receive.
				SetPreset(wu_hdr.idx, false);
				SetWUState(WU_RECVDATA, ((u8*)&rampreset) + wu_hdr.offset, wu_hdr.len);
			}
			continue;
		case WU_RECVDATA:
			if (wu_hdr.cmd == 1) {
				// finished receiving preset. mark it dirty 
				ramtime[GEN_PRESET] = millis();
			}
			goto reset;
		case WU_SENDHDR:// ok we sent them the header; now send them the data
		{
			u8* data = (wu_hdr.idx == sysparams.curpreset) ? (u8 * )&rampreset : (u8*)GetSavedPreset(wu_hdr.idx);
			SetWUState(WU_SENDDATA, data + wu_hdr.offset, wu_hdr.len);
			continue;
		}
		case WU_SENDDATA: // ok we finished sending our data. nothing to do except get ready for more.
			goto reset;
		} // state
	} // while loop
}
