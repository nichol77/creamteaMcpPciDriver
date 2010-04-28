#include <linux/ioctl.h>

/* we use A1 as our magic, for no good reason */
/* but it is good steak sauce */
#define TEST_IOC_MAGIC 0xA1
/* SURFs have 0-15, LOS has 16-31, TURFIO 32-47, we take 48+ */
#define TEST_IOC_MINNR 0x30

struct testDriverRead {
  unsigned int barno;
  unsigned int address;
  unsigned int value;
};

#define TEST_IOCREAD _IOWR(TEST_IOC_MAGIC, TEST_IOC_MINNR+0, struct testDriverRead)
#define TEST_IOCWRITE _IOWR(TEST_IOC_MAGIC, TEST_IOC_MINNR+1, struct testDriverRead)

#define TEST_IOC_MAXNR TEST_IOC_MINNR+2
