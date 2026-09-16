#pragma once

#include "executor.hpp"
#include "socket.hpp"

void handle_connection(Socket socket, ServerContext ctx);