#ifndef BITUTILS_H_HBYMFJ6H
#define BITUTILS_H_HBYMFJ6H
#include <assert.h>
#include <stdint.h>
#if defined(LITTLE_ENDIAN)
static inline uint16_t ooLittleToHost16(uint16_t a) {return a;}
static inline uint32_t ooLittleToHost32(uint32_t a) {return a;}
static inline uint64_t ooLittleToHost64(uint64_t a) {return a;}

static inline uint16_t
ooBigToHost16(uint16_t a)
{
  return a << 8 | a >> 8;
}

static inline uint32_t
ooBigToHost32(uint32_t a)
{
  return (ooBigToHost16(a & 0x0ffff) << 16) | ooBigToHost16(a >> 16);
}

static inline uint64_t
ooBigToHost64(uint64_t a)
{
  assert(0 && "not implemented");
  
  return a;
}

#elif defined(BIG_ENDIAN)

static inline uint16_t
ooLittleToHost16(uint16_t a)
{
  return a << 8 | a >> 8;
}

static inline uint32_t
ooLittleToHost32(uint32_t a)
{
  return (ooLittleToHost16(a & 0x0ffff) << 16) | ooLittleToHost16(a >> 16);
}

static inline uint64_t
ooLittleToHost64(uint64_t a)
{
  assert(0 && "not implemented");
  return a;
}

static inline uint16_t ooBigToHost16(uint16_t a) {return a;}
static inline uint32_t ooBigToHost32(uint32_t a) {return a;}
static inline uint64_t ooBigToHost64(uint64_t a) {return a;}

#else
#error "Unknown endianess of target"
#endif

#endif /* end of include guard: BITUTILS_H_HBYMFJ6H */