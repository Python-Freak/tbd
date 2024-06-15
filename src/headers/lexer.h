#ifndef _TBD_HEADERS_LEXER_GUARD

#define _TBD_HEADERS_LEXER_GUARD

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "../utils/errors.h"
#include "../utils/logging.h"

const char *NEWLINE = "\r\n";
const char *COMMENT_DEIMETER = "#";
const char *WHITESPACE = " \r\n";
const char *DELIMETER = " \r\n,():";

typedef struct Token {
    char *beginning;
    char *end;
} Token;

bool token_equals_string(Token *, char *);
Error lex(char *, Token *);

#endif
