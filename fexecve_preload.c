#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <err.h>

int execve(const char *path, char *const argv[], char *const envp[]) {
  int fd = memfd_create(path, 0);
  if(fd == -1){
    perror("memfd_create");
  }
  FILE * inbinfile;
  inbinfile = fopen(path, "r");
  // TODO error check the above, see "Disclaimers" in README.md
  fseek(inbinfile, 0, SEEK_END);
  long binsize = ftell(inbinfile) + 1;
  rewind(inbinfile);
  char inbin[binsize];
  fread(inbin, 1, binsize, inbinfile);
  write(fd, inbin, binsize);

  fexecve(fd, (char * const *) argv, (char * const *) envp);

  err(1, "%s failed", "fexecve");
}
