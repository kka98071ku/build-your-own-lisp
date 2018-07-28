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
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR };

/* Create Enumeration of Possible Error Types */
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

/*
 * S-expresisons are a variable length lists of other values (see the syntax
 * rule below) However, "struct" is fixed sized, so we have to use a double star
 * pointer to point to a list of "lval"s.
 */
typedef struct lval {
  int type;
  long num;

  /* Error and Symbol types have some string data */
  char *err;
  char *sym;

  /* Count and Pointer to an array of "lval*" */
  int count;
  /* NOTE: use 'struct lval' to refer to itself inside the definition */
  struct lval **cell;
} lval;

lval *lval_num(long x) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

lval *lval_err(char *m) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  /*
   * NOTE: C strings are null terminated, meaning that the final character is
   * always '\0'; however, "strlen" only returns the length excluding the null
   * terminator...
   */
  v->err = malloc(strlen(m) + 1);
  strcpy(v->err, m);
  return v;
}

lval *lval_sym(char *s) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

lval *lval_sexpr(void) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  /* NOTE: NULL is a special constant that points to memory location 0 */
  v->cell = NULL;
  return v;
}

void lval_del(lval *v) {
  switch (v->type) {
  case LVAL_NUM:
    break;
  case LVAL_ERR:
    free(v->err);
    break;
  case LVAL_SYM:
    free(v->sym);
    break;
  case LVAL_SEXPR:
    for (int i = 0; i < v->count; i++) {
      /* NOTE: recursively! */
      lval_del(v->cell[i]);
    }
    /* NOTE: also free the memory allocated to contain the pointers */
    free(v->cell);
    break;
  }

  /* NOTE: free the memory allocated for the "lval" struct itself */
  free(v);
}

lval *lval_read_num(mpc_ast_t *t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval *lval_add(lval *v, lval *x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval *) * v->count);
  v->cell[v->count - 1] = x;
  return v;
}

lval *lval_read(mpc_ast_t *t) {
  /* If Symbol or Number return conversion to that type */
  if (strstr(t->tag, "number")) {
    return lval_read_num(t);
  }

  if (strstr(t->tag, "symbol")) {
    return lval_sym(t->contents);
  }

  lval *x = NULL;
  /* If root (>) or sexpr then create empty list */
  if (strcmp(t->tag, ">") == 0) {
    x = lval_sexpr();
  }
  if (strstr(t->tag, "sexpr")) {
    x = lval_sexpr();
  }
  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0 ||
        strcmp(t->children[i]->contents, ")") == 0 ||
        strcmp(t->children[i]->tag, "regex") == 0) {
      continue;
    }
    x = lval_add(x, lval_read(t->children[i]));
  }
  return x;
}

void lval_print(lval *v);
void lval_expr_print(lval *v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++) {
    lval_print(v->cell[i]);
    if (i != (v->count - 1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

void lval_print(lval *v) {
  switch (v->type) {
  case LVAL_NUM:
    printf("%li", v->num);
    break;
  case LVAL_ERR:
    printf("Error: %s", v->err);
    break;
  case LVAL_SYM:
    printf("%s", v->sym);
    break;
  case LVAL_SEXPR:
    lval_expr_print(v, '(', ')');
    break;
  }
}

void lval_println(lval *v) {
  lval_print(v);
  putchar('\n');
}

// lval eval_op(lval x, char *op, lval y) {
//   if (x.type == LVAL_ERR) {
//     return x;
//   }
//   if (y.type == LVAL_ERR) {
//     return y;
//   }
//
//   if (strcmp(op, "+") == 0) {
//     return lval_num(x.num + y.num);
//   }
//   if (strcmp(op, "-") == 0) {
//     return lval_num(x.num - y.num);
//   }
//   if (strcmp(op, "*") == 0) {
//     return lval_num(x.num * y.num);
//   }
//   if (strcmp(op, "/") == 0) {
//     return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num / y.num);
//   }
//
//   /* min and max could be evaluated recursively */
//   if (strcmp(op, "min") == 0) {
//     return x.num < y.num ? x : y;
//   }
//   if (strcmp(op, "max") == 0) {
//     return x.num < y.num ? y : x;
//   }
//   return lval_err(LERR_BAD_OP);
// }
//
// lval eval(mpc_ast_t *t) {
//
//   /* If tagged as number return it directly */
//   if (strstr(t->tag, "number")) {
//     /* a speical error number to check exception */
//     errno = 0;
//     long x = strtol(t->contents, NULL, 10);
//     return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
//   }
//
//   /* operator is always the second child */
//   char *op = t->children[1]->contents;
//
//   /* starting evaluating */
//   lval x = eval(t->children[2]);
//
//   /* iterate the remaining children and combining */
//   int i = 3;
//   while (strstr(t->children[i]->tag, "expr")) {
//     x = eval_op(x, op, eval(t->children[i]));
//     i++;
//   }
//   return x;
// }

int main(int argc, char **argv) {

  /* Create Some Parsers */
  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Symbol = mpc_new("symbol");
  mpc_parser_t *Sexpr = mpc_new("sexpr");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");

  /*
   * Define them with the following language
   *
   * The syntactic elements of Lisp are 'symbolic expressions', a.k.a.
   * 's-expressions' Both programs and data are represented as s-expressions: an
   * s-expresison may either an 'atom' or 'list'
   *
   * Examples of atoms include:
   *   100
   *   hyphenated-name
   *   nil
   *   *some-global*
   *
   * A list is a sequence of either atoms or other lists separated by blancks
   * and enclosed in parentheses.
   *
   * Examples of lists include:
   *   (1 2 3 4)
   *   (george kate james joyce)
   *   (a (b c ) (d (e f)))
   *   ()
   *
   * p.s. number could be negative and we allow multiple preceding zeros
   */
  mpca_lang(
      MPCA_LANG_DEFAULT,
      "																										      \
			  number		: /-?[0-9]+/ ;													      \
				symbol		: '+' | '-' | '*' | '/' | \"min\" | \"max\";  \
				sexpr 		: '(' <expr> *')';                            \
				expr			: <number> | <symbol> | <sexpr> ;    					\
				lispy		  : /^/ <expr>* /$/ ;					      						\
			",
      Number, Symbol, Sexpr, Expr, Lispy);

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
      // lval result = eval(r.output);
      // lval_println(result);
      lval *result = lval_read(r.output);
      lval_println(result);
      lval_del(result);
      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);
  }

  /* Undefine and delete the parsers */
  mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lispy);
  return 0;
}
