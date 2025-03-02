#include "common.h"
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <unistd.h>

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

bool is_valid_name(const char *file_name) {
  if (strchr(file_name, '/') != NULL) {
    return false;
  }

  if (strcmp(file_name, "..") == 0) {
    return false;
  }

  return true;
}

void print_file_meta(const file_meta *file_info) {
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
  }
  return -1;
}

int recv_file_meta(int socket_fd, file_meta *file_info) {
  char buffer[sizeof(file_info->file_type) + sizeof(file_info->file_size) +
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
  memcpy(&file_info->file_type, buffer, sizeof(file_info->file_type));

  uint64_t file_size;
  memcpy(&file_size, buffer + sizeof(file_info->file_type),
         sizeof(file_info->file_size));
  file_info->file_size = ntohll(file_size);
  memcpy(file_info->file_name,
         buffer + sizeof(file_info->file_type) + sizeof(file_info->file_size),
         sizeof(file_info->file_name));
  return 0;
}

int send_file_meta(int socket_fd, const file_meta *file_info) {
  char buffer[sizeof(file_info->file_type) + sizeof(file_info->file_size) +
              sizeof(file_info->file_name)];

  uint64_t file_size = htonll(file_info->file_size);
  ssize_t ret;
  memcpy(buffer, &file_info->file_type, sizeof(file_info->file_type));
  memcpy(buffer + sizeof(file_info->file_type), &file_size, sizeof(file_size));
  memcpy(buffer + sizeof(file_info->file_type) + sizeof(file_size),
         &file_info->file_name, sizeof(file_info->file_name));
  ret = write(socket_fd, buffer, sizeof(buffer));
  if (ret != sizeof(buffer)) {
    perror("write error\n");
    return -1;
  }
  return 0;
}

int from_file_name_to_file_meta(const char *file_name, file_meta *file_info) {
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
  file_meta file_info;
  if (recv_file_meta(socket_fd, &file_info) != 0) {
    fprintf(stderr, "failed to recv file info");
    return -1;
  }

  print_file_meta(&file_info);

  FILE *fp;
  int read_result;

  uint64_t file_sz_rem = file_info.file_size;

  if ((fp = fopen(file_info.file_name, "w")) == NULL) {
    perror("failed fopen()\n");
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

int send_file(int socket_fd, const file_meta *file_info) {

  print_file_meta(file_info);

  if (file_info->file_type == D) {
    fprintf(stderr, "cannot send directory\n");
    return -1;
  }

  if (send_file_meta(socket_fd, file_info) != 0) {
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

  while (offset < file_info->file_size) {
    bytes = sendfile(socket_fd, file_fd, &offset, file_info->file_size - offset);
    if (bytes <= 0) {
      perror("failed to sendfile()\n");
      close(file_fd);
      return -1;
    }
  }
  printf("finish send file\n");

  close(file_fd);

  return 0;
}
