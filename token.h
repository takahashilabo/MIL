#ifndef TOKEN_H_INCLUDED
#define TOKEN_H_INCLUDED
#include <stdio.h>

typedef enum {
    INT_VALUE_TOKEN,
    IDENTIFIER_TOKEN,
    STRING_LITERAL_TOKEN,
    EQ_TOKEN,
    NE_TOKEN,
    GE_TOKEN,
    LE_TOKEN,
    ADD_TOKEN,
    SUB_TOKEN,
    MUL_TOKEN,
    DIV_TOKEN,
    ASSIGN_TOKEN,
    GT_TOKEN,
    LT_TOKEN,
    LEFT_PAREN_TOKEN,
    RIGHT_PAREN_TOKEN,
    LEFT_BRACE_TOKEN,
    RIGHT_BRACE_TOKEN,
    COMMA_TOKEN,
    SEMICOLON_TOKEN,
    IF_TOKEN,
    ELSE_TOKEN,
    WHILE_TOKEN,
    GOTO_TOKEN,
    GOSUB_TOKEN,
    RETURN_TOKEN,
    PRINT_TOKEN,
    END_OF_FILE_TOKEN
} TokenKind;

typedef struct {
    TokenKind kind;
    union { //ここがポイント
        int int_value;
        char *string;
        char *identifier;
    } u;
} Token;

void lex_initialize(FILE *src_fp);
Token lex_get_token(void);
int lex_get_line_number(void);

extern char *g_string_pointer_pool[];

#endif /* TOKEN_H_INCLUDED */