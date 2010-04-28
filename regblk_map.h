//
// Register map for devices using the BLK functional block design.
//

#ifndef REGBLK_MAP_H_
#define REGBLK_MAP_H_

///////////////////////////////
// BLK DMA block, v1.0
///////////////////////////////
// base+0x0: DMA address
//           May be an address to perform DMA into/from,
//           or a scatter-gather list depending on
//           type of DMA.
// base+0x1: DMA length. Always is length of DMA transfer.
// base+0x2: Status of DMA block.
// base+0x3: Control register.
//           Bits [31:28]: Indicate what type of DMA is supported.
//           Bit [31]: Single-length transfer TO device
//           Bit [30]: Scatter-gather transfer TO device
//           Bit [29]: Single-length transfer FROM device
//           Bit [28]: Scatter-gather transfer FROM device
//           Bits [3:0]: Initiate type of DMA transfer
//           Bit [3]: Start single-length transfer TO device
//           Bit [2]: Start scatter-gather transfer TO device
//           Bit [1]: Start single-length transfer FROM device
//           Bit [0]: Start scatter-gather transfer FROM device
///////////////////////////////
#define BLK_DMA_ADDR 0x0
#define BLK_DMA_LENGTH 0x1
#define BLK_DMA_STATUS 0x2
#define BLK_DMA_CONTROL 0x3
///////////////////////////////
// BLK Interrupt Block, v1.0
///////////////////////////////
// 
#define BLK_INTERRUPT_CSR 0x0
#define BLK_INTERRUPT_SOFT 0x1
///////////////////////////////
// BLK ID block: (4xBLKs long)
///////////////////////////////
#define BLK_ID_DEVICE 0x0
#define BLK_ID_VERSION 0x1
#define BLK_ID_BOARDID 0x2
#define BLK_ID_STATUS 0x3
#define BLK_ID_NUMREGS 0x4
#define BLK_ID_BLKVERSION 0x5
#endif
