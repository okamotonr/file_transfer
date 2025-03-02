#include <stddef.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "client.h"
#include "server.h"

const char* action_to_string(int action) {
    switch (action) {
        case SEND_FILE:
            return "SEND";
        case RECV_FILE:
            return "RECV";
        case GET_CONTENTS:
            return "DIR";
        default:
            return "UNKNOWN";
    }
}

void print_transfer_cmd(const transfer_cmd *tcmd) {
    if (tcmd == NULL) {
        printf("NULL pointer provided.\n");
        return;
    }

    printf("Transfer Command:\n");
    printf("  Command   : %s\n", action_to_string(tcmd->cmd));
    printf("  File Name : %s\n", tcmd->file_info.file_name);
    printf("  File Size : %u bytes\n", tcmd->file_info.file_size);
}

ssize_t read_all(int fd, char* buffer) {
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
    handle_server();
    return 0;
  }

  if (strcmp(argv[1], "client") == 0) {
    printf("client\n");
    if (argc < 5) {
      printf("command, address and file name\n");
    } else {
      handle_client(argv[2], argv[3], argv[4]);
    }
    return 0;
  }

  printf("should be server or client\n");
  return 1;
}

