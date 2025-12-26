#pragma once
#include <string>
#include <vector>

enum class TokKind {
    Word,
    Pipe, Amp,
    Lt, Gt, GtGt,
};

struct Tok {
    TokKind kind{};
    std::string text;
};

std::vector<Tok> tokenize(const std::string& line);
