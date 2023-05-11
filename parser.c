#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "token.h"
#include "mil.h"

int g_bytecode[65536];
int g_bytecode_size = 0;
char *g_str_pool[4096];
int g_str_pool_size = 0;

static char *st_var_table[VAR_MAX]; //変数名を格納する配列
static int st_var_table_size = 0;

typedef struct {
  char *identifier;
  int address;
} Label;
static Label st_label_table[65536]; //ラベルを格納する配列
static int st_label_table_size = 0;

static Token st_look_ahead_token;
static int st_look_ahead_token_exists;

static Token get_token(void) {
  Token ret;
  if (st_look_ahead_token_exists) {
    ret = st_look_ahead_token;
    st_look_ahead_token_exists = 0;
  } else {
    ret = lex_get_token();
  }
  return ret;
}

static void unget_token(Token token) {
  st_look_ahead_token = token;
  st_look_ahead_token_exists = 1; //トークンを押し戻す
}

static void add_bytecode(int bytecode) {
  g_bytecode[g_bytecode_size] = bytecode;
  g_bytecode_size++;
}

static void parse_error(char *message) {
  int line_number = lex_get_line_number();
  fprintf(stderr, "%d:Parse error:%s\n", line_number, message);
  exit(1);
}

static void check_expected_token(TokenKind expected) {
  Token token = get_token();
  if (token.kind != expected) {
    parse_error("parse error");
  }
}

//変数を検索
static int search_var(char *identifier) {
  int i;
  for (i = 0; i < st_var_table_size; i++) {
    if (!strcmp(identifier, st_var_table[i]))
      return i;
  }
  return -1;
}

//変数を検索または新たに登録する
static int search_or_new_var(char *identifier) { 
  int ret;
  ret = search_var(identifier);
  if (ret < 0) {
    st_var_table[st_var_table_size] = identifier;
    return st_var_table_size++; 
  }
  return ret;
}

static void parse_expression(void);

//基本式をパース
static void parse_primary_expression(void) {
  Token token;

  token = get_token();
  if (token.kind == INT_VALUE_TOKEN) {
    add_bytecode((int)OP_PUSH_INT);
    add_bytecode(token.u.int_value);
  } else if (token.kind == STRING_LITERAL_TOKEN) {
    add_bytecode((int)OP_PUSH_STRING);
    g_str_pool[g_str_pool_size] = token.u.string; 
    add_bytecode(g_str_pool_size);
    g_str_pool_size++;
  } else if (token.kind == LEFT_PAREN_TOKEN) {  
    parse_expression();
    check_expected_token(RIGHT_PAREN_TOKEN);
  } else if (token.kind == IDENTIFIER_TOKEN) {
    int var_idx = search_var(token.u.identifier); if (var_idx < 0) {
      parse_error("identifier not found.");
    }
    add_bytecode((int)OP_PUSH_VAR);
    add_bytecode(var_idx);
  }
}

//負の数のある式をパース
static void parse_unary_expression(void) {
  Token token;
  token = get_token();
  if (token.kind == SUB_TOKEN) {
    parse_primary_expression();
    add_bytecode((int)OP_MINUS);
  } else {
    unget_token(token);
    parse_primary_expression();
  }
}

//*または/からなる式（項）をパース
static void parse_multiplicative_expression(void) {
  Token token;
  parse_unary_expression();
  for (;;) {
    token = get_token();
    if (token.kind != MUL_TOKEN && token.kind != DIV_TOKEN) {
      unget_token(token);
      break;
    }
    parse_unary_expression();
    if (token.kind == MUL_TOKEN) {
      add_bytecode((int)OP_MUL);
    } else {
      add_bytecode((int)OP_DIV);
    }
  }
}

//+または-からなる式（項）をパース
static void parse_additive_expression(void) {
  Token token;
  parse_multiplicative_expression();
  for (;;) {
    token = get_token();
    if (token.kind != ADD_TOKEN && token.kind != SUB_TOKEN) {
      unget_token(token);
      break;
    } 
    parse_multiplicative_expression();
    if (token.kind == ADD_TOKEN) {
      add_bytecode((int)OP_ADD);
    } else {
      add_bytecode((int)OP_SUB);
    }
  }
}

//比較演算子からなる式（項）をパース
static void parse_compare_expression(void) {
  Token token;
  parse_additive_expression();
  for (;;) {
    token = get_token();
    if (token.kind != EQ_TOKEN && token.kind != NE_TOKEN && token.kind != GT_TOKEN && token.kind != GE_TOKEN && token.kind != LT_TOKEN && token.kind != LE_TOKEN) {
      unget_token(token);
      break;
    }
    parse_additive_expression();
    if (token.kind == EQ_TOKEN) {
      add_bytecode((int)OP_EQ);
    } else if (token.kind == NE_TOKEN) {
      add_bytecode((int)OP_NE);
    } else if (token.kind == GT_TOKEN) {
      add_bytecode((int)OP_GT);
    } else if (token.kind == GE_TOKEN) {
      add_bytecode((int)OP_GE);
    } else if (token.kind == LT_TOKEN) {
      add_bytecode((int)OP_LT);
    } else if (token.kind == LE_TOKEN) {
      add_bytecode((int)OP_LE);
    }
  }
}

//式をパース
static void parse_expression(void){     
  parse_compare_expression();
}

static void parse_block(void);

static int get_label(void){
  return st_label_table_size++;
}
static void set_label(int label_idx) {  
  st_label_table[label_idx].address = g_bytecode_size; 
}

static int search_or_new_label(char *label) {
  int i;
  for (i = 0; i < st_label_table_size; i++) {
    if (st_label_table[i].identifier != NULL && !strcmp(st_label_table[i].identifier, label)) {
      return i;
    }
  }
  st_label_table[i].identifier = label;
  return st_label_table_size++;
}

//if文をパース
static void parse_if_statement(void) {
  Token token;
  int else_label;
  int end_if_label;
  check_expected_token(LEFT_PAREN_TOKEN); 
  parse_expression();
  check_expected_token(RIGHT_PAREN_TOKEN);
  else_label = get_label();
  add_bytecode((int)OP_JUMP_IF_ZERO);
  add_bytecode(else_label); parse_block();
  token = get_token();
  if (token.kind == ELSE_TOKEN) {
    end_if_label = get_label(); 
    add_bytecode((int)OP_JUMP);
    add_bytecode(end_if_label);
    set_label(else_label);
    parse_block();
    set_label(end_if_label);
  } else {
    unget_token(token);
    set_label(else_label);
  }
}

//while文をパース
static void parse_while_statement(void) {
  int loop_label;
  int end_while_label;
  loop_label = get_label();
  set_label(loop_label);
  check_expected_token(LEFT_PAREN_TOKEN);
  parse_expression();
  check_expected_token(RIGHT_PAREN_TOKEN);
  end_while_label = get_label();
  add_bytecode((int)OP_JUMP_IF_ZERO);
  add_bytecode(end_while_label);
  parse_block();
  add_bytecode((int)OP_JUMP);
  add_bytecode(loop_label);
  set_label(end_while_label);
}

//print文をパース
static void parse_print_statement(void) { 
  check_expected_token(LEFT_PAREN_TOKEN); 
  parse_expression();
  check_expected_token(RIGHT_PAREN_TOKEN); 
  add_bytecode((int)OP_PRINT);
  check_expected_token(SEMICOLON_TOKEN);
}

//代入文をパース
static void parse_assign_statement(char *identifier) {
  int var_idx = search_or_new_var(identifier);
  check_expected_token(ASSIGN_TOKEN); 
  parse_expression();
  add_bytecode((int)OP_ASSIGN_TO_VAR);
  add_bytecode(var_idx);
  check_expected_token(SEMICOLON_TOKEN);
}

//goto文をパース
static void parse_goto_statement(void){
  Token token;
  int label;
  check_expected_token(MUL_TOKEN);
  token = get_token();
  if (token.kind != IDENTIFIER_TOKEN) {
    parse_error("label identifier expected");
  }
  label = search_or_new_label(token.u.identifier); 
  add_bytecode((int)OP_JUMP);
  add_bytecode(label);
  check_expected_token(SEMICOLON_TOKEN);
}

//gosub文をパース
static void parse_gosub_statement(void){
  Token token;
  int label;
  check_expected_token(MUL_TOKEN);
  token = get_token();
  if (token.kind != IDENTIFIER_TOKEN) {
    parse_error("label identifier expected");
  }
  label = search_or_new_label(token.u.identifier); 
  add_bytecode((int)OP_GOSUB);
  add_bytecode(label);
  check_expected_token(SEMICOLON_TOKEN);
}

//ラベル文をパース
static void parse_label_statement(void) {
  Token token;
  int label;
  token = get_token();
  if (token.kind != IDENTIFIER_TOKEN) {
    parse_error("label identifier expected");
  }
  label = search_or_new_label(token.u.identifier); 
  set_label(label);
}

//return文をパース
static void parse_return_statement(void) { 
  add_bytecode((int)OP_RETURN); 
  check_expected_token(SEMICOLON_TOKEN);
}

static void parse_statement(void){
  Token token;
  token = get_token();

  //トークンを読み取り種別ごとに分岐
  if (token.kind == IF_TOKEN) {
    parse_if_statement();
  } else if (token.kind == WHILE_TOKEN) {
    parse_while_statement();
  } else if (token.kind == PRINT_TOKEN) {
    parse_print_statement();
  } else if (token.kind == GOTO_TOKEN) {
    parse_goto_statement();
  } else if (token.kind == GOSUB_TOKEN) {
    parse_gosub_statement();
  } else if (token.kind == RETURN_TOKEN) {
    parse_return_statement();
  } else if (token.kind == MUL_TOKEN) {
    parse_label_statement();
  } else if (token.kind == IDENTIFIER_TOKEN) {
    parse_assign_statement(token.u.identifier);
  } else {
    parse_error("bad statement."); 
  }
}

// { }のブロックをパース
static void parse_block(void){
  Token token; check_expected_token(LEFT_BRACE_TOKEN);
  for (;;) {
    token = get_token();
    if (token.kind == RIGHT_BRACE_TOKEN) {
      break;
    }
    unget_token(token);
    parse_statement();
  }
}

//ジャンプ先の実アドレスを書き込む
static void fix_labels(void) {
  int i;
  for (i = 0; i < g_bytecode_size; i++) {
    if (g_bytecode[i] == OP_PUSH_INT
      || g_bytecode[i] == OP_PUSH_STRING
      || g_bytecode[i] == OP_PUSH_VAR
      || g_bytecode[i] == OP_ASSIGN_TO_VAR) {
        i++;
    }else if (g_bytecode[i] == OP_JUMP
      || g_bytecode[i] == OP_JUMP_IF_ZERO
      || g_bytecode[i] == OP_GOSUB) {
        g_bytecode[i+1] = st_label_table[g_bytecode[i+1]].address;
    }
  }
}

//パーサーはここからスタート
static void parse(void) {
  Token token;
  for (;;) {
    token = get_token();
    if (token.kind == END_OF_FILE_TOKEN) {
      break;
    } else {
      unget_token(token);
      parse_statement();
    }
  }
}

int main(int argc, char **argv){
  FILE *src_fp;
  int c = 0;
  if (argc != 2) {
    fprintf(stderr, "Usage:%s filename¥n", argv[0]);
    exit(1);
  }
  src_fp = fopen(argv[1], "r");
  if (src_fp == NULL) {
    fprintf(stderr, "%s not found.¥n", argv[1]);
    exit(1);
  }
  lex_initialize(src_fp); //字句解析をして
  
  parse();
  fix_labels(); //構文解析をして
  mvm_execute(); //実行する

  while(g_string_pointer_pool[c] != 0){
    free(g_string_pointer_pool[c++]);
  }
  return 0;
}