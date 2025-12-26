#pragma once
#include <string>
#include <vector>
#include <optional>
#include <sys/types.h>

struct JobProcess { pid_t pid{}; };

struct Job {
    int id{};
    pid_t pgid{};
    std::string cmdline;
    bool running{true};
    std::vector<JobProcess> procs;
};

class Jobs {
public:
    int add_job(pid_t pgid, std::string cmdline, std::vector<pid_t> pids);
    void mark_done(pid_t pid, int status);
    std::optional<Job> find_by_id(int id) const;
    std::optional<Job> find_by_pgid(pid_t pgid) const;
    std::vector<Job> list() const;

    bool set_foreground(int id); // skeleton

private:
    int next_id_{1};
    std::vector<Job> jobs_;
};

Jobs& jobs();
