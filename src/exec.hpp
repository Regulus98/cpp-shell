#pragma once
#include <string>
#include <vector>

struct Redir {
    enum class Kind { In, OutTrunc, OutAppend };
    Kind kind{};
    std::string path;
};

struct Command {
    std::vector<std::string> argv;
    std::vector<Redir> redirs;
};

struct Pipeline {
    std::vector<Command> cmds;
    bool background{false};
};

struct ExecResult {
    int exit_code{0};
    bool started_background{false};
    int job_id{-1};
};

ExecResult execute_pipeline(const Pipeline& pl);
