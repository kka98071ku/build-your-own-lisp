#include "mpc/mpc.h"
#include <stdio.h>
#include <stdlib.h>

/* If we are compiling on Windows compile these functions */
#ifdef _WIN32
#include <string.h>

static char buffer[2048];

/* Fake readline function */
char *readline(char *prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char *cpy = malloc(strlen(buffer) + 1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy) - 1] = '\0';
  return cpy;
}

/* Fake add_history function */
void add_history(char *unused) {}

/* Otherwise include the editline headers */
#else
#include <editline/readline.h>
#endif

/* Create Enumeration of Possible lval Types */
enum { LVAL_NUM, LVAL_ERR };

/* Create Enumeration of Possible Error Types */
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

/* Declare New lval Struct */
typedef struct {
  int type;
  long num;
  int err;
} lval;

/* Create a new number type lval */
lval lval_num(long x) {
  lval v;
  v.type = LVAL_NUM;
  v.num = x;
  return v;
}

/* Create a new error type lval */
lval lval_err(int x) {
  lval v;
  v.type = LVAL_ERR;
  v.err = x;
  return v;
}

void lval_print(lval v) {
  switch (v.type) {
  case LVAL_NUM:
    printf("%li", v.num);
    break;
  case LVAL_ERR:
    if (v.err == LERR_DIV_ZERO) {
      printf("Error: Division By Zero!");
    } else if (v.err == LERR_BAD_OP) {
      printf("Error: Invalid Operator!");
    } else if (v.err == LERR_BAD_NUM) {
      printf("Error: Invalid Number!");
    }
    break;
  }
}

void lval_println(lval v) {
  lval_print(v);
  putchar('\n');
}

lval eval_op(lval x, char *op, lval y) {
  if (x.type == LVAL_ERR) {
    return x;
  }
  if (y.type == LVAL_ERR) {
    return y;
  }

  if (strcmp(op, "+") == 0) {
    return lval_num(x.num + y.num);
  }
  if (strcmp(op, "-") == 0) {
    return lval_num(x.num - y.num);
  }
  if (strcmp(op, "*") == 0) {
    return lval_num(x.num * y.num);
  }
  if (strcmp(op, "/") == 0) {
    return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num / y.num);
  }

  /* min and max could be evaluated recursively */
  if (strcmp(op, "min") == 0) {
    return x.num < y.num ? x : y;
  }
  if (strcmp(op, "max") == 0) {
    return x.num < y.num ? y : x;
  }
  return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t *t) {

  /* If tagged as number return it directly */
  if (strstr(t->tag, "number")) {
    /* a speical error number to check exception */
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
  }

  /* operator is always the second child */
  char *op = t->children[1]->contents;

  /* starting evaluating */
  lval x = eval(t->children[2]);

  /* iterate the remaining children and combining */
  int i = 3;
  while (strstr(t->children[i]->tag, "expr")) {
    x = eval_op(x, op, eval(t->children[i]));
    i++;
  }
  return x;
}

int main(int argc, char **argv) {

  /* Create Some Parsers */
  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Operator = mpc_new("operator");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");

  /* Define them with the following language */
  /* number could be negative and we allow multiple preceding zeros */
  mpca_lang(
      MPCA_LANG_DEFAULT,
      "																										    \
			  number		: /-?[0-9]+/ ;													    \
				operator	: '+' | '-' | '*' | '/' | \"min\" | \"max\"; \
				expr			: <number> | '(' <operator> <expr>+ ')';    \
				lispy		  : /^/ <operator> <expr>+ /$/ ;					     \
			",
      Number, Operator, Expr, Lispy);

  puts("Lispy Version 0.0.0.0.1");
  puts("Press Ctrl+c to Exit\n");

  while (1) {
    // output and get input
    char *input = readline("lispy> ");

    add_history(input);

    /*
     * typedef struct mpc_ast_t {
     *   char* tag;
     *   char* contents;
     *   mpc_state_t state;
     *   int children_num;
     *   struct mpc_ast_t** children;
     * } mpc_ast_t;
     */
    /* Attempt to parse the user input and print out AST */
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      lval result = eval(r.output);
      lval_println(result);
      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);
  }

  /* Undefine and delete the parsers */
  mpc_cleanup(4, Number, Operator, Expr, Lispy);
  return 0;
}
