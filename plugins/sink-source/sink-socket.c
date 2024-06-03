#include <obs-module.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <util/threading.h>
#include "sink-source.h"


#define BUFFER_SIZE 64 * 1024
#define SOCKET_PATH "/tmp/sinksource.sock"

uint8_t* read_file_to_memory(const char* filepath, uint32_t* out_size);


void* sink_socket_listener(void* arg) {
    struct sink_source *context = arg;

    uint8_t* write_buffer = bzalloc(BUFFER_SIZE);
    uint32_t bytes_in_buffer = 0;

    int client_fd;
    struct sockaddr_un client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // Set the server socket to non-blocking mode
    fcntl(context->server_fd, F_SETFL, O_NONBLOCK);

    while (!atomic_load(&context->stop_signal)) {
        client_fd = accept(context->server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_fd == -1) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                // No incoming connection, sleep for a short duration
                usleep(1000); // 1 ms
                continue;
            } else {
                fprintf(stderr, "Failed to accept connection\n");
                continue;
            }
        }

        while (!atomic_load(&context->stop_signal)) {
            ssize_t bytes_received = recv(client_fd, write_buffer + bytes_in_buffer, BUFFER_SIZE - bytes_in_buffer, 0);
            if (bytes_received <= 0) {
                if (bytes_received == 0) {
                    fprintf(stdout, "Client disconnected\n");
                } else {
                    fprintf(stderr, "recv failed\n");
                }
                break;
            }

            bytes_in_buffer += bytes_received;

            // TODO: how do we know if the buffer contains a full image?
            if (bytes_in_buffer == 66666) {
                // Wait if the main thread is still decoding the previous image
                while (atomic_load(&context->decoding_image)) {
                    usleep(1000); // 1 ms
                }
                // Swap the read and write buffers
                uint8_t *tmp = context->read_buffer;
                context->read_buffer = write_buffer;
                context->read_buffer_data_size = bytes_in_buffer;
                write_buffer = tmp;
                bytes_in_buffer = 0;

                // Signal the main thread to start decoding the image
                atomic_store(&context->image_decoded, false);
            }
        }

        close(client_fd);
    }
    bfree(write_buffer);

    return NULL;
}

int init_sink_thread(struct sink_source *context) {
    context->server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (context->server_fd == -1) {
        fprintf(stderr, "Failed to create socket\n");
        return 1;
    }

    struct sockaddr_un server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);
    unlink(SOCKET_PATH); // Remove the socket file if it already exists

    if (bind(context->server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        fprintf(stderr, "Failed to bind socket\n");
        close(context->server_fd);
        return 1;
    }

    if (listen(context->server_fd, 5) == -1) {
        fprintf(stderr, "Failed to listen on socket\n");
        close(context->server_fd);
        return 1;
    }

    context->read_buffer = bzalloc(BUFFER_SIZE);
    context->read_buffer_data_size = 0;
    context->image_decoded = true;
    context->decoding_image = false;

    // TODO: Temp code to load an image into the buffer from disk.
    uint32_t buf_size;
    uint8_t *imagebuffer = read_file_to_memory("/Users/emoller/Downloads/opera_gx.jpg", &buf_size);
    if (imagebuffer) {
        memcpy(context->read_buffer, imagebuffer, buf_size);
        context->read_buffer_data_size = buf_size;
        bfree(imagebuffer);
        context->image_decoded = false;
    }

    if (pthread_create(&context->listener_thread, NULL, sink_socket_listener, context) != 0) {
        fprintf(stderr, "Failed to create listener thread\n");
        close(context->server_fd);
        bfree(context->read_buffer);
        return 1;
    }
    
    return 0;
}

int join_sink_thread(struct sink_source *context) {
    // Signal the listener thread to stop
    atomic_store(&context->stop_signal, true);
    pthread_join(context->listener_thread, NULL);

    bfree(context->read_buffer);
    close(context->server_fd);
    unlink(SOCKET_PATH);

    return 0;
}
