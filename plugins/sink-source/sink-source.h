#ifndef SINK_SOURCE_H
#define SINK_SOURCE_H

#include <obs-module.h>
#include <stdatomic.h>

struct sink_source {
	obs_source_t *source;
	uint32_t width;
	uint32_t height;
	uint8_t *img_data;
	gs_texture_t *texture;
	uint8_t *read_buffer;
	uint32_t read_buffer_data_size;
	atomic_bool decoding_image;
	atomic_bool image_decoded;
	int server_fd;
	pthread_t listener_thread;
	pthread_t socket_listener_thread;
	atomic_bool stop_signal;
};

#endif // SINK_SOURCE_H
