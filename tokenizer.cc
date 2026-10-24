#include "tokenizer.h"
#include <cctype>

std::vector<Token> tokenize(const std::string &input)
{
    std::vector<Token> tokens;
    std::string word;
    char quote = 0;

    for (int i = 0; i < input.size(); i++)
    {
        char c = input[i];

        if (quote)
        {
            if (c == quote) quote = 0;
            else word += c;
            continue;
        }

        if (c == '"' || c == '\'')
        {
            quote = c;
            continue;
        }

        if (isspace(c))
        {
            if (!word.empty())
            {
                TokenType type = tokens.empty() || tokens.back().type == TOKEN_PIPE
                    ? TOKEN_COMMAND : TOKEN_ARGUMENT;
                tokens.push_back({type, word});
                word.clear();
            }
            continue;
        }

        std::string op;
        if (input.compare(i, 3, ">>&") == 0) op = ">>&";
        else if (input.compare(i, 2, "2>") == 0) op = "2>";
        else if (input.compare(i, 2, ">>") == 0) op = ">>";
        else if (c == '>' || c == '<' || c == '|' || c == '&') op = c;

        if (!op.empty())
        {
            if (!word.empty())
            {
                TokenType type = tokens.empty() || tokens.back().type == TOKEN_PIPE
                    ? TOKEN_COMMAND : TOKEN_ARGUMENT;
                tokens.push_back({type, word});
                word.clear();
            }

            TokenType type;
            if (op == ">") type = TOKEN_REDIRECT;
            else if (op == ">>") type = TOKEN_APPEND;
            else if (op == "<") type = TOKEN_INPUT;
            else if (op == "2>") type = TOKEN_ERROR;
            else if (op == ">>&") type = TOKEN_REDIRECT_AND_ERROR;
            else if (op == "|") type = TOKEN_PIPE;
            else type = TOKEN_BACKGROUND;

            tokens.push_back({type, op});
            i += op.size() - 1;
        }
        else word += c;
    }

    if (!word.empty())
    {
        TokenType type = tokens.empty() || tokens.back().type == TOKEN_PIPE
            ? TOKEN_COMMAND : TOKEN_ARGUMENT;
        tokens.push_back({type, word});
    }

    tokens.push_back({TOKEN_EOF, ""});
    return tokens;
}