#include <arpa/inet.h>
#include <dirent.h>
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
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
int server_get_contents(int socket_fd, DIR *dir) {
  int count = 0;
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    count += 1;
  }
  rewinddir(dir);

  size_t max_ser_size = count * file_info_ser_size();
  uint8_t *buffer = malloc(max_ser_size);

  int dir_fd = dirfd(dir);

  if (buffer == NULL) {
    perror("malloc");
    closedir(dir);
    return -1;
  }

  size_t offset = 0;
  printf("count is %d\n", count);

  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    struct stat st;

    if (fstatat(dir_fd, entry->d_name, &st, 0) == -1) {
      perror("faied to fstatat()\n");
      continue;
    }

    file_info file_info;

    if (S_ISDIR(st.st_mode)) {
      file_info.file_type = D;
    } else if (S_ISREG(st.st_mode)) {
      file_info.file_type = F;
    } else {
      continue;
    }

    file_info.file_size = (uint64_t)st.st_size;

    strncpy(file_info.file_name, entry->d_name, MAX_NAME - 1);
    file_info.file_name[MAX_NAME - 1] = '\0';

    serialize_file_info(&file_info, buffer + offset);
    offset += file_info_ser_size();
  }

  uint64_t bytes = htonll(offset);
  printf("send bytes as header\n");
  if (send(socket_fd, &bytes, sizeof(bytes), 0) != sizeof(bytes)) {
    perror("failed to send content bytes\n");
    free(buffer);
    return -1;
  }

  printf("send body\n");
  if (send(socket_fd, buffer, offset, 0) != (ssize_t)offset) {
    perror("failed to send entries\n");
    free(buffer);
    return -1;
  }

  free(buffer);
  printf("free buffer\n");

  return 0;
}

int handle_server_mkdir(int socket_fd, const char *file_path) {
  printf("mkdir %s\n", file_path);
  uint8_t resp;
  if (!is_valid_path(file_path)) {
    fprintf(stderr, "file path %s is not allowed\n", file_path);
    resp = INVALIDNAME;
  } else if (mkdir(file_path, 0755) == 0) {
    resp = ACK;
  } else {
    perror(file_path);
    resp = FAILED;
  }
  if (write(socket_fd, &resp, sizeof(uint8_t)) < 0) {
    perror("failed to write()\n");
    return -1;
  }
  return 0;
}

int handle_server_rmdir(int socket_fd, const char *file_path) {
  printf("rmdir %s\n", file_path);
  uint8_t resp;
  if (!is_valid_path(file_path)) {
    fprintf(stderr, "file path %s is not allowed\n", file_path);
    resp = INVALIDNAME;
  } else if (rmdir(file_path) == 0) {
    resp = ACK;
  } else {
    perror(file_path);
    resp = FAILED;
  }
  if (write(socket_fd, &resp, sizeof(uint8_t)) < 0) {
    perror("failed to write()\n");
    return -1;
  }
  return 0;
}

int handle_server_remove(int socket_fd, const char *file_path) {
  printf("remove %s\n", file_path);
  uint8_t resp;
  if (!is_valid_path(file_path)) {
    fprintf(stderr, "file path %s is not allowed\n", file_path);
    resp = INVALIDNAME;
  } else if (remove(file_path) == 0) {
    resp = ACK;
  } else {
    perror(file_path);
    resp = FAILED;
  }
  if (write(socket_fd, &resp, sizeof(uint8_t)) < 0) {
    perror("failed to write()\n");
    return -1;
  }
  return 0;
}

int handle_server_send_file(int socket_fd, const char *file_name) {
  printf("client call send file\n");
  if (!is_valid_path(file_name)) {
    fprintf(stderr, "file_name is invalid, %s\n", file_name);
    uint8_t resp = INVALIDNAME;
    if (write(socket_fd, &resp, sizeof(uint8_t)) < 0) {
      perror("failed to write()\n");
    }
    return -1;
  }
  uint8_t ack = ACK;
  if (write(socket_fd, &ack, sizeof(uint8_t)) < 0) {
    perror("faied to write()\n");
    return -1;
  }

  uint8_t resp;
  if (recv_file(socket_fd, file_name) == 0) {
    resp = ACK;
  } else {
    resp = FAILED;
  };

  if (write(socket_fd, &resp, sizeof(uint8_t)) < 0) {
    perror("failed to write()\n");
    return -1;
  }
  return 0;
}

int handle_server_recv_file(int socket_fd, const char *file_name) {

  file_info file_info;
  printf("client call recv file\n");

  if (!is_valid_path(file_name)) {
    fprintf(stderr, "file_name is invalid, %s\n", file_name);
    uint8_t resp = INVALIDNAME;
    if (write(socket_fd, &resp, sizeof(uint8_t)) < 0) {
      perror("failed to write()\n");
    }
    return -1;
  }
  if (from_file_name_to_file_info(file_name, &file_info) != 0) {
    fprintf(stderr, "failed to get file info\n");
    uint8_t resp = NOTFOUND;
    if (write(socket_fd, &resp, sizeof(uint8_t)) < 0) {
      perror("failed to write()\n");
    }
    return -1;
  }

  if (file_info.file_type == D) {
    fprintf(stderr, "cannot send directory\n");
    uint8_t resp = ISDIR;
    if (write(socket_fd, &resp, sizeof(uint8_t)) < 0) {
      perror("failed to write()\n");
    }
    return -1;
  }

  uint8_t ack = ACK;
  if (write(socket_fd, &ack, sizeof(uint8_t)) < 0) {
    perror("faied to write()\n");
    return -1;
  }

  return send_file(socket_fd, &file_info);
}

int handle_server_get_contents(int socket_fd, const char *file_name) {
  printf("client call get contents\n");
  if (!is_valid_path(file_name)) {
    fprintf(stderr, "file_name is invalid, %s\n", file_name);
    uint8_t resp = INVALIDNAME;
    if (write(socket_fd, &resp, sizeof(uint8_t)) < 0) {
      perror("failed to write()\n");
    }
    return -1;
  }

  DIR *dir = opendir(file_name);

  if (!dir) {
    perror("failed to opendir()\n");
    uint8_t resp = ISNOTDIR;
    if (write(socket_fd, &resp, sizeof(uint8_t)) < 0) {
      perror("failed to write()\n");
    }
    return -1;
  }

  uint8_t ack = ACK;
  if (write(socket_fd, &ack, sizeof(uint8_t)) < 0) {
    perror("faied to write()\n");
    return -1;
  }

  server_get_contents(socket_fd, dir);
  printf("finish get contents\n");
  return 0;
}

void recv_cmd(int socket_fd, struct transfer_cmd *cmd) {
  uint8_t buffer[sizeof(cmd->cmd) + sizeof(cmd->file_path)];

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

  memcpy(cmd->file_path, buffer + sizeof(cmd->cmd), sizeof(cmd->file_path));
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
      handle_server_send_file(client_sock, cmd.file_path);
      break;
    case RECV_FILE:
      handle_server_recv_file(client_sock, cmd.file_path);
      break;
    case GET_CONTENTS:
      handle_server_get_contents(client_sock, cmd.file_path);
      break;
    case MKDIR:
      handle_server_mkdir(client_sock, cmd.file_path);
      break;
    case RMDIR:
      handle_server_rmdir(client_sock, cmd.file_path);
      break;
    case REMOVE:
      handle_server_remove(client_sock, cmd.file_path);
      break;
    default:
      fprintf(stderr, "unknown command");
      uint8_t resp = DENY;
      write(client_sock, &resp, sizeof(uint8_t));
    }
  }
  return 0;
}
