//
// MCP Register Map
//
#ifndef MCP_REGISTER_MAP_H_
#define MCP_REGISTER_MAP_H_

// The register map inside each block is common between most drivers
// which use the BLK functional blocks. (4x 32 bit registers)
#include "regblk_map.h"

// So all we need to do is define the block bases
#define BLK_DMA_BASE 0x18
#define BLK_INTERRUPT_BASE 0x14
#define BLK_ID_BASE 0x0

// The MCP has one custom block
#define BLK_MCP_BASE 0x1C
#define BLK_MCP_COMMAND 0x0
#define BLK_MCP_TXLENGTH 0x1

// Utility macros
#include "regblk_macros.h"
#define BLK_MCP(base,reg) ((unsigned int *) (base) + BLK_MCP_BASE + (reg))

#endif
