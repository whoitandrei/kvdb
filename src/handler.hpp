#pragma once

#include "socket.hpp"
#include "store.hpp"
#include "protocol.hpp"
#include "wal.hpp"

void handle_connection(Socket socket, Store& store, Wal& wal);