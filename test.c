#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include "testDriver_ioctl.h"

int main(int argc, char **argv) {
  struct testDriverRead req;
  int testfd;
  unsigned char cmd;
  
  testfd = open("/dev/test", O_RDWR | O_NONBLOCK);
  if (!testfd) {
    perror("open");
    exit(1);
  }
  argc--;
  argv++;
  
  if (!argc) exit(1);
  cmd = **argv;

  argc--;
  argv++;

  if (!argc) exit(1);
  if (cmd == 'r') {
    req.address = strtoul(*argv, NULL, 0);
    req.value = 0x00000000;
    req.barno = 1;
    if (ioctl(testfd, TEST_IOCREAD, &req)) {
      perror("ioctl");
      close(testfd);
      exit(1);
    }
  } else if (cmd == 'w') {
    req.address = strtoul(*argv, NULL, 0);
    req.barno = 1;
    argc--;
    argv++;
    if (!argc) exit(1);
    req.value = strtoul(*argv, NULL, 0);
    if (ioctl(testfd, TEST_IOCWRITE, &req)) {
      perror("ioctl");
      close(testfd);
      exit(1);
    }
  } else if (cmd == 'R') {
    req.address = strtoul(*argv, NULL, 0);
    req.value = 0x00000000;
    req.barno = 0;
    if (ioctl(testfd, TEST_IOCREAD, &req)) {
      perror("ioctl");
      close(testfd);
      exit(1);
    }
  } else if (cmd == 'W') {
    req.address = strtoul(*argv, NULL, 0);
    req.barno = 0;
    argc--;
    argv++;
    if (!argc) exit(1);
    req.value = strtoul(*argv, NULL, 0);
    if (ioctl(testfd, TEST_IOCWRITE, &req)) {
      perror("ioctl");
      close(testfd);
      exit(1);
    }
  }
  printf("0x%8.8X: 0x%8.8X\n", req.address, req.value);
  close(testfd);
  exit(0);
}
