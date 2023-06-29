typedef struct lfo {
	float r, i, a;
} lfo;
#define LFOINIT(f) {1.f,0.f,(f)+(f)}
__STATIC_FORCEINLINE void lfo_setfreq(lfo *l, float f) {
	l->a = f + f;
}
__STATIC_FORCEINLINE float lfo_next(lfo *l) {
	l->r -= l->a * l->i;
	l->i += l->a * l->r;
	return l->r;
}

