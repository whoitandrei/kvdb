#pragma once
#include <system_error>

#define GREEN "\033[32m"
#define RED   "\033[31m"
#define RESET "\033[0m"

#define ERR_RETURN -1
#define SUCCESS_RETURN 0

[[noreturn]] void throw_errno(const char* what) {
    throw std::system_error(errno, std::system_category(), what);
}