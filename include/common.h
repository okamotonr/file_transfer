#ifndef SERVER_PROT
#define SERVER_PROT

#include <stdbool.h>
#include <stdint.h>

#define PORT 2121

#define SEND_FILE 0
#define RECV_FILE 1
#define GET_CONTENTS 2

#define ACK 40

#define ISNOTDIR 49
#define ISDIR 50
#define NOTFOUND 51
#define INVALIDNAME 52
#define DENY 255
#define MAX_FILE_NAME 4096 // same as path_max

#define F 0
#define D 1

typedef struct transfer_cmd {
  char cmd;
  char file_name[MAX_FILE_NAME];
} transfer_cmd;

typedef struct file_info {
  char file_type;
  uint64_t file_size;
  char file_name[MAX_FILE_NAME];
} file_info;

void print_transfer_cmd(const transfer_cmd *cmd);
int parse_command(const char *command_name);
int send_file(int socket_fd, const file_info *file_info);
int recv_file(int socket_fd, const char *file_name);
int recv_file_meta(int socket_fd, file_info *file_info);
int send_file_meta(int socket_fd, const file_info *file_info);
int from_file_name_to_file_info(const char *file_name, file_info *file_info);
bool is_valid_name(const char *file_name);

#endif
