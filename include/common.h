#ifndef SERVER_PROT
#define SERVER_PROT

#include <stdbool.h>
#include <stdint.h>

#define PORT 2121

#define SEND_FILE 0
#define RECV_FILE 1
#define GET_CONTENTS 2

#define ACK 40

#define ISDIR 50
#define NOTFOUND 51
#define INVALIDNAME 52
#define DENY 255

#define F 0
#define D 1

typedef struct file_info {
  uint32_t file_size;
  char file_name[100];
} file_info;

typedef struct transfer_cmd {
  char cmd;
  file_info file_info;
} transfer_cmd;

typedef struct file_meta {
  char file_type;
  uint64_t file_size;
  char file_name[100];
} file_meta;

void print_transfer_cmd(const transfer_cmd *cmd);
int parse_command(const char *command_name);
int send_file(int socket_fd, const file_meta *file_info);
int recv_file(int socket_fd, const char *file_name);
int recv_file_meta(int socket_fd, file_meta *file_info);
int send_file_meta(int socket_fd, const file_meta *file_info);
int from_file_name_to_file_meta(const char *file_name, file_meta *file_info);
bool is_valid_name(const char *file_name);

#endif
