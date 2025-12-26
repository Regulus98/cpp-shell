#pragma once
#include <string>

class Shell {
public:
    int run();

private:
    void install_signal_handlers();
    void reap_background();

    // Returns true if builtin handled (or empty line). False if not a builtin.
    bool handle_builtin(const std::string& line);

    std::string prompt() const;
};
