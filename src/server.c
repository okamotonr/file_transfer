#include "common.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

int get_contents(int socket_fd, const file_meta* file_info) {
  
  return 0;
}

int handle_server_send_file(int socket_fd, const char *file_name) {
  printf("client call send file\n");
  if (!is_valid_name(file_name)) {
    fprintf(stderr, "file_name is invalid, %s\n", file_name);
    char resp = INVALIDNAME;
    if (write(socket_fd, &resp, sizeof(char)) < 0) {
      perror("failed to write()\n");
    }
    return -1;
  }
  char ack = ACK;
  if (write(socket_fd, &ack, sizeof(char)) < 0) {
    perror("faied to write()\n");
    return -1;
  }
  recv_file(socket_fd, file_name);
  return 0;
}

int handle_server_recv_file(int socket_fd, const char *file_name) {

  file_meta file_info;
  printf("client call recv file\n");

  if (!is_valid_name(file_name)) {
    fprintf(stderr, "file_name is invalid, %s\n", file_name);
    char resp = INVALIDNAME;
    if (write(socket_fd, &resp, sizeof(char)) < 0) {
      perror("failed to write()\n");
    }
    return -1;
  }
  if (from_file_name_to_file_info(file_name, &file_info) != 0) {
    fprintf(stderr, "failed to get file info\n");
    char resp = NOTFOUND;
    if (write(socket_fd, &resp, sizeof(char)) < 0) {
      perror("failed to write()\n");
    }
    return -1;
  }

  if (file_info.file_type == D) {
    fprintf(stderr, "cannot send directory\n");
    char resp = ISDIR;
    if (write(socket_fd, &resp, sizeof(char)) < 0) {
      perror("failed to write()\n");
    }
    return -1;
  }

  char ack = ACK;
  if (write(socket_fd, &ack, sizeof(char)) < 0) {
    perror("faied to write()\n");
    return -1;
  }

  return send_file(socket_fd, &file_info);
}

int handle_server_get_contents(int socket_fd, const char *file_name) {
  file_meta file_info;
  if (!is_valid_name(file_name)) {
    fprintf(stderr, "file_name is invalid, %s\n", file_name);
    char resp = INVALIDNAME;
    if (write(socket_fd, &resp, sizeof(char)) < 0) {
      perror("failed to write()\n");
    }
    return -1;
  }

  if (file_info.file_type == F) {
    fprintf(stderr, "not directory, %s\n", file_name);
    char resp = ISNOTDIR;
    if (write(socket_fd, &resp, sizeof(char)) < 0) {
      perror("failed to write()\n");
    }
    return -1;
  }

  char ack = ACK;
  if (write(socket_fd, &ack, sizeof(char)) < 0) {
    perror("faied to write()\n");
    return -1;
  }

  return 0;
}

void recv_cmd(int socket_fd, struct transfer_cmd *cmd) {
  char buffer[sizeof(cmd->cmd) + sizeof(cmd->file_name)];

  size_t total_read = 0;
  ssize_t n;
  while (total_read < sizeof(buffer)) {
    n = read(socket_fd, buffer + total_read, sizeof(buffer) - total_read);
    if (n < 0) {
      perror("failed to read()\n");
      exit(EXIT_FAILURE);
    } else if (n == 0) {
      fprintf(stderr, "connection closed\n");
      exit(EXIT_FAILURE);
    }
    total_read += n;
  }

  memcpy(&cmd->cmd, buffer, sizeof(cmd->cmd));

  uint32_t file_size;
  memcpy(&file_size, buffer + sizeof(cmd->cmd), sizeof(file_size));

  memcpy(cmd->file_name, buffer + sizeof(cmd->cmd), sizeof(cmd->file_name));
}

int handle_server() {
  int sock_fd;
  int client_sock;
  unsigned int client_len;
  struct sockaddr_in sock_addr;
  struct sockaddr_in client_addr;

  printf("starting server...\n");
  sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_fd < 0) {
    perror("failed to open socket\n");
    exit(EXIT_FAILURE);
  }

  memset(&sock_addr, 0, sizeof(sock_addr));
  sock_addr.sin_family = AF_INET;
  sock_addr.sin_port = htons(PORT);
  sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sock_fd, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0) {
    perror("failed to bind socket\n");
    exit(EXIT_FAILURE);
  }

  if (listen(sock_fd, 5) < 0) {
    perror("failed to listen socket\n");
    exit(EXIT_FAILURE);
  }
  printf("waiting for connection...\n");

  while (1) {
    client_len = sizeof(client_addr);
    if ((client_sock = accept(sock_fd, (struct sockaddr *)&client_addr,
                              &client_len)) < 0) {
      perror("failed to accept socket\n");
      exit(EXIT_FAILURE);
    }
    printf("connection established\n");
    transfer_cmd cmd;
    recv_cmd(client_sock, &cmd);
    print_transfer_cmd(&cmd);
    switch (cmd.cmd) {
    case SEND_FILE:
      handle_server_send_file(client_sock, cmd.file_name);
      break;
    case RECV_FILE:
      handle_server_recv_file(client_sock, cmd.file_name);
      break;
    case GET_CONTENTS:
      handle_server_get_contents(client_sock, cmd.file_name);
      break;
    default:
      fprintf(stderr, "unknown command");
    }
  }
  return 0;
}
