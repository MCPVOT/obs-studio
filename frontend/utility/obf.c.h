#include "frontend/build/frontend-utility-c.pch.h"

#define LOWER_HALFBYTE(x) ((x) & 0xF)
#define UPPER_HALFBYTE(x) (((x) >> 4) & 0xF)

void deobfuscate_str(char *str, uint64_t val)
{
	uint8_t *dec_val = (uint8_t *)&val;
	int i = 0;

	while (*str != 0) {
		int pos = i / 2;
		bool bottom = (i % 2) == 0;
		uint8_t *ch = (uint8_t *)str;
		uint8_t xor = bottom ? LOWER_HALFBYTE(dec_val[pos]) : UPPER_HALFBYTE(dec_val[pos]);

		*ch ^= xor;

		if (++i == sizeof(uint64_t) * 2) {
			i = 0;
		}

		str++;
	}
}

/* File-local macros (BUILD.md): out of the next file in the unity group. */
#undef LOWER_HALFBYTE
#undef UPPER_HALFBYTE
