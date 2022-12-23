#ifndef TYPES_H
#define TYPES_H

#define true 1
#define false 0
#define NULL 0

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;
typedef uint8_t bool;

#define asmlinkage __attribute__((regparm(0))) 

#endif
