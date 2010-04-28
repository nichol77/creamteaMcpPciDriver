//
// Basic Logic Kernel (BLK) functional block driver functions
//
// These functions allow for easy implementation of BLKs.

#include <linux/kernel.h>
#include <linux/pci.h>
#include "regblk_macros.h"

void blk_identify(char *id_string, unsigned int *base) {
  unsigned int blk_device_id;
  unsigned int blk_device_version;
  unsigned int blk_board_id;
  unsigned int blk_num_blks;
  unsigned int blk_blk_version;

  blk_device_id = readl(BLK_ID(base, BLK_ID_DEVICE));
  blk_device_version = readl(BLK_ID(base, BLK_ID_VERSION));
  blk_board_id = readl(BLK_ID(base, BLK_ID_BOARDID));
  blk_num_blks = readl(BLK_ID(base, BLK_ID_NUMREGS));
  blk_blk_version = readl(BLK_ID(base, BLK_ID_BLKVERSION));

  printk(KERN_INFO "%s: Device ID is %c%c%c%c (board %d)\n",
	 id_string,
	 (blk_device_id & 0xFF000000) >> 24,
	 (blk_device_id & 0x00FF0000) >> 16,
	 (blk_device_id & 0x0000FF00) >> 8,
	 (blk_device_id & 0xFF),
	 (blk_board_id));
  printk(KERN_INFO "%s: Firmware Version %d.%d rev %d (build date %d/%d)\n",
	 id_string,
	 (blk_device_version & 0x0000F000) >> 12,
	 (blk_device_version & 0x00000F00) >> 8,
	 (blk_device_version & 0x000000FF),
	 (blk_device_version & 0xFF000000) >> 24,
	 (blk_device_version & 0x00FF0000) >> 16);
  printk(KERN_INFO "%s: %d BLKs (BLK ID version %d)\n",
	 id_string,
	 blk_num_blks,
	 blk_blk_version);
}
