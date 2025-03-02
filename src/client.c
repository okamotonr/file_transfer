#include "common.h"

#include <stddef.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

int handle_client_send_file(int socket_fd, const char *file_name) {
  // wait ack
  char resp;
  file_info file_info;
  if (read(socket_fd, &resp, sizeof(char)) <= 0) {
    perror("failed to read\n");
    return -1;
  }

  if (resp != ACK) {
    fprintf(stderr, "server couldnot ack send file, %d\n", resp);
    return -1;
  }

  if (from_file_name_to_file_info(file_name, &file_info) != 0) {
    fprintf(stderr, "cannot get file info %s\n", file_name);
    return -1;
  }
  return send_file(socket_fd, &file_info);

}

int handle_client_recv_file(int socket_fd, const char *file_name) {
  printf("recv file");
  char resp;
  if (read(socket_fd, &resp, sizeof(char)) <= 0) {
    perror("failed to read\n");
    return -1;
  }

  if (resp != ACK) {
    fprintf(stderr, "server couldnot ack send file, %d\n", resp);
    return -1;
  }
  return recv_file(socket_fd, file_name);
}


int handle_client_get_contents() {}

void send_cmd(int socket_fd, const struct transfer_cmd *cmd) {
  unsigned char buffer[sizeof(cmd->cmd) + sizeof(cmd->file_name)];
  ssize_t ret;

  memcpy(buffer, &cmd->cmd, sizeof(cmd->cmd));
  memcpy(buffer + sizeof(cmd->cmd), cmd->file_name, sizeof(cmd->file_name));

  ret = write(socket_fd, buffer, sizeof(buffer));
  if (ret != sizeof(buffer)) {
    perror("write error\n");
  }
}


int handle_client(char* command_name, char* addr, char *file_name) {
  int client_sock;
  struct sockaddr_in server_addr;
  char cmd_type;
  int parse_command_ret;
  if ((parse_command_ret = parse_command(command_name)) == -1) {
    fprintf(stderr, "failed to parse_command()");
    exit(EXIT_FAILURE);
  }
  cmd_type = parse_command_ret;

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

  if (connect(client_sock, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
    perror("failed connect()\n");
    exit(EXIT_FAILURE);
  } 
  printf("connection established\n");

  transfer_cmd cmd;
  cmd.cmd = cmd_type;
  memcpy(cmd.file_name, file_name, sizeof(cmd.file_name));
  print_transfer_cmd(&cmd);
  send_cmd(client_sock, &cmd);
  switch (cmd.cmd) {
     case SEND_FILE:
       handle_client_send_file(client_sock, cmd.file_name);
       break;
     case RECV_FILE:
       handle_client_recv_file(client_sock, cmd.file_name);
       break;
case GET_CONTENTS:
       handle_client_get_contents();
       break;
     default:
       fprintf(stderr, "unknown cmd");
       return -1;
  }
  return 0;
}

