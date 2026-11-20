#ifndef USER_PROG_H
#define USER_PROG_H

#include <stdint.h>

#define USER_CODE_BASE  0x40000000u
#define USER_STACK_BASE 0x40100000u

#define USER_MSG_OFFSET 0x40

extern const uint8_t user_code_blob[];
extern const uint32_t user_code_size;

#endif
