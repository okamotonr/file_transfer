#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common.h"

int client_get_contents(int socket_fd) {
  uint64_t buffer_size;
  if (read(socket_fd, &buffer_size, sizeof(buffer_size)) !=
      sizeof(buffer_size)) {
    perror("failed to read header\n");
    return -1;
  }

  buffer_size = ntohll(buffer_size);
  uint8_t *buffer = malloc(buffer_size);
  uint64_t buffer_rem = buffer_size;
  uint64_t offset = 0;
  while (buffer_rem > 0) {
    ssize_t read_result = read(socket_fd, buffer + offset, buffer_size);
    if (read_result == 0) {
      perror("connectio closed()\n");
      free(buffer);
      return -1;
    } else if (read_result < 0) {
      perror("failed to read()\n");
      free(buffer);
      return -1;
    } else {
      offset += (uint64_t)read_result;
      buffer_rem -= (uint64_t)read_result;
    }
  }

  offset = 0;
  while (buffer_size > offset) {
    file_info file_info;
    size_t _offset = deserialize_file_info(&file_info, (buffer + offset));
    print_file_info(&file_info);
    offset += _offset;
    if (offset + file_info_ser_size() > buffer_size) {
      fprintf(stderr, "something wrong happen\n");
      break;
    }
  }

  free(buffer);
  printf("finish get contents\n");
  return 0;
}

int handle_client_send_file(int socket_fd, const char *file_name,
                            const char *local_file_path) {
  // wait ack
  printf("send file from %s to %s\n", local_file_path, file_name);
  uint8_t resp;
  file_info file_info;
  if (read(socket_fd, &resp, sizeof(uint8_t)) <= 0) {
    perror("failed to read\n");
    return -1;
  }

  if (resp != ACK) {
    fprintf(stderr, "server couldnot ack send file, %d\n", resp);
    return -1;
  }

  if (from_file_name_to_file_info(local_file_path, &file_info) != 0) {
    fprintf(stderr, "cannot get file info %s\n", local_file_path);
    return -1;
  }
  return send_file(socket_fd, &file_info);
}

int handle_client_recv_file(int socket_fd, const char *file_name,
                            const char *local_file_path) {
  printf("recv file from %s to %s\n", file_name, local_file_path);
  uint8_t resp;
  if (read(socket_fd, &resp, sizeof(uint8_t)) <= 0) {
    perror("failed to read\n");
    return -1;
  }

  if (resp != ACK) {
    fprintf(stderr, "server couldnot ack send file, %d\n", resp);
    return -1;
  }

  return recv_file(socket_fd, local_file_path);
}

int handle_client_get_contents(int socket_fd, const char *file_name) {
  printf("get contents of %s\n", file_name);
  uint8_t resp;
  if (read(socket_fd, &resp, sizeof(uint8_t)) <= 0) {
    perror("failed to read\n");
    return -1;
  }

  if (resp != ACK) {
    fprintf(stderr, "server couldnot ack send file, %d\n", resp);
    return -1;
  }

  return client_get_contents(socket_fd);
}

int handle_client_mkdir(int socket_fd, const char *file_name) {
  printf("mkdir %s\n", file_name);
  uint8_t resp;
  if (read(socket_fd, &resp, sizeof(uint8_t)) <= 0) {
    perror("failed to read\n");
  }

  if (resp != ACK) {
    fprintf(stderr, "server couldnot mkdir, %d\n", resp);
    return -1;
  }
  return 0;
}

int handle_client_rmdir(int socket_fd, const char *file_name) {
  printf("rmdir %s\n", file_name);
  uint8_t resp;
  if (read(socket_fd, &resp, sizeof(uint8_t)) <= 0) {
    perror("failed to read\n");
  }

  if (resp != ACK) {
    fprintf(stderr, "server couldnot rmdir, %d\n", resp);
    return -1;
  }
  return 0;
}

int handle_client_general(int socket_fd, const char *file_name,
                          const char *command_name) {
  printf("%s %s\n", command_name, file_name);
  uint8_t resp;
  if (read(socket_fd, &resp, sizeof(uint8_t)) <= 0) {
    perror("failed to read\n");
  }

  if (resp != ACK) {
    fprintf(stderr, "server couldnot %s, %d\n", command_name, resp);
    return -1;
  }
  return 0;
}

void send_cmd(int socket_fd, const struct transfer_cmd *cmd) {
  uint8_t buffer[sizeof(cmd->cmd) + sizeof(cmd->file_path)];
  ssize_t ret;

  memcpy(buffer, &cmd->cmd, sizeof(cmd->cmd));
  memcpy(buffer + sizeof(cmd->cmd), cmd->file_path, sizeof(cmd->file_path));

  ret = write(socket_fd, buffer, sizeof(buffer));
  if (ret != sizeof(buffer)) {
    perror("write error\n");
  }
}

int handle_client(int argc, char *argv[]) {
  int client_sock;
  struct sockaddr_in server_addr;
  uint8_t cmd_type;
  int parse_command_ret;
  if (argc < 5) {
    return -1;
  }

  char *command_name = argv[2];
  char *addr = argv[3];
  char *remote_file_path = argv[4];

  if ((parse_command_ret = parse_command(command_name)) == -1) {
    fprintf(stderr, "failed to parse_command()\n");
    exit(EXIT_FAILURE);
  }
  cmd_type = parse_command_ret;

  transfer_cmd cmd;
  cmd.cmd = cmd_type;
  memcpy(cmd.file_path, remote_file_path, sizeof(cmd.file_path));
  print_transfer_cmd(&cmd);

  if (cmd.cmd == SEND_FILE || cmd.cmd == RECV_FILE) {

    if (argc < 6) {
      fprintf(stderr, "require local file path\n");
      return -1;
    }
  }

  memset(&server_addr, 0, sizeof(server_addr));

  if (inet_aton(addr, &server_addr.sin_addr) == 0) {
    fprintf(stderr, "address is not acceptable\n");
    exit(EXIT_FAILURE);
  }
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(PORT);

  if ((client_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("failed socket()\n");
    exit(EXIT_FAILURE);
  }

  if (connect(client_sock, (struct sockaddr *)&server_addr,
              sizeof(server_addr)) < 0) {
    perror("failed connect()\n");
    exit(EXIT_FAILURE);
  }
  printf("connection established\n");
  send_cmd(client_sock, &cmd);
  switch (cmd.cmd) {
  case SEND_FILE:
    handle_client_send_file(client_sock, cmd.file_path, argv[5]);
    break;
  case RECV_FILE:
    handle_client_recv_file(client_sock, cmd.file_path, argv[5]);
    break;
  case GET_CONTENTS:
    handle_client_get_contents(client_sock, cmd.file_path);
    break;
  case MKDIR:
    handle_client_general(client_sock, cmd.file_path, "mkdir");
    break;
  case RMDIR:
    handle_client_general(client_sock, cmd.file_path, "rmdir");
    break;
  case REMOVE:
    handle_client_general(client_sock, cmd.file_path, "remove");
    break;
  default:
    fprintf(stderr, "unknown cmd");
    return -1;
  }
  return 0;
}
