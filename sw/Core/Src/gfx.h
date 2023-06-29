#pragma once
typedef enum  {
    BOLD=16,

    F_8=0,F_12,F_16,F_20,F_24,F_28,F_32,
    F_8_BOLD=BOLD,F_12_BOLD,F_16_BOLD,F_20_BOLD,F_24_BOLD,F_28_BOLD,F_32_BOLD,

} EFont;
int strwidth(EFont f, const char *buf);

void invertrectangle(int x1, int y1, int x2, int y2);
void fillrectangle(int x1, int y1, int x2, int y2);

extern u8 textcol; // 0 =black, 1=white, 2=upper shadow, 3=lower shadow
int drawstr(int x, int y, EFont f, const char *buf);
int fdrawstr(int x, int y, EFont f, const char *fmt, ...);
int drawchar(int x, int y, EFont f, char c, char textcol);
void vline(int x1, int y1, int y2, int c);
void hline(int x1, int y1, int x2, int c);
void putpixel(int x, int y, int c);
void clear();

#define W 128
#define H 32

extern u8 vrambuf[H / 8 * W + 1]; // first byte is 0x40

static inline u8* getvram(void) { return vrambuf + 1; }

///////////////////////////////////////////////

#ifdef IMPL
#include "fontdata.h"

u8 vrambuf[H / 8 * W + 1];


void clear(void) {
	vrambuf[0] = 0x40;
	memset(getvram(),0,W*H/8);
}

void putpixel(int x, int y, int c) {
	if (x >= W)
		return;
	if (y >= H)
		return;
	u8 *dst = getvram() + x + ((y >> 3) << 7);
	u8 b = 1 << (y & 7);
	if (c)
		*dst |= b;
	else
		*dst &= ~b;
}

void vline(int x1, int y1, int y2, int c) {
	if (x1 < 0 || x1 >= W)
		return;
	y1 = clampi(y1, 0, H);
	y2 = clampi(y2, 0, H);
	if (y1 >= y2)
		return;
	int y1b = y1 >> 3;
	int n = (y2 >> 3) - y1b;
	u8 *dst = getvram() + (x1) + (y1b << 7);
	u8 b1 = 0xff << (y1 & 7), b2 = ~(0xff << (y2 & 7));
	if (c) {
		u8 mask = (c == 1) ? 255 : (x1 & 1) ? 0x55 : 0xaa;
		if (n == 0) {
			*dst |= b1 & b2 & mask;
		} else {
			*dst |= b1 & mask;
			dst += W;
			for (; --n; dst += W)
				*dst |= mask;
			*dst |= b2 & mask;
		}
	} else {
		if (n == 0) {
			*dst &= ~(b1 & b2);
		} else {
			*dst &= ~b1;
			dst += W;
			for (; --n; dst += W)
				*dst = 0;
			*dst &= ~b2;
		}
	}
}

void hline(int x1, int y1, int x2, int c) {
	if (y1 < 0 || y1 >= H)
		return;
	x1 = clampi(x1, 0, W);
	x2 = clampi(x2, 0, W);
	int n = (x2 - x1);
	if (n <= 0)
		return;
	u8 *dst = getvram() + ((y1 >> 3) << 7) + (x1);
	u8 b = (1 << (y1 & 7));
	if (c) {
		for (; n--; dst++)
			*dst |= b;
	} else {
		b = ~b;
		for (; n--; dst++)
			*dst &= b;
	}
}


int charwidth(EFont f, char c) {
	int xsize = ((int) (f) & 15) * 2 + 4;
	int ysize = xsize * 2;
	if (c & 0x80)
		return 20;
	if (c <= 32 || c > '~')
		return xsize / 2; // space
	int fo = fontoffsets[f & 15][(f&BOLD) >= BOLD];
	const u8 *data = (((const u8*) fontdata) + fo);
	u8 datap = (ysize + 7) / 8;
	u32 mask = (2 << (ysize - 1)) - 1;
	data += datap * xsize * (c - 32);
	while (xsize > 0 && ((*(u32*) data) & mask) == 0) {
		data += datap;
		--xsize;
	} // skip blank at start?
	int lastset = 0;
	for (int xx = 0; xx < xsize; ++xx) {
		u32 d = ((*((const u32*) data)) & mask);
		data += datap;
		if (!d)
			continue;
		lastset = xx;
	}
	return lastset + 2;
}

void invertrectangle(int x1, int y1, int x2, int y2) {
	x1=clampi(0,x1,128);
	y1=clampi(0,y1,32);
	x2=clampi(0,x2,128);
	y2=clampi(0,y2,32);
	if (y1>=y2 || x1>=x2) return ;
	u32 mask=(2<<(y2-y1-1))-1;
	u8 *dst=getvram() + x1 + (y1/8)*128;
	mask<<=y1&7;
	int w=x2-x1;
	while (mask) {
		u8 bmask;
		bmask=mask; for (int i=0;i<w;++i) dst[i]^=bmask;
		mask>>=8;
		dst+=128;
	}
}

void fillrectangle(int x1, int y1, int x2, int y2) {
	x1 = clampi(0, x1, 128);
	y1 = clampi(0, y1, 32);
	x2 = clampi(0, x2, 128);
	y2 = clampi(0, y2, 32);
	if (y1 >= y2 || x1 >= x2) return;
	u32 mask = (2 << (y2 - y1 - 1)) - 1;
	u8* dst = getvram() + x1 + (y1 / 8) * 128;
	mask <<= y1 & 7;
	int w = x2 - x1;
	while (mask) {
		u8 bmask;
		bmask = mask; for (int i = 0; i < w; ++i) dst[i] |= bmask;
		mask >>= 8;
		dst += 128;
	}
}

void halfrectangle(int x1, int y1, int x2, int y2) {
	x1 = clampi(0, x1, 128);
	y1 = clampi(0, y1, 32);
	x2 = clampi(0, x2, 128);
	y2 = clampi(0, y2, 32);
	if (y1 >= y2 || x1 >= x2) return;
	u32 mask = (2 << (y2 - y1 - 1)) - 1;
	u8* dst = getvram() + x1 + (y1 / 8) * 128;
	mask <<= y1 & 7;
	int w = x2 - x1;
	while (mask) {
		u8 bmask;
		u8 dither = (x1 & 1) ? 0x55 : 0xaa;
		bmask = mask; for (int i = 0; i < w; ++i, dither^=255) dst[i] |= (bmask & dither);
		mask >>= 8;
		dst += 128;
	}
}

#include "icons.h"

int drawicon(int x, int y, unsigned char c, int textcol) {
	if (x<=-16 || x>=128 || y<=-16 || y>=32 || c>=numicons)
		return 20;
	const u16 *data =icons[c];
	u8 *dst = getvram() + x;
	for (int xx = 0; xx < 15; ++xx, ++dst, ++x) {
		if (x >= W)
			break;
		u32 d = *data++;
		if (!d || x<0)
			continue;
		if (y < 0)
			d >>= -y;
		else
			d <<= y;
		if (!d)
			continue;
		if (textcol == 0) {
			if (d & 255) dst[0] &= ~d;
			if (!(d >>= 8)) continue;
			if (d & 255) dst[W] &= ~d;
			if (!(d >>= 8)) continue;
			if (d & 255) dst[W * 2] &= ~d;
			if ((d >>= 8) & 255) dst[W * 3] &= ~d;
		} 
		else {
			if (d & 255) dst[0] |= d;
			if (!(d >>= 8)) continue;
			if (d & 255) dst[W] |= d;
			if (!(d >>= 8)) continue;
			if (d & 255) dst[W * 2] |= d;
			if ((d >>= 8) & 255) dst[W * 3] |= d;
		}
	}
	return 20;
}


int drawchar(int x, int y, EFont f, char c, char textcol) {
	if (textcol == 2) {
		drawchar(x-1, y - 1, f, c, 0);
		textcol = 1;
	}
	else if (textcol == 3) {
		drawchar(x-1, y + 1, f, c, 0);
		textcol = 1;
	}
	int xsize = ((int) (f) & 15) * 2 + 4;
	int ysize = xsize * 2;
	if (c&0x80)
		return drawicon(x,mini(y,18),c^0x80, textcol);
	if (c <= 32 || c > '~')
		return xsize / 2; // space
	int fo = fontoffsets[f & 15][(f&BOLD) >= BOLD];
	const u8 *data = (((const u8*) fontdata) + fo);
	u8 datap = (ysize + 7) / 8;
	u32 mask = (2 << (ysize - 1)) - 1;
	data += datap * xsize * (c - 32);
	while (xsize > 0 && ((*(u32*) data) & mask) == 0) {
		data += datap;
		--xsize;
	} // skip blank at start?
	int lastset = 0;
//	u8 *vram=getvram();
	u8 *dst = getvram() + x;
	for (int xx = 0; xx < xsize; ++xx, ++dst, ++x) {
		if (x >= W)
			break;
		u32 d = ((*((const u32*) data)) & mask);
		data += datap;
		if (!d)
			continue;
		lastset = xx;
		if (x < 0)
			continue;
		if (y < 0)
			d >>= -y;
		else
			d <<= y;
		if (!d)
			continue;
		if (textcol == 0) {
			if (d & 255) dst[0] &= ~d;
			if (!(d >>= 8)) continue;
			if (d & 255) dst[W] &= ~d;
			if (!(d >>= 8)) continue;
			if (d & 255) dst[W * 2] &= ~d;
			d >>= 8;
			if (d & 255) dst[W * 3] &= ~d;
		}
		else {
			if (d & 255) dst[0] |= d;
			if (!(d >>= 8)) continue;
			if (d & 255) dst[W] |= d;
			if (!(d >>= 8)) continue;
			if (d & 255) dst[W * 2] |= d;
			d >>= 8;
			if (d & 255) dst[W * 3] |= d;
		}
	}
	return lastset + 2;
}

u8 textcol = 1;


int drawstr_noright(int x, int y, EFont f, const char* buf) {
	if (!buf)
		return 0;

	int ox = x;
	int ysize = ((int) (f) & 15) * 2 + 4;
	if (x>=128 || y<=-ysize || y>=32)
		return 0;
	for (; *buf;) {
		if (*buf == '\n') {
			x = ox;
			y += ysize*2-2;
		} else
			x += drawchar(x, y, f, *buf, textcol);
		buf++;
		if (x>W+1)
			return x;
	}
	return x;
}

int drawstr(int x, int y, EFont f, const char* buf) {
	if (x < 0) {
		// right align! hohoho
		int w = strwidth(f, buf);
		x = -x - w;
	}
	return drawstr_noright(x, y, f, buf);
}

int fontheight(EFont f) {
	return ((int)(f) & 15) * 4 + 8;
}

int strheight(EFont f, const char *buf) {
	int lines = 1;
	for (; *buf;++buf) if (*buf == '\n') lines++;
	return lines * fontheight(f);
}

int strwidth(EFont f, const char *buf) {
	if (!buf) return 0;
	int w=0;
	int mw=0;
	for (; *buf;) {
		if (*buf & 0x80)
			w += 20;
		else if (*buf == '\n') {
			mw=maxi(mw,w);
			w=0;
		} else
			w += charwidth(f, *buf);
		buf++;
	}
	return maxi(mw,w);
}


int fdrawstr(int x, int y, EFont f, const char *fmt, ...) {
	if (!fmt)
		return 0;
	va_list args;
	va_start(args, fmt);
	char buf[64];
	vsnprintf(buf, 63, fmt, args);
	va_end(args);
	return drawstr(x, y, f, buf);
}

#endif
