#include "sys.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace sys {

[[noreturn]] void throw_errno(const char* what) {
    throw std::system_error(errno, std::generic_category(),
                            std::string(what) + ": " + std::strerror(errno));
}

void set_cloexec(int fd) {
    int flags = ::fcntl(fd, F_GETFD);
    if (flags < 0) throw_errno("fcntl(F_GETFD)");
    if (::fcntl(fd, F_SETFD, flags | FD_CLOEXEC) < 0) throw_errno("fcntl(F_SETFD)");
}

int open_read(const std::string& path) {
    int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) throw_errno("open(read)");
    set_cloexec(fd);
    return fd;
}

int open_write_trunc(const std::string& path) {
    int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) throw_errno("open(write_trunc)");
    set_cloexec(fd);
    return fd;
}

int open_write_append(const std::string& path) {
    int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) throw_errno("open(write_append)");
    set_cloexec(fd);
    return fd;
}

} // namespace sys
