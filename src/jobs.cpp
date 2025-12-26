#include "jobs.hpp"

static Jobs g_jobs;

Jobs& jobs() { return g_jobs; }

int Jobs::add_job(pid_t pgid, std::string cmdline, std::vector<pid_t> pids) {
    Job j;
    j.id = next_id_++;
    j.pgid = pgid;
    j.cmdline = std::move(cmdline);
    j.running = true;
    for (pid_t p : pids) j.procs.push_back(JobProcess{p});
    jobs_.push_back(std::move(j));
    return jobs_.back().id;
}

void Jobs::mark_done(pid_t /*pid*/, int /*status*/) {
    // TODO: Track all child statuses and mark job finished only when all pids exit.
}

std::optional<Job> Jobs::find_by_id(int id) const {
    for (auto const& j : jobs_) if (j.id == id) return j;
    return std::nullopt;
}

std::optional<Job> Jobs::find_by_pgid(pid_t pgid) const {
    for (auto const& j : jobs_) if (j.pgid == pgid) return j;
    return std::nullopt;
}

std::vector<Job> Jobs::list() const { return jobs_; }

bool Jobs::set_foreground(int /*id*/) {
    return false;
}
