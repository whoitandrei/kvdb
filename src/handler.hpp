#pragma once

#include "socket.hpp"
#include "store.hpp"
#include "protocol.hpp"

void handle_connection(Socket socket, Store& store);