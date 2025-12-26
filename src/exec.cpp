#include "exec.hpp"
#include "sys.hpp"
#include "jobs.hpp"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>

namespace {

struct Fd {
    int fd{-1};
    Fd() = default;
    explicit Fd(int f) : fd(f) {}
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    Fd(Fd&& o) noexcept : fd(o.fd) { o.fd = -1; }
    Fd& operator=(Fd&& o) noexcept {
        if (this != &o) { reset(); fd = o.fd; o.fd = -1; }
        return *this;
    }
    ~Fd() { reset(); }
    void reset() { if (fd >= 0) ::close(fd); fd = -1; }
    int get() const { return fd; }
    int release() { int t=fd; fd=-1; return t; }
};

struct Pipe { Fd r, w; };

Pipe make_pipe() {
    int fds[2];
    if (::pipe(fds) < 0) sys::throw_errno("pipe");
    sys::set_cloexec(fds[0]);
    sys::set_cloexec(fds[1]);
    return Pipe{Fd{fds[0]}, Fd{fds[1]}};
}

void apply_redirs(const Command& cmd) {
    for (auto const& r : cmd.redirs) {
        if (r.kind == Redir::Kind::In) {
            Fd fd{sys::open_read(r.path)};
            if (::dup2(fd.get(), STDIN_FILENO) < 0) sys::throw_errno("dup2(<)");
        } else if (r.kind == Redir::Kind::OutTrunc) {
            Fd fd{sys::open_write_trunc(r.path)};
            if (::dup2(fd.get(), STDOUT_FILENO) < 0) sys::throw_errno("dup2(>)");
        } else {
            Fd fd{sys::open_write_append(r.path)};
            if (::dup2(fd.get(), STDOUT_FILENO) < 0) sys::throw_errno("dup2(>>)");
        }
    }
}

std::vector<char*> make_argv(const Command& cmd) {
    std::vector<char*> argv;
    argv.reserve(cmd.argv.size() + 1);
    for (auto const& s : cmd.argv) argv.push_back(const_cast<char*>(s.c_str()));
    argv.push_back(nullptr);
    return argv;
}

void child_reset_signals() {
    ::signal(SIGINT, SIG_DFL);
    ::signal(SIGQUIT, SIG_DFL);
    ::signal(SIGTSTP, SIG_DFL);
    ::signal(SIGTTIN, SIG_DFL);
    ::signal(SIGTTOU, SIG_DFL);
    ::signal(SIGCHLD, SIG_DFL);
}

} // namespace

ExecResult execute_pipeline(const Pipeline& pl) {
    const int n = static_cast<int>(pl.cmds.size());
    std::vector<Pipe> pipes;
    pipes.reserve((n > 1) ? static_cast<size_t>(n - 1) : 0);
    for (int i = 0; i < n - 1; ++i) pipes.push_back(make_pipe());

    pid_t pgid = 0;
    std::vector<pid_t> pids;
    pids.reserve(static_cast<size_t>(n));

    for (int i = 0; i < n; ++i) {
        pid_t pid = ::fork();
        if (pid < 0) sys::throw_errno("fork");

        if (pid == 0) {
            child_reset_signals();

            if (pgid == 0) pgid = ::getpid();
            if (::setpgid(0, pgid) < 0) sys::throw_errno("setpgid(child)");

            if (i > 0) {
                if (::dup2(pipes[i-1].r.get(), STDIN_FILENO) < 0) sys::throw_errno("dup2(pipe in)");
            }
            if (i < n - 1) {
                if (::dup2(pipes[i].w.get(), STDOUT_FILENO) < 0) sys::throw_errno("dup2(pipe out)");
            }

            for (auto& p : pipes) { p.r.reset(); p.w.reset(); }

            apply_redirs(pl.cmds[i]);

            auto argv = make_argv(pl.cmds[i]);
            ::execvp(argv[0], argv.data());
            std::fprintf(stderr, "execvp failed: %s\n", std::strerror(errno));
            _exit(127);
        }

        if (pgid == 0) pgid = pid;
        if (::setpgid(pid, pgid) < 0 && errno != EACCES) {
            sys::throw_errno("setpgid(parent)");
        }
        pids.push_back(pid);
    }

    for (auto& p : pipes) { p.r.reset(); p.w.reset(); }

    ExecResult res;

    if (pl.background) {
        int id = jobs().add_job(pgid, "(background)", pids);
        res.started_background = true;
        res.job_id = id;
        std::printf("[%d] %d\n", id, (int)pgid);
        return res;
    }

    int status = 0;
    int last_exit = 0;
    for (pid_t pid : pids) {
        if (::waitpid(pid, &status, 0) < 0) sys::throw_errno("waitpid");
        if (WIFEXITED(status)) last_exit = WEXITSTATUS(status);
        else if (WIFSIGNALED(status)) last_exit = 128 + WTERMSIG(status);
    }
    res.exit_code = last_exit;
    return res;
}
