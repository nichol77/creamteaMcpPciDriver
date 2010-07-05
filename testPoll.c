#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include "mcpPciDriver_ioctl.h"
int main() {
  struct mcpPciDriverRead req;
  int tfd;
  int val;

  tfd = open("/dev/test", O_RDONLY | O_NONBLOCK);
  if (!tfd) {perror("open");exit(1);}
  val = 0;
  while (1) {
    req.address = 0x14;
    req.barno = 1;
    req.value = 0x000;
    if (ioctl(tfd, TEST_IOCREAD, &req)) { perror("ioctl");close(tfd);exit(1);}
    if ((req.value & 0x2) != (val & 0x2)) {
      if (req.value & 0x2) printf("TICK\n");
      else printf("TOCK\n");
    }
    val = req.value;
  }
  close(tfd);
}
