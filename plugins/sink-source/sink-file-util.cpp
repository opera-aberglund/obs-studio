#include <obs-module.h>

uint8_t* read_file_to_memory(const char* filepath, uint32_t* out_size) {
    FILE *file = fopen(filepath, "rb");
    uint8_t *buffer = NULL;

    if (!file) {
        fprintf(stderr, "Failed to open file: %s\n", filepath);
        return NULL;
    }

    // Seek to the end of the file to get the file size
    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (filesize <= 0) {
        fprintf(stderr, "Failed to get file size: %s\n", filepath);
        fclose(file);
        return NULL;
    }

    // Allocate memory for the file content
    buffer = (uint8_t*)bzalloc(filesize);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate memory\n");
        fclose(file);
        return NULL;
    }

    // Read the file content into the buffer
    uint32_t read_size = fread(buffer, 1, filesize, file);
    if (read_size != filesize) {
        fprintf(stderr, "Failed to read file content\n");
        bfree(buffer);
        fclose(file);
        return NULL;
    }

    // Close the file
    fclose(file);

    // Set the output size
    *out_size = filesize;

    return buffer;
}
