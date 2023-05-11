#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "token.h"

char *g_string_pointer_pool[65536];
int g_string_pointer_pool_size = 0;

static FILE *st_source_file;
static int st_current_line_number;

//読み取っているトークンの種別を保持するための列挙型
typedef enum {
  INITIAL_STATE,
  INT_VALUE_STATE,
  IDENTIFIER_STATE,
  STRING_STATE,
  OPERATOR_STATE,
  COMMENT_STATE
} LexerState;

static void lex_error(char *message, int ch) {
  fprintf(stderr, "lex error:%s near\'%c\'\n", message, ch);
  exit(1);
}

static char *my_strdup(char *src) {
  char *dest = malloc(strlen(src) + 1);
  g_string_pointer_pool[g_string_pointer_pool_size++] = dest;
  strcpy(dest, src);
  return dest;
}

void lex_initialize(FILE *src_fp) {
  st_source_file = src_fp;
  st_current_line_number = 1;
}

static void add_letter(char *token, int letter) {
  int len = strlen(token);
  token[len] = letter;
  token[len + 1] = '\0';
}

typedef struct {
  char *token;
  TokenKind kind;
} OperatorInfo;

//演算子とトークンの列挙型を対応付ける
static OperatorInfo st_operator_table[] = {
    {"==", EQ_TOKEN},
    {"!=", NE_TOKEN},
    {">=", GE_TOKEN},
    {"<=", LE_TOKEN},
    {"+", ADD_TOKEN},
    {"-", SUB_TOKEN},
    {"*", MUL_TOKEN},
    {"/", DIV_TOKEN},
    {"=", ASSIGN_TOKEN},
    {">", GT_TOKEN},
    {"<", LT_TOKEN},
    {"(", LEFT_PAREN_TOKEN},
    {")", RIGHT_PAREN_TOKEN},
    {"{", LEFT_BRACE_TOKEN},
    {"}", RIGHT_BRACE_TOKEN},
    {",", COMMA_TOKEN},
    {";", SEMICOLON_TOKEN}
};

int in_operator(char *token, int letter) {
  int op_idx;
  int letter_idx;
  int len = strlen(token);

  for (op_idx = 0; op_idx < (sizeof(st_operator_table) / sizeof(OperatorInfo)); op_idx++) {
    for (letter_idx = 0; letter_idx < len &&
                         st_operator_table[op_idx].token[letter_idx] != '\0';
         letter_idx++) {
      if (token[letter_idx] != st_operator_table[op_idx].token[letter_idx]) {
        break;
      }
    }
    if (token[letter_idx] == '\0' &&
        st_operator_table[op_idx].token[letter_idx] == letter) {
      return 1;
    }
  }
  return 0;
}

//演算子および区切り子を判定
TokenKind select_operator(char *token) {
  int i;
  for (i = 0; i < sizeof(st_operator_table) / sizeof(OperatorInfo); i++) {
    if (!strcmp(token, st_operator_table[i].token)) {
      return st_operator_table[i].kind;
    }
  }
  assert(0);
  return 0;
}

typedef struct {
  char *token;
  TokenKind kind;
} KeywordInfo;

//キーワードとトークンの列挙型を対応づける
KeywordInfo st_keyword_table[] = {
    {"if", IF_TOKEN},
    {"else", ELSE_TOKEN},
    {"while", WHILE_TOKEN},
    {"goto", GOTO_TOKEN},
    {"gosub", GOSUB_TOKEN},
    {"return", RETURN_TOKEN},
    {"print", PRINT_TOKEN}
};

int is_keyword(char *token, TokenKind *kind) {
  int i;
  for (i = 0; i < sizeof(st_keyword_table) / sizeof(KeywordInfo); i++) {
    if (!strcmp(st_keyword_table[i].token, token)) {
      *kind = st_keyword_table[i].kind;
      return 1;
    }
  }
  return 0;
}

//レキシカルアナライザの心臓部
Token lex_get_token(void) {
  Token ret;
  LexerState state = INITIAL_STATE; //読み取り中のトークンの種別を保持するための変数
  char token[256];
  int ch;

  token[0] = '\0';

  //ファイルから１文字読み取る（ループ）
  while ((ch = getc(st_source_file)) != EOF) {
    switch(state) {
    case INITIAL_STATE:
      if (isdigit(ch)) { //数字？
        add_letter(token, ch);
        state = INT_VALUE_STATE;
      } else if (isalpha(ch) || ch == '_') { //英文字？　_？
        add_letter(token, ch);
        state = IDENTIFIER_STATE;
      } else if (ch == '\"') { //文字列？
        state = STRING_STATE;
      } else if (in_operator(token, ch)) { //演算子？
        add_letter(token, ch);
        state = OPERATOR_STATE;
      } else if (isspace(ch)) { //空白？
        if (ch == '\n') { //改行？
          st_current_line_number++;
        }
      } else if (ch == '#') { //コメント？
        state = COMMENT_STATE;
      } else {
        lex_error("bad character", ch); //エラー
      }
      break;
    case INT_VALUE_STATE:
      if (isdigit(ch)) {
        add_letter(token, ch);
      } else {
        ret.kind = INT_VALUE_TOKEN;
        sscanf(token, "%d", &ret.u.int_value);
        ungetc(ch, st_source_file); //数字でなければ１文字戻す
        goto LOOP_END;
      }
      break;
    case IDENTIFIER_STATE:
      if (isalpha(ch) || ch == '_' || isdigit(ch)) {
        add_letter(token, ch);
      } else {
        ret.u.identifier = token;
        ungetc(ch, st_source_file); //英文字でなければ１文字戻す
        goto LOOP_END;
      }
      break;
    case STRING_STATE:
      if (ch == '\"') {
        ret.kind = STRING_LITERAL_TOKEN;
        ret.u.string = my_strdup(token);
        goto LOOP_END;
      } else {
        add_letter(token, ch);
      }
      break;
    case OPERATOR_STATE:
      if (in_operator(token, ch)) {
        add_letter(token, ch);
      } else {
        ungetc(ch, st_source_file); //演算子でなければ１文字戻す
        goto LOOP_END;
      }
      break;
    case COMMENT_STATE:
      if (ch == '\n') {
        state = INITIAL_STATE; //改行がくるまでコメントを読み飛ばす
      }
      break;
    default:
      assert(0);
    }
  }
LOOP_END:
  //EOFをチェック
  if (ch == EOF) {
    if (state == INITIAL_STATE || state == COMMENT_STATE) {
      ret.kind = END_OF_FILE_TOKEN;
      return ret;
    }
  }
  if (state == IDENTIFIER_STATE) {
    if (!is_keyword(token, &ret.kind)) {
      ret.kind = IDENTIFIER_TOKEN;
      ret.u.string = my_strdup(token);
    }
  } else if (state == OPERATOR_STATE) {
    ret.kind = select_operator(token);
  }
  return ret;
}

int lex_get_line_number(void) {
  return st_current_line_number;
}

int main(int argc, char *argv[]) {
  FILE *fp;
  Token token;

  fp = fopen(argv[1], "r");
  lex_initialize(fp);

  do {
    token = lex_get_token();
    if (token.kind == INT_VALUE_TOKEN) {
      printf("%d 整数\n", token.u.int_value);
    } else if (token.kind == IDENTIFIER_TOKEN) {
      printf("%s 識別子\n", token.u.identifier);
    } else if (token.kind == STRING_LITERAL_TOKEN) {
      printf("%s 文字列リテラル\n", token.u.string);
    } else if (token.kind >= EQ_TOKEN && token.kind <= SEMICOLON_TOKEN) {
      printf("%s 演算子または区切り子\n", st_operator_table[token.kind - EQ_TOKEN].token);
    } else if (token.kind != END_OF_FILE_TOKEN && token.kind >= IF_TOKEN) {
      printf("%s 予約語（キーワード）\n", st_keyword_table[token.kind - IF_TOKEN].token);
    }
  } while (token.kind != END_OF_FILE_TOKEN);

  fclose(fp);

  return 0;
}