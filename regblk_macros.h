//
// BLK Utility Macros
//
// You should include the register map file for your device only!
//
#ifndef REGBLK_MACROS_H_
#define REGBLK_MACROS_H_
#include "regblk_map.h"
// This is a recursive include, so the device register map
// needs to be protected against a double include.
// This is here because the blk_functions.c includes this
// file (it doesn't know what the register map is called)
// but user programs can include the register_map.h file.
#include "mcp_register_map.h"

#define BLK_ID(base,reg) ((unsigned int *) (base) + BLK_ID_BASE + (reg))
#define BLK_INTERRUPT(base,reg) ((unsigned int *) (base) + BLK_INTERRUPT_BASE + (reg))
#define BLK_DMA(base,reg) ((unsigned int *) (base) + BLK_DMA_BASE + (reg))

#endif
