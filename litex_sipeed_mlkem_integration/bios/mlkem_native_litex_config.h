#ifndef MLKEM_NATIVE_LITEX_CONFIG_H
#define MLKEM_NATIVE_LITEX_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#define MLK_CONFIG_PARAMETER_SET 512
#define MLK_CONFIG_NAMESPACE_PREFIX mlkem
#define MLK_CONFIG_INTERNAL_API_QUALIFIER static
#define MLK_CONFIG_NO_ASM
#define MLK_CONFIG_NO_RANDOMIZED_API
#define MLK_CONFIG_CUSTOM_ZEROIZE

static inline void mlk_zeroize(void *ptr, size_t len)
{
	volatile uint8_t *p = (volatile uint8_t *)ptr;
	while (len-- != 0)
		*p++ = 0;
}

#endif
