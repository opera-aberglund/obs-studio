#ifndef SINK_SOCKET_H
#define SINK_SOCKET_H

#include "sink-source.hpp"

int init_sink_thread(sink_source *context);
int init_socket_thread(sink_source *context);
int join_sink_thread(sink_source *context);

#endif // SINK_SOCKET_H
