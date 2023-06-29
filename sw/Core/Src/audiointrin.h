#define RVMASK 16383
#define DLMASK 32767


#ifndef EMU
#define __STATIC_FORCEINLINE                   __attribute__((always_inline)) static inline
#define CORTEX
#else
#define __STATIC_FORCEINLINE                   static inline
#endif

__STATIC_FORCEINLINE
s16 SATURATE16(s32 a) {
	//	int __ssat(int val, unsigned int sat)
#ifdef CORTEX
	int tmp;
	asm ("ssat %0, %1, %2" : "=r" (tmp) : "I" (16), "r" (a) );
	return tmp;
#else
	if (a<-32768) a=-32768;
	else if (a>32767) a=32767;
	return a;
#endif
}

__STATIC_FORCEINLINE
s32 SATURATE17(s32 a) {
#ifdef CORTEX
	int tmp;
	asm ("ssat %0, %1, %2" : "=r" (tmp) : "I" (17), "r" (a) );
	return tmp;
#else
	if (a<-65536) a=-65536;
	else if (a>65535) a=65535;
	return a;
#endif
}


__STATIC_FORCEINLINE
u16 SATURATEU16(s32 a) {
	//	int __ssat(int val, unsigned int sat)
#ifdef CORTEX
	int tmp;
	asm ("usat %0, %1, %2" : "=r" (tmp) : "I" (16), "r" (a) );
	return tmp;
#else
	if (a<0) a=0;
	else if (a>65535) a=65535;
	return a;
#endif
}


#define FLOAT2FIXED(x,bits) ((int)((x)*(1<<(bits))))
#define STEREOUNPACK(lr) int lr##l=(s16)lr, lr##r=(s16)(lr>>16);

__STATIC_FORCEINLINE
u32 STEREOPACK(s16 l, s16 r) {
#ifdef CORTEX
	int32_t out;
	asm ("pkhbt %0, %1, %2, lsl #16" : "=r" (out) : "r" (l), "r" (r));
	return out;
#else
	return ((u16)l)+(((u16)r)<<16);
#endif
}

__STATIC_FORCEINLINE
u32 STEREOADDAVERAGE(u32 a, u32 b) {
#ifdef CORTEX
	int32_t out;
	asm ("shadd16 %0, %1, %2" : "=r" (out) : "r" (a), "r" (b));
	return out;
#else
	STEREOUNPACK(a);
	STEREOUNPACK(b);
	return STEREOPACK((al+bl)>>1,(ar+br)>>1);
#endif
}

__STATIC_FORCEINLINE
u32 STEREOADDSAT(u32 a, u32 b) {
#ifdef CORTEX
	int32_t out;
	asm ("qadd16 %0, %1, %2" : "=r" (out) : "r" (a), "r" (b));
	return out;
#else
	STEREOUNPACK(a);
	STEREOUNPACK(b);
	return STEREOPACK(SATURATE16(al+bl),SATURATE16(ar+br));
#endif
}

__STATIC_FORCEINLINE
u32 STEREOSCALE(u32 in, int scale) {
	STEREOUNPACK(in);
	return STEREOPACK((inl*scale)>>16,(inr*scale)>>16);
}

__STATIC_FORCEINLINE
u32 MIDSIDESCALE(u32 in, int midscale, int sidescale) {
	STEREOUNPACK(in);
	int mid = inl + inr;
	int side = inl - inr;
	mid = (mid * midscale) >> 17;
	side = (side * sidescale) >> 17;
	inl = mid + side;
	inr = mid - side;
	return STEREOPACK(inl,inr);
}
__STATIC_FORCEINLINE
s16 LINEARINTERPDL(const s16 *buf, int basei, int wobpos) { // read buf[basei-wobpos>>12] basically
	basei -= wobpos >> 12;
	wobpos &= 0xfff;
	s16 a0 = buf[basei & DLMASK];
	s16 a1 = buf[(basei - 1) & DLMASK];
#ifdef CORTEX
	int32_t out;
	uint32_t a = STEREOPACK(a1, a0);
	uint32_t b = STEREOPACK(wobpos, 0x1000 - wobpos);
	asm ("smuad %0, %1, %2" : "=r" (out) : "r" (a), "r" (b));
	return out >> 12;
#else
	// dual mul
	return ((a0*(0x1000-wobpos)+a1*wobpos))>>12;
#endif
}

__STATIC_FORCEINLINE
s16 LINEARINTERPRV(const s16* buf, int basei, int wobpos) { // read buf[basei-wobpos>>12] basically
	basei -= wobpos >> 12;
	wobpos &= 0xfff;
	s16 a0 = buf[basei & RVMASK];
	s16 a1 = buf[(basei - 1) & RVMASK];
#ifdef CORTEX
	int32_t out;
	uint32_t a = STEREOPACK(a1, a0);
	uint32_t b = STEREOPACK(wobpos, 0x1000 - wobpos);
	asm("smuad %0, %1, %2" : "=r" (out) : "r" (a), "r" (b));
	return out >> 12;
#else
	// dual mul
	return ((a0 * (0x1000 - wobpos) + a1 * wobpos)) >> 12;
#endif
}


