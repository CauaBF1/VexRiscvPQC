#include <stddef.h>
#include <stdint.h>

extern uint8_t _heap_start;
extern uint8_t _heap_end;

static uintptr_t heap_current = 0;

static uintptr_t align_up(uintptr_t value, uintptr_t alignment) {
  return (value + alignment - 1u) & ~(alignment - 1u);
}

void *memcpy(void *dest, const void *src, size_t n) {
  uint8_t *d = (uint8_t *)dest;
  const uint8_t *s = (const uint8_t *)src;

  for (size_t i = 0; i < n; i++) {
    d[i] = s[i];
  }

  return dest;
}

void *memset(void *dest, int value, size_t n) {
  uint8_t *d = (uint8_t *)dest;

  for (size_t i = 0; i < n; i++) {
    d[i] = (uint8_t)value;
  }

  return dest;
}

int memcmp(const void *lhs, const void *rhs, size_t n) {
  const uint8_t *a = (const uint8_t *)lhs;
  const uint8_t *b = (const uint8_t *)rhs;

  for (size_t i = 0; i < n; i++) {
    if (a[i] != b[i]) {
      return (int)a[i] - (int)b[i];
    }
  }

  return 0;
}

void *malloc(size_t size) {
  if (size == 0) {
    return 0;
  }

  if (heap_current == 0) {
    heap_current = (uintptr_t)&_heap_start;
  }

  uintptr_t header = align_up(heap_current, 8u);
  uintptr_t start = header + sizeof(uintptr_t);
  uintptr_t end = align_up(start + size, 8u);

  if (end > (uintptr_t)&_heap_end) {
    return 0;
  }

  *((uintptr_t *)header) = end - header;
  heap_current = end;
  return (void *)start;
}

void free(void *ptr) {
  if (ptr == 0) {
    return;
  }

  uintptr_t header = (uintptr_t)ptr - sizeof(uintptr_t);
  uintptr_t size = *((uintptr_t *)header);

  if (header + size == heap_current) {
    heap_current = header;
  }
}

void exit(int status) {
  (void)status;
  while (1) {
  }
}
