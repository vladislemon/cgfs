#ifndef CGFS_FILE_H
#define CGFS_FILE_H

#include "types.h"

int file_read_all_binary(const char *path, usize *length, u8 *data);

int file_read_all_text(const char *path, usize *length, u8 *data);

#endif //CGFS_FILE_H
