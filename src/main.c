#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "client.h"
#include "server.h"

ssize_t read_all(int fd, char *buffer) {
  ssize_t n;
  size_t size = sizeof(*buffer);
  size_t total_read = 0;
  while (total_read < size) {
    n = read(fd, buffer + total_read, size - total_read);
    if (n < 0) {
      perror("failed to read()\n");
      return n;
    } else if (n == 0) {
      fprintf(stderr, "connection closed\n");
      return n;
    }
    total_read += n;
  }
  return 1;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("specify server or clinet");
    return 0;
  }

  if (strcmp(argv[1], "server") == 0) {
    printf("server\n");
    if (argc >= 3) {
      const char *woring_dir = argv[2];
      if (chdir(woring_dir) != 0) {
        perror(woring_dir);
        exit(EXIT_FAILURE);
      };
    }
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
      printf("current working directory is %s\n", cwd);
    } else {
      perror("failed to getcwd\n");
      exit(EXIT_FAILURE);
    }
    handle_server();
    return 0;
  }

  if (strcmp(argv[1], "client") == 0) {
    printf("client\n");
    handle_client(argc, argv);
    return 0;
  }

  printf("should be server or client\n");
  return 1;
}
