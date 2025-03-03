#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common.h"
uint64_t htonll(uint64_t host_value) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
  uint32_t high_part = htonl((uint32_t)(host_value >> 32));
  uint32_t low_part = htonl((uint32_t)(host_value & 0xFFFFFFFFULL));
  return (((uint64_t)low_part) << 32) | high_part;
#else
  return host_value;
#endif
}

uint64_t ntohll(uint64_t net_value) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
  uint32_t high_part = ntohl((uint32_t)(net_value >> 32));
  uint32_t low_part = ntohl((uint32_t)(net_value & 0xFFFFFFFFULL));
  return (((uint64_t)low_part) << 32) | high_part;
#else
  return net_value;
#endif
}

const char *action_to_string(int action) {
  switch (action) {
  case SEND_FILE:
    return "SEND";
  case RECV_FILE:
    return "RECV";
  case GET_CONTENTS:
    return "DIR";
  case MKDIR:
    return "MKDIR";
  case RMDIR:
    return "RMDIR";
  case REMOVE:
    return "REMOVE";
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
  printf("  File Name : %s\n", tcmd->file_path);
}

size_t file_info_ser_size() {
  size_t ret = (sizeof(((file_info *)0)->file_type) +
                sizeof(((file_info *)0)->file_size) +
                sizeof(((file_info *)0)->file_name));
  return ret;
}

size_t serialize_file_info(const file_info *info, uint8_t *buffer) {
  size_t offset = 0;
  buffer[offset] = info->file_type;
  offset += sizeof(info->file_type);

  uint64_t net_size = htonll(info->file_size);
  memcpy(buffer + offset, &net_size, sizeof(info->file_size));
  offset += sizeof(info->file_size);

  memset(buffer + offset, 0, MAX_NAME);
  strncpy((char *)(buffer + offset), info->file_name, MAX_NAME - 1);
  offset += sizeof(info->file_name);

  return offset;
}

size_t deserialize_file_info(file_info *info, const uint8_t *buffer) {
  size_t offset = 0;
  info->file_type = buffer[offset];
  offset += sizeof(info->file_type);

  uint64_t net_size;
  memcpy(&net_size, buffer + offset, sizeof(net_size));
  info->file_size = ntohll(net_size);
  offset += sizeof(net_size);

  memset(info->file_name, 0, MAX_NAME);
  strncpy(info->file_name, (char *)(buffer + offset), MAX_NAME - 1);
  offset += sizeof(info->file_name);

  return offset;
}

bool is_valid_path(const char *file_name) {
  size_t len = strlen(file_name);

  if (len >= 4 && strstr(file_name, "/../") != NULL) {
    return false;
  }

  if (len >= 2 && strncmp(file_name, "..", 2) == 0) {
    if (len == 2) {
      return false;
    }
    if (file_name[2] == '/') {
      return false;
    }
    return true;
  }

  if (len >= 3 && strcmp(file_name + len - 3, "/..") == 0) {
    return false;
  }

  return true;
}

void print_file_info(const file_info *file_info) {
  printf("File Info:\n");
  printf("  File Type: %s\n", file_info->file_type == F ? "File" : "Dir");
  printf("  File Size: %lu\n", file_info->file_size);
  printf("  File Name: %s\n", file_info->file_name);
}

int parse_command(const char *command_name) {
  if (strcmp(command_name, "send") == 0) {
    return SEND_FILE;
  } else if (strcmp(command_name, "recv") == 0) {
    return RECV_FILE;
  } else if (strcmp(command_name, "dir") == 0) {
    return GET_CONTENTS;
  } else if (strcmp(command_name, "mkdir") == 0) {
    return MKDIR;
  } else if (strcmp(command_name, "rmdir") == 0) {
    return RMDIR;
  } else if (strcmp(command_name, "remove") == 0) {
    return REMOVE;
  }

  return -1;
}

int recv_file_info(int socket_fd, file_info *file_info) {
  uint8_t buffer[sizeof(file_info->file_type) + sizeof(file_info->file_size) +
                 sizeof(file_info->file_name)];
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

  deserialize_file_info(file_info, buffer);
  return 0;
}

int send_file_info(int socket_fd, const file_info *file_info) {

  uint8_t buffer[sizeof(file_info->file_type) + sizeof(file_info->file_size) +
                 sizeof(file_info->file_name)];

  serialize_file_info(file_info, buffer);

  ssize_t ret;
  ret = write(socket_fd, buffer, sizeof(buffer));
  if (ret != sizeof(buffer)) {
    perror("write error\n");
    return -1;
  }
  return 0;
}

int from_file_name_to_file_info(const char *file_name, file_info *file_info) {
  struct stat st;
  if (stat(file_name, &st) != 0) {
    perror("failed to stat()\n");
    return -1;
  }
  if (S_ISREG(st.st_mode)) {
    file_info->file_type = F;
  } else if (S_ISDIR(st.st_mode)) {
    file_info->file_type = D;
  } else {
    fprintf(stderr, "unknown file type");
    return -1;
  }

  memcpy(file_info->file_name, file_name, sizeof(file_info->file_name));
  if (st.st_size < 0) {
    fprintf(stderr, "st_size is under zero");
    return -1;
  }
  file_info->file_size = (uint64_t)st.st_size;

  return 0;
}

int recv_file(int socket_fd, const char *file_name) {
  char buffer[4096];
  file_info file_info;
  if (recv_file_info(socket_fd, &file_info) != 0) {
    fprintf(stderr, "failed to recv file info");
    return -1;
  }

  print_file_info(&file_info);

  FILE *fp;
  int read_result;

  uint64_t file_sz_rem = file_info.file_size;

  if ((fp = fopen(file_name, "w")) == NULL) {
    perror("failed fopen() %s\n");
    return -1;
  }

  while (file_sz_rem > 0) {
    read_result = read(socket_fd, &buffer, sizeof(buffer));
    if (read_result == 0) {
      break;
    } else if (read_result < 0) {
      perror("failed to read() on socket");
      fclose(fp);
      return errno;
    } else {
      fwrite(&buffer, sizeof(char), read_result, fp);
      file_sz_rem -= read_result;
    }
  }

  printf("finish recv file\n");
  fclose(fp);

  return 0;
}

int send_file(int socket_fd, const file_info *file_info) {

  print_file_info(file_info);

  if (file_info->file_type == D) {
    fprintf(stderr, "cannot send directory\n");
    return -1;
  }

  if (send_file_info(socket_fd, file_info) != 0) {
    fprintf(stderr, "failed to send file information\n");
    return -1;
  }

  int file_fd = open(file_info->file_name, O_RDONLY);
  if (file_fd < 0) {
    perror("failed to open()\n");
    return -1;
  }

  off_t offset = 0;
  ssize_t bytes = 0;

  while ((uint64_t)offset < file_info->file_size) {
    bytes =
        sendfile(socket_fd, file_fd, &offset, file_info->file_size - offset);
    if (bytes <= 0) {
      perror("failed to sendfile()\n");
      close(file_fd);
      return -1;
    }
    if (offset < 0) {
      fprintf(stderr, "something wrong\n");
      close(file_fd);
      return -1;
    }
  }
  printf("finish send file\n");

  close(file_fd);

  return 0;
}
