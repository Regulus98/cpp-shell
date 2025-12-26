#pragma once
#include <string>
#include <system_error>

namespace sys {

[[noreturn]] void throw_errno(const char* what);

int  open_read(const std::string& path);
int  open_write_trunc(const std::string& path);
int  open_write_append(const std::string& path);

void set_cloexec(int fd);

} // namespace sys
