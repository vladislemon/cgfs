#include "file.h"
#include <stdio.h>
#include <errno.h>

int file_read_all(const char *modes, const char *path, usize *length, u8 *data) {
    FILE *file = fopen(path, modes);
    if (file == NULL) {
        return errno;
    }
    fseek(file, 0L, SEEK_END);
    *length = ftell(file);
    if (data == NULL) {
        return 0;
    }
    fseek(file, 0L, SEEK_SET);
    size_t read = fread(data, 1, *length, file);
    if (read != *length) {
        if (feof(file)) {
            return 1;
        }
        if (ferror(file)) {
            return errno;
        }
    }
    return 0;
}

int file_read_all_binary(const char *path, usize *length, u8 *data) {
    return file_read_all("rb", path, length, data);
}

int file_read_all_text(const char *path, usize *length, u8 *data) {
    return file_read_all("r", path, length, data);
}
