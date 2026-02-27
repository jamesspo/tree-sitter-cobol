#include "tree_sitter/parser.h"
#include <wctype.h>
#include <string.h>
#include <ctype.h>

enum TokenType {
    WHITE_SPACES,
    LINE_PREFIX_COMMENT,
    LINE_SUFFIX_COMMENT,
    LINE_COMMENT,
    COMMENT_ENTRY,
    multiline_string,
    EXECUTE_BODY,
};

void *tree_sitter_COBOL_external_scanner_create() {
    return NULL;
}

static bool is_white_space(int c) {
    return iswspace(c) || c == ';' || c == ',';
}

const int number_of_comment_entry_keywords = 9;
char* any_content_keyword[] = {
    "author",
    "installlation",
    "date-written",
    "date-compiled",
    "security",
    "identification division",
    "environment division",
    "data division",
    "procedure division",
};

static bool start_with_word( TSLexer *lexer, char *words[], int number_of_words) {
    while(lexer->lookahead == ' ' || lexer->lookahead == '\t') {
        lexer->advance(lexer, true);
    }

    char *keyword_pointer[number_of_words];
    bool continue_check[number_of_words];
    for(int i=0; i<number_of_words; ++i) {
        keyword_pointer[i] = words[i];
        continue_check[i] = true;
    }

    while(true) {
        // At the end of the line
        if(lexer->get_column(lexer) > 71 || lexer->lookahead == '\n' || lexer->lookahead == 0) {
            return false;
        }

        // If all keyword matching fails, move to the end of the line
        bool all_match_failed = true;
        for(int i=0; i<number_of_words; ++i) {
            if(continue_check[i]) {
                all_match_failed = false;
            }
        }

        if(all_match_failed) {
            for(; lexer->get_column(lexer) < 71 && lexer->lookahead != '\n' && lexer->lookahead != 0;
            lexer->advance(lexer, true)) {
            }
            return false;
        }

        // If the head of the line matches any of specified keywords, return true;
        char c = lexer->lookahead;
        for(int i=0; i<number_of_words; ++i) {
            if(*(keyword_pointer[i]) == 0 && continue_check[i]) {
                return true;
            }
        }

        // matching keywords
        for(int i=0; i<number_of_words; ++i) {
            char k = *(keyword_pointer[i]);
            if(continue_check[i]) {
                continue_check[i] = c == towupper(k) || c == towlower(k);
            }
            (keyword_pointer[i])++;
        }

        // next character
        lexer->advance(lexer, true);
    }

    return false;
}

bool tree_sitter_COBOL_external_scanner_scan(void *payload, TSLexer *lexer,
                                            const bool *valid_symbols) {
    if(lexer->lookahead == 0) {
        return false;
    }

    if(valid_symbols[WHITE_SPACES]) {
        if(is_white_space(lexer->lookahead)) {
            while(is_white_space(lexer->lookahead)) {
                lexer->advance(lexer, true);
            }
            lexer->result_symbol = WHITE_SPACES;
            lexer->mark_end(lexer);
            return true;
        }
    }

    if(valid_symbols[LINE_PREFIX_COMMENT] && lexer->get_column(lexer) <= 5) {
        while(lexer->get_column(lexer) <= 5) {
            lexer->advance(lexer, true);
        }
        lexer->result_symbol = LINE_PREFIX_COMMENT;
        lexer->mark_end(lexer);
        return true;
    }

    if(valid_symbols[LINE_COMMENT]) {
        if(lexer->get_column(lexer) == 6) {
            if(lexer->lookahead == '*' || lexer->lookahead == '/' ||
               lexer->lookahead == 'D' || lexer->lookahead == 'd') {
                while(lexer->lookahead != '\n' && lexer->lookahead != 0) {
                    lexer->advance(lexer, true);
                }
                lexer->result_symbol = LINE_COMMENT;
                lexer->mark_end(lexer);
                return true;
            } else if(lexer->lookahead == '-') {
                // Continuation line indicator: consume the '-' at column 6 and
                // emit as LINE_COMMENT so the rest of the line parses normally.
                lexer->advance(lexer, true);
                lexer->result_symbol = LINE_COMMENT;
                lexer->mark_end(lexer);
                return true;
            } else {
                lexer->advance(lexer, true);
                lexer->mark_end(lexer);
                return false;
            }
        }
    }

    if(valid_symbols[LINE_SUFFIX_COMMENT]) {
        if(lexer->get_column(lexer) >= 72) {
            while(lexer->lookahead != '\n' && lexer->lookahead != 0) {
                lexer->advance(lexer, true);
            }
            lexer->result_symbol = LINE_SUFFIX_COMMENT;
            lexer->mark_end(lexer);
            return true;
        }
    }

    if(valid_symbols[COMMENT_ENTRY]) {
        if(lexer->lookahead == '\n' || lexer->lookahead == 0) {
            return false;
        }
        if(!start_with_word(lexer, any_content_keyword, number_of_comment_entry_keywords)) {
            lexer->mark_end(lexer);
            lexer->result_symbol = COMMENT_ENTRY;
            return true;
        } else {
            return false;
        }
    }

    if(valid_symbols[multiline_string]) {
        // Accept either single-quoted or double-quoted strings that span
        // continuation lines (indicated by '-' at column 6 of the next line).
        int quote_char = lexer->lookahead;
        if(quote_char != '"' && quote_char != '\'') {
            return false;
        }
        while(true) {
            if(lexer->lookahead != quote_char) {
                return false;
            }
            lexer->advance(lexer, false);
            while(lexer->lookahead != quote_char && lexer->lookahead != 0
                  && lexer->lookahead != '\n' && lexer->get_column(lexer) < 72) {
                lexer->advance(lexer, false);
            }
            if(lexer->lookahead == quote_char) {
                lexer->result_symbol = multiline_string;
                lexer->advance(lexer, false);
                lexer->mark_end(lexer);
                return true;
            }
            while(lexer->lookahead != 0 && lexer->lookahead != '\n') {
                lexer->advance(lexer, true);
            }
            if(lexer->lookahead == 0) {
                return false;
            }
            lexer->advance(lexer, true);
            int i;
            for(i=0; i<=5; ++i) {
                if(lexer->lookahead == 0 || lexer->lookahead == '\n') {
                    return false;
                }
                lexer->advance(lexer, true);
            }

            if(lexer->lookahead != '-') {
                return false;
            }

            lexer->advance(lexer, true);
            while(lexer->lookahead == ' ' && lexer->get_column(lexer) < 72) {
                lexer->advance(lexer, true);
            }
        }
    }

    if(valid_symbols[EXECUTE_BODY]) {
        if(lexer->lookahead == 0) {
            return false;
        }

        // Consume tokens one "word" at a time until we see END-EXEC or END-EXECUTE.
        // A "word" here is any run of non-whitespace, non-newline characters.
        // We stop BEFORE consuming the END-EXEC / END-EXECUTE token so that the
        // grammar's execute_end rule can match it normally.
        //
        // Strategy: consume a word into a small buffer; if it matches END-EXEC or
        // END-EXECUTE (case-insensitive) AND is at a word boundary, do NOT include
        // those characters in the token (mark_end was already set before we started
        // the word) and return. Otherwise, call mark_end to commit the word.

        bool consumed = false;

        while(lexer->lookahead != 0) {
            // Skip whitespace (mark as "skip" so it's not part of the token)
            while(is_white_space(lexer->lookahead)) {
                lexer->advance(lexer, true);
            }
            if(lexer->lookahead == '\n' || lexer->lookahead == 0) {
                break;
            }

            // Collect the next word (alphanumeric + hyphen, consistent with _WORD)
            char word[64];
            int wlen = 0;
            while(lexer->lookahead != 0 &&
                  !is_white_space(lexer->lookahead) &&
                  lexer->lookahead != '\n' &&
                  (isalnum(lexer->lookahead) || lexer->lookahead == '-') &&
                  wlen < 63) {
                word[wlen++] = (char)toupper(lexer->lookahead);
                lexer->advance(lexer, false);
            }
            word[wlen] = '\0';

            if(wlen == 0) {
                // Non-word punctuation character (e.g. '(', ')', '.', '\'')
                // Include it in the body token
                lexer->advance(lexer, false);
                lexer->mark_end(lexer);
                consumed = true;
                continue;
            }

            // Check if this word is END-EXEC or END-EXECUTE
            bool is_end_exec    = (strcmp(word, "END-EXEC")    == 0);
            bool is_end_execute = (strcmp(word, "END-EXECUTE") == 0);

            if(is_end_exec || is_end_execute) {
                // Do NOT include this word in the token.
                // mark_end was last set before we started consuming this word,
                // so returning true here gives a token that ends before END-EXEC.
                if(consumed) {
                    lexer->result_symbol = EXECUTE_BODY;
                    return true;
                }
                return false;
            }

            // Not END-EXEC — commit this word into the token
            lexer->mark_end(lexer);
            consumed = true;
        }

        if(consumed) {
            lexer->result_symbol = EXECUTE_BODY;
            return true;
        }
        return false;
    }

    return false;
}

unsigned tree_sitter_COBOL_external_scanner_serialize(void *payload, char *buffer) {
    return 0;
}

void tree_sitter_COBOL_external_scanner_deserialize(void *payload, const char *buffer, unsigned length) {
}

void tree_sitter_COBOL_external_scanner_destroy(void *payload) {
}
