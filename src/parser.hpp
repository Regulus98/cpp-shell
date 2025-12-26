#pragma once
#include "exec.hpp"
#include "tokenizer.hpp"
#include <vector>

Pipeline parse_pipeline(const std::vector<Tok>& toks);
