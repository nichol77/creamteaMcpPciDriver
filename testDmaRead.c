#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

int exit_program;
void signal_handler(int signum) {
  exit_program = 1;
}

int main() {
  int tfd;
  unsigned int *buffer;
  int cnt;
  int i,j;

  tfd = open("/dev/test", O_RDONLY | O_NONBLOCK);
  if (tfd <0) {perror("open"); exit(1); }
  // Freaking huge!
  buffer = malloc(sizeof(unsigned int)*300000);
  if (!buffer) 
     {
	printf("WTF No buffer space!\n");
	close(tfd);
	exit(1);
     }
  printf("Starting read\n");
  // Woo lots of data!
  signal(SIGINT, signal_handler);
  signal(SIGQUIT, signal_handler);
  signal(SIGPIPE, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGABRT, signal_handler);
  signal(SIGHUP, signal_handler);

  exit_program = 0;  
  while (!exit_program) {
    cnt = read(tfd, buffer, 300000*sizeof(unsigned int));
    if (cnt < 0) 
      {
	if (errno == EAGAIN) {
	  sleep(1);
	  continue;
	}
	perror("read");
	free(buffer);
	close(tfd);
	exit(1);
      }
    
    printf("Out of read: %d bytes read\n", cnt);
    for (i=0;i<(cnt/sizeof(int));i+=8) {
      for (j=0;j<8;j++) 
	{
	  if (i+j<(cnt/sizeof(int)))
	    printf("%8.8X", buffer[i+j]);
	  if ((j+1==8) || (i+j+1==cnt))
	    printf("\n");
	  else
	    printf(" ");
	    //printf("\n");
	}
    }
    exit_program = 1;
  }
  free(buffer);
  close(tfd);
}

