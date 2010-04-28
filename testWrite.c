#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

int main() {
  unsigned int command[4] = {0x1,0x1,0x1,0x1};
  int fd;

  fd = open("/dev/test", O_RDWR);
  if (fd < 0) {perror("open"); exit(1);}
  if (write(fd, command, sizeof(unsigned int)*4) < 0) {
    perror("write"); close(fd); exit(1);
  }
  close(fd);
}
