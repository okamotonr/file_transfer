#ifndef SERVER_PROT
#define SERVER_PROT

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PORT 2121

#define SEND_FILE 0
#define RECV_FILE 1
#define GET_CONTENTS 2
#define MKDIR 3
#define RMDIR 4
#define REMOVE 5

#define ACK 0

#define FAILED 1
#define ISNOTDIR 49
#define ISDIR 50
#define NOTFOUND 51
#define INVALIDNAME 52
#define DENY 255
#define MAX_FILE_PATH 4096 // same as path_max
#define MAX_NAME 255

#define F 0
#define D 1

typedef struct transfer_cmd {
  uint8_t cmd;
  char file_path[MAX_FILE_PATH];
} transfer_cmd;

typedef struct file_info {
  char file_type;
  uint64_t file_size;
  char file_name[MAX_NAME];
} file_info;

void print_transfer_cmd(const transfer_cmd *cmd);
void print_file_info(const file_info *file_info);
int parse_command(const char *command_name);
int send_file(int socket_fd, const file_info *file_info);
int recv_file(int socket_fd, const char *file_name);
int recv_file_info(int socket_fd, file_info *file_info);
int send_file_info(int socket_fd, const file_info *file_info);
int from_file_name_to_file_info(const char *file_name, file_info *file_info);
size_t file_info_ser_size();
size_t serialize_file_info(const file_info *info, uint8_t *buffer);
size_t deserialize_file_info(file_info *info, const uint8_t *buffer);
bool is_valid_path(const char *file_name);
uint64_t htonll(uint64_t host_value);
uint64_t ntohll(uint64_t net_value);

#endif
