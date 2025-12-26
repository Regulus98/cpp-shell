#include "parser.hpp"
#include <stdexcept>

static bool is_word(const Tok& t) { return t.kind == TokKind::Word; }

Pipeline parse_pipeline(const std::vector<Tok>& toks) {
    Pipeline pl;
    Command cur;

    auto finish_cmd = [&](){
        if (cur.argv.empty()) throw std::runtime_error("empty command");
        pl.cmds.push_back(std::move(cur));
        cur = Command{};
    };

    for (size_t i = 0; i < toks.size(); ++i) {
        const Tok& t = toks[i];

        switch (t.kind) {
        case TokKind::Word:
            cur.argv.push_back(t.text);
            break;

        case TokKind::Pipe:
            finish_cmd();
            break;

        case TokKind::Amp:
            if (i != toks.size() - 1) throw std::runtime_error("& must be at end");
            pl.background = true;
            break;

        case TokKind::Lt:
            if (i + 1 >= toks.size() || !is_word(toks[i+1]))
                throw std::runtime_error("expected file after <");
            cur.redirs.push_back({Redir::Kind::In, toks[i+1].text});
            ++i;
            break;

        case TokKind::Gt:
            if (i + 1 >= toks.size() || !is_word(toks[i+1]))
                throw std::runtime_error("expected file after >");
            cur.redirs.push_back({Redir::Kind::OutTrunc, toks[i+1].text});
            ++i;
            break;

        case TokKind::GtGt:
            if (i + 1 >= toks.size() || !is_word(toks[i+1]))
                throw std::runtime_error("expected file after >>");
            cur.redirs.push_back({Redir::Kind::OutAppend, toks[i+1].text});
            ++i;
            break;
        }
    }

    if (!cur.argv.empty()) pl.cmds.push_back(std::move(cur));
    if (pl.cmds.empty()) throw std::runtime_error("no command");
    return pl;
}
