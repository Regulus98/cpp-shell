#include "tokenizer.hpp"
#include <stdexcept>

static bool is_space(char c) { return c==' ' || c=='\t' || c=='\n'; }

std::vector<Tok> tokenize(const std::string& line) {
    std::vector<Tok> out;
    std::string cur;

    auto flush_word = [&](){
        if (!cur.empty()) {
            out.push_back({TokKind::Word, cur});
            cur.clear();
        }
    };

    enum class Q { None, Single, Double };
    Q q = Q::None;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];

        if (q == Q::None) {
            if (is_space(c)) { flush_word(); continue; }

            if (c == '\'') { q = Q::Single; continue; }
            if (c == '"')  { q = Q::Double; continue; }

            if (c == '\\') {
                if (i + 1 < line.size()) cur.push_back(line[++i]);
                else throw std::runtime_error("dangling escape");
                continue;
            }

            // operators
            if (c == '|') { flush_word(); out.push_back({TokKind::Pipe, "|"}); continue; }
            if (c == '&') { flush_word(); out.push_back({TokKind::Amp, "&"}); continue; }
            if (c == '<') { flush_word(); out.push_back({TokKind::Lt, "<"}); continue; }
            if (c == '>') {
                flush_word();
                if (i + 1 < line.size() && line[i+1] == '>') {
                    ++i;
                    out.push_back({TokKind::GtGt, ">>"});
                } else {
                    out.push_back({TokKind::Gt, ">"});
                }
                continue;
            }

            cur.push_back(c);
        } else if (q == Q::Single) {
            if (c == '\'') { q = Q::None; continue; }
            cur.push_back(c);
        } else { // double
            if (c == '"') { q = Q::None; continue; }
            if (c == '\\') {
                if (i + 1 < line.size()) cur.push_back(line[++i]);
                else throw std::runtime_error("dangling escape");
                continue;
            }
            cur.push_back(c);
        }
    }

    if (q != Q::None) throw std::runtime_error("unterminated quote");
    flush_word();
    return out;
}
