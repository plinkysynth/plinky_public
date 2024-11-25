
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "tusb.h"
#include "main.h"


enum
{
    VENDOR_REQUEST_WEBUSB = 1,
    VENDOR_REQUEST_MICROSOFT = 2
};

extern uint8_t const desc_ms_os_20[];

extern bool web_serial_connected;
bool web_serial_connected = false;


#define URL  "www.plinkysynth.com/webusb"

const tusb_desc_webusb_url_t desc_url =
{
  .bLength         = 3 + sizeof(URL) - 1,
  .bDescriptorType = 3, // WEBUSB URL type
  .bScheme         = 1, // 0: http, 1: https
  .url             = URL
};



void OTG_FS_IRQHandler(void)
{
  tud_int_handler(0);
}

//int HAL_GetTick(void);
#define millis HAL_GetTick
typedef unsigned char u8;
extern u8 led_ram[9][8];
void board_led_write(bool state) {
//	led_ram[0][0]=state?255:0;
	if (state)
		GPIOD->BSRR=1;
	else
		GPIOD->BRR=1;
}

/* This MIDI example send sequence of note (on/off) repeatedly. To test on PC, you need to install
 * synth software and midi connection management software. On
 * - Linux (Ubuntu): install qsynth, qjackctl. Then connect TinyUSB output port to FLUID Synth input port
 * - Windows: install MIDI-OX
 * - MacOS: SimpleSynth
 */

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;
void led_blinking_task(void);
//void midi_task(void);

extern TIM_HandleTypeDef htim1;

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
uint32_t serialno;

/*------------- MAIN -------------*/
void midiinit(void)
{
#if 0

	// based on https://github.com/hathach/tinyusb/blob/fix-stm32l4/hw/bsp/stm32l476disco/stm32l476disco.c
	 /* Enable Power Clock*/
	  __HAL_RCC_PWR_CLK_ENABLE();

	  /* Enable USB power on Pwrctrl CR2 register */
	  HAL_PWREx_EnableVddUSB();

	  GPIO_InitTypeDef  GPIO_InitStruct;
	 // USB
	  /* Configure DM DP Pins */
	  GPIO_InitStruct.Pin = (GPIO_PIN_11 | GPIO_PIN_12);
	  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	  GPIO_InitStruct.Pull = GPIO_NOPULL;
	  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	  GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
	  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	  /* Configure VBUS Pin */
	  GPIO_InitStruct.Pin = GPIO_PIN_11;
	  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	  GPIO_InitStruct.Pull = GPIO_NOPULL;
	  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	  /* Enable USB FS Clock */
	  __HAL_RCC_USB_OTG_FS_CLK_ENABLE();

	  // L476Disco use general GPIO PC11 for VBUS sensing instead of dedicated PA9 as others
	  // Disable VBUS Sense and force device mode
	  USB_OTG_FS->GCCFG &= ~USB_OTG_GCCFG_VBDEN;

	  USB_OTG_FS->GUSBCFG &= ~USB_OTG_GUSBCFG_FHMOD;
	  USB_OTG_FS->GUSBCFG |= USB_OTG_GUSBCFG_FDMOD;

	  USB_OTG_FS->GOTGCTL |= USB_OTG_GOTGCTL_BVALOEN;
	  USB_OTG_FS->GOTGCTL |= USB_OTG_GOTGCTL_BVALOVAL;
#endif

	uint32_t 	uid0=HAL_GetUIDw0 ();
	uint32_t 	uid1=HAL_GetUIDw1 ();
	uint32_t 	uid2=HAL_GetUIDw2 ();
	serialno = mix(uid0,uid1,uid2);

  tusb_init();
}


bool midi_receive(unsigned char packet[4]) {
	  return tud_midi_available() && tud_midi_packet_read(packet);
}
bool usb_midi_write(const uint8_t packet[4]) {
    return tud_midi_packet_write(packet);
}

/*
int miditest(void) {
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
  GPIOD->MODER|=1;

  while (1)
  {
    tud_task(); // tinyusb device task
    led_blinking_task();
    midi_task();
  }
  return 0;
}*/


//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
  web_serial_connected = true;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
  web_serial_connected = false;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
  web_serial_connected = false;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
  web_serial_connected = true;
}

//--------------------------------------------------------------------+
// WebUSB use vendor class
//--------------------------------------------------------------------+

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request)
{
    // nothing to with DATA & ACK stage
    if (stage != CONTROL_STAGE_SETUP) return true;

    switch (request->bRequest)
    {
    case VENDOR_REQUEST_WEBUSB:
        // match vendor request in BOS descriptor
        // Get landing page url
        return tud_control_xfer(rhport, request, (void*)&desc_url, desc_url.bLength);

    case VENDOR_REQUEST_MICROSOFT:
        if (request->wIndex == 7)
        {
            // Get Microsoft OS 2.0 compatible descriptor
            uint16_t total_len;
            memcpy(&total_len, desc_ms_os_20 + 8, 2);

            return tud_control_xfer(rhport, request, (void*)desc_ms_os_20, total_len);
        }
        else
        {
            return false;
        }

    case 0x22:
        // Webserial simulate the CDC_REQUEST_SET_CONTROL_LINE_STATE (0x22) to
        // connect and disconnect.
        web_serial_connected = (request->wValue != 0);

        // Always lit LED if connected
        if (web_serial_connected)
        {
            board_led_write(true);
//            blink_interval_ms = BLINK_ALWAYS_ON;

            //tud_vendor_write_str("\r\nTinyUSB WebUSB device example\r\n");
        }
        else
        {
            blink_interval_ms = BLINK_MOUNTED;
        }

        // response with status OK
        return tud_control_status(rhport, request);

    default:
        // stall unknown request
        return false;
    }

    return true;
}

void PumpWebUSB(bool calling_from_audio_thread);


/*
//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+
void cdc_task(void)
{
    if (tud_cdc_connected())
    {
        // connected and there are data available
        if (tud_cdc_available())
        {
            uint8_t buf[64];

            uint32_t count = tud_cdc_read(buf, sizeof(buf));

            // echo back to both web serial and cdc
            echo_all(buf, count);
        }
    }
}

// Invoked when cdc when line state changed e.g connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    (void)itf;

    // connected
    if (dtr && rts)
    {
        // print initial message when connected
        tud_cdc_write_str("\r\nTinyUSB WebUSB device example\r\n");
    }
}

// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf)
{
    (void)itf;
}
*/
/*
//--------------------------------------------------------------------+
// MIDI Task
//--------------------------------------------------------------------+

void DebugLog(const char *fmt, ...);

void midi_task(void)
{
  static uint32_t start_ms = 0;

  if (tud_midi_available()) {
	  unsigned char packet[4]={};
	  if (tud_midi_receive(packet)) {
		//  DebugLog("%02x %02x %02x %02x\r\n", packet[0],packet[1],packet[2],packet[3]);
	  }
  }

  // send note every 1000 ms
  if (millis() - start_ms < 286) return; // not enough time
  start_ms += 286;

  // Previous positions in the note sequence.
  int previous = note_pos - 1;

  // If we currently are at position 0, set the
  // previous position to the last note in the sequence.
  if (previous < 0) previous = sizeof(note_sequence) - 1;

  // Send Note On for current position at full velocity (127) on channel 1.
  tudi_midi_write24(0, 0x90, note_sequence[note_pos], 127);

  // Send Note Off for previous note.
  tudi_midi_write24(0, 0x80, note_sequence[previous], 0);

  // Increment position
  note_pos++;

  // If we are at the end of the sequence, start over.
  if (note_pos >= sizeof(note_sequence)) note_pos = 0;
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // Blink every interval ms
  if ( millis() - start_ms < blink_interval_ms) return; // not enough time
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}
*/
