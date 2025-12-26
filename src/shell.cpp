#include "shell.hpp"
#include "tokenizer.hpp"
#include "parser.hpp"
#include "exec.hpp"
#include "jobs.hpp"

#include <iostream>
#include <sstream>
#include <vector>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>
#include <cstdlib>

#include <readline/readline.h>
#include <readline/history.h>
#include <dirent.h>
#include <cstring>
#include <libgen.h>

static volatile sig_atomic_t g_sigchld = 0;

static void on_sigchld(int) { g_sigchld = 1; }
static void print_welcome();
static std::string history_file_path();

static char** completion(const char* text, int start, int end);
static char* command_generator(const char* text, int state);

void Shell::install_signal_handlers() {
    // Ignore Ctrl-C / Ctrl-\ at the shell prompt.
    ::signal(SIGINT, SIG_IGN);
    ::signal(SIGQUIT, SIG_IGN);

    struct sigaction sa{};
    sa.sa_handler = on_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, nullptr);
}

std::string Shell::prompt() const {
    char cwd[PATH_MAX];

    // readline needs non-printing sequences wrapped in \001 and \002
    const std::string BLUE  = "\001\033[1;34m\002";  // bold blue
    const std::string RESET = "\001\033[0m\002";

    if (::getcwd(cwd, sizeof(cwd))) {
        return BLUE + std::string(cwd) + RESET + " $ ";
    }

    return BLUE + "$" + RESET + " ";
}

void Shell::reap_background() {
    if (!g_sigchld) return;
    g_sigchld = 0;

    int status = 0;
    while (true) {
        pid_t pid = ::waitpid(-1, &status, WNOHANG);
        if (pid <= 0) break;
        jobs().mark_done(pid, status);
    }
}

static std::vector<std::string> split_ws(const std::string& s) {
    std::istringstream iss(s);
    std::vector<std::string> out;
    for (std::string w; iss >> w; ) out.push_back(w);
    return out;
}

bool Shell::handle_builtin(const std::string& line) {
    auto parts = split_ws(line);
    if (parts.empty()) return true;

    const auto& cmd = parts[0];

    if (cmd == "exit") {
        std::exit(0);
    }

    if (cmd == "pwd") {
        char cwd[PATH_MAX];
        if (::getcwd(cwd, sizeof(cwd))) std::cout << cwd << "\n";
        else std::perror("getcwd");
        return true;
    }

    if (cmd == "cd") {
        const char* path = (parts.size() >= 2) ? parts[1].c_str() : std::getenv("HOME");
        if (!path) path = "/";
        if (::chdir(path) < 0) std::perror("cd");
        return true;
    }

    if (cmd == "export") {
        // export KEY=VALUE
        if (parts.size() < 2) { std::cerr << "usage: export KEY=VALUE\n"; return true; }
        auto pos = parts[1].find('=');
        if (pos == std::string::npos) { std::cerr << "export expects KEY=VALUE\n"; return true; }
        std::string k = parts[1].substr(0, pos);
        std::string v = parts[1].substr(pos + 1);
        if (::setenv(k.c_str(), v.c_str(), 1) < 0) std::perror("setenv");
        return true;
    }

    if (cmd == "unset") {
        if (parts.size() < 2) { std::cerr << "usage: unset KEY\n"; return true; }
        if (::unsetenv(parts[1].c_str()) < 0) std::perror("unsetenv");
        return true;
    }

    if (cmd == "jobs") {
        for (auto const& j : jobs().list()) {
            std::cout << "[" << j.id << "] " << (int)j.pgid << "  " << j.cmdline << "\n";
        }
        return true;
    }

    // Not a builtin
    return false;
}

static void print_welcome() {
    constexpr const char* BLUE   = "\033[1;34m";
    constexpr const char* GREEN  = "\033[1;32m";
    constexpr const char* RESET  = "\033[0m";

    std::cout << BLUE <<
R"( 
        ===========================================================================

             ██████╗ ██████╗ ██████╗     ███████╗██╗  ██╗███████╗██╗     ██╗     
            ██╔════╝ ██╔══██╗██╔══██╗    ██╔════╝██║  ██║██╔════╝██║     ██║     
            ██║      ██████╔╝██████╔╝    ███████╗███████║█████╗  ██║     ██║     
            ██║      ██╔═══╝ ██╔═══╝     ╚════██║██╔══██║██╔══╝  ██║     ██║     
            ╚██████╗ ██║     ██║         ███████║██║  ██║███████╗███████╗███████╗
             ╚═════╝ ╚═╝     ╚═╝         ╚══════╝╚═╝  ╚═╝╚══════╝╚══════╝╚══════╝

)" << GREEN <<
R"(                                by Regulus98
)" << BLUE <<
R"(
        ===========================================================================
)" << RESET <<  "\n\n";
}

static std::string history_file_path() {
    char exe_path[PATH_MAX];

    // Get full path of running executable
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len == -1) {
        std::cerr << "[history] ERROR: readlink(/proc/self/exe) failed: "
                  << std::strerror(errno) << "\n";
        std::cerr << "[history] Falling back to local .cppshell_history\n";
        // fallback: current directory
        return ".cppshell_history";
    }
    exe_path[len] = '\0';

    // exe_path = ~/Desktop/cppshell/build/cppshell
    char* build_dir = dirname(exe_path);  // modifies exe_path
    if (!build_dir) {
        std::cerr << "[history] ERROR: dirname(build) failed\n";
        return ".cppshell_history";
    }

    // Keep history alongside the built binary
    std::string history_path = std::string(build_dir) + "/.cppshell_history";

    // Success log
    std::cout << "[history] History file path resolved: "
              << history_path << "\n";

    return history_path;
}


int Shell::run() {
    // Install shell-level signal handlers
    install_signal_handlers();

    // Enable TAB completion via readline
    rl_attempted_completion_function = completion;

    // Initialize readline history subsystem
    using_history();

    // Persistent history
    const std::string hist_file = history_file_path();
    read_history(hist_file.c_str()); // ignore errors if file doesn't exist

    // Welcome banner
    print_welcome();

    while (true) {
        reap_background();

        // Read input using readline (REQUIRED for TAB support)
        char* input = readline(prompt().c_str());
        if (!input) {
            // Ctrl-D (EOF)
            std::cout << "\n";
            break;
        }

        std::string line(input);
        free(input);

        // Ignore empty input
        if (line.find_first_not_of(" \t") == std::string::npos)
            continue;

        // Save command to history
        add_history(line.c_str());

        // Write history incrementally
        append_history(1, hist_file.c_str());

        try {
            // Quick builtin path (simple whitespace split only).
            // NOTE: builtins inside pipelines will be a later milestone.
            if (handle_builtin(line)) continue;

            // Tokenize → parse → execute
            auto tokens  = tokenize(line);
            auto pipeline = parse_pipeline(tokens);
            execute_pipeline(pipeline);

        } catch (const std::exception& e) {
            std::cerr << "error: " << e.what() << "\n";
        }
    }

    // Ensure full write on exit (safe)
    write_history(hist_file.c_str());

    return 0;
}

static char** completion(const char* text, int start, int /*end*/) {
    // First word → command completion
    if (start == 0) {
        return rl_completion_matches(text, command_generator);
    }

    // Other positions → filename completion
    return rl_completion_matches(text, rl_filename_completion_function);
}

static char* command_generator(const char* text, int state) {
    static std::vector<std::string> matches;
    static size_t index;

    if (state == 0) {
        matches.clear();
        index = 0;

        // Built-in commands
        const char* builtins[] = {
            "cd", "exit", "pwd", "export", "unset", "jobs"
        };

        for (auto b : builtins) {
            if (std::strncmp(b, text, std::strlen(text)) == 0)
                matches.emplace_back(b);
        }

        // PATH executables
        const char* path = std::getenv("PATH");
        if (path) {
            std::string p(path);
            size_t pos = 0;

            while ((pos = p.find(':')) != std::string::npos) {
                std::string dir = p.substr(0, pos);
                p.erase(0, pos + 1);

                if (DIR* d = opendir(dir.c_str())) {
                    while (auto* ent = readdir(d)) {
                        if (std::strncmp(ent->d_name, text, std::strlen(text)) == 0)
                            matches.emplace_back(ent->d_name);
                    }
                    closedir(d);
                }
            }
        }
    }

    if (index < matches.size())
        return strdup(matches[index++].c_str());

    return nullptr;
}
