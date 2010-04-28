#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  unsigned int value;
  unsigned int temp;

  argc--;
  argv++;

  if (argc != 2) {
    printf("decode: syntax\n");
    printf("        decode (register type) (register value)\n");
    exit(1);
  }
  if (!strcmp(*argv, "brd")) {
    argv++;
    sscanf(*argv, "%x", &value);
    printf("Read 0x%X\n", value);
    printf("Decoded as...\n");
    if (value & (1<<0)) printf("   BRD_BURST_ENABLE\n");
    if (value & (1<<1)) printf("   BRD_NREADY_ENABLE\n");
    if (value & (1<<2)) printf("   BRD_BTERM_ENABLE\n");
    temp = (value >> 3) & 0x3;
    if (!temp) printf("   BRD_PREFETCH_NONE\n");
    else if (temp == 0x1) printf("   BRD_PREFETCH_04\n");
    else if (temp == 0x2) printf("   BRD_PREFETCH_08\n");
    else if (temp == 0x3) printf("   BRD_PREFETCH_16\n");
    if (value & (1<<5)) printf("   BRD_PREFETCH_ENABLE\n");
    temp = (value >> 6) & 0x1F;
    if (temp) {
      printf("   BRD_NRAD_%2.2d\n", temp);
    }
    temp = (value >> 11) & 0x3;
    if (temp) {
      printf("   BRD_NRDD_%d\n", temp);
    }
    temp = (value >> 13) & 0x3;
    if (temp) {
      printf("   BRD_NXDA_%d\n", temp);
    }
    temp = (value >> 15) & 0x1F;
    if (temp) {
      printf("   BRD_NWAD_%d\n", temp);
    }
    temp = (value >> 20) & 0x3;
    if (temp) {
      printf("   BRD_NWDD_%d\n", temp);
    }
    temp = (value >> 22) & 0x3;
    if (temp == 0) printf("   BRD_BUS_WIDTH_8\n");
    if (temp == 1) printf("   BRD_BUS_WIDTH_16\n");
    if (temp == 2) printf("   BRD_BUS_WIDTH_32\n");
    if (temp == 3) printf("   BRD_BUS_WIDTH_RESERVED\n");
    
    if (value & (1<<24)) printf("   BRD_BIG_ENDIAN\n");
    else printf("   BRD_LITTLE_ENDIAN\n");

    if (value & (1<<25)) printf("   BRD_BIG_ENDIAN_HIGH_BYTE_LANE\n");
    
    temp = (value >> 26) & 0x3;
    if (temp) {
      printf("   BRD_RD_WS_%d\n", temp);
    }
    temp = (value >> 28) & 0x3;
    if (temp) {
      printf("   BRD_WR_WS_%d\n", temp);
    }
    temp = (value >> 30) & 0x3;
    if (temp) {
      printf("   BRD_WR_HOLD_%d\n", temp);
    }
  }
}

