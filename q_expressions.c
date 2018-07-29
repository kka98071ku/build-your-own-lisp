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

/* Forward Declarations */
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

/* Let's define function pointers to allow user-defined operations!
 *
 * NOTE: notice the first arg is a pointer to the environment
 * "To get an lval* we dereference lbuiltin and call it with a lenv* and a
 * lval*." Therefore lbuiltin must be a function pointer that takes an lenv* and
 * a lval* and returns a lval*.
 */
typedef lval *(*lbuiltin)(lenv *, lval *);

/* Create Enumeration of Possible lval Types */
enum { LVAL_NUM, LVAL_ERR, LVAL_FUN, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };

/* Create Enumeration of Possible Error Types */
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

/*
 * S-expresisons are a variable length lists of other values (see the syntax
 * rule below) However, "struct" is fixed sized, so we have to use a double star
 * pointer to point to a list of "lval"s.
 */
struct lval {
  int type;
  long num;

  /* Error and Symbol types have some string data */
  char *err;
  char *sym;
  lbuiltin fun;

  /* Count and Pointer to an array of "lval*" */
  int count;
  /* NOTE: use 'struct lval' to refer to itself inside the definition */
  struct lval **cell;
};

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

lval *lval_fun(lbuiltin func) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->fun = func;
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

lval *lval_qexpr(void) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

void lval_del(lval *v) {
  switch (v->type) {
  case LVAL_NUM:
  case LVAL_FUN:
    break;
  case LVAL_ERR:
    free(v->err);
    break;
  case LVAL_SYM:
    free(v->sym);
    break;
  case LVAL_QEXPR:
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
  if (strstr(t->tag, "qexpr")) {
    x = lval_qexpr();
  }
  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0 ||
        strcmp(t->children[i]->contents, ")") == 0 ||
        strcmp(t->children[i]->contents, "{") == 0 ||
        strcmp(t->children[i]->contents, "}") == 0 ||
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
  case LVAL_FUN:
    printf("<func>");
    break;
  case LVAL_SYM:
    printf("%s", v->sym);
    break;
  case LVAL_SEXPR:
    lval_expr_print(v, '(', ')');
    break;
  case LVAL_QEXPR:
    lval_expr_print(v, '{', '}');
    break;
  }
}

void lval_println(lval *v) {
  lval_print(v);
  putchar('\n');
}

lval *lval_copy(lval *v) {
  lval *x = malloc(sizeof(lval));
  x->type = v->type;
  switch (v->type) {
  /* Copy Functions and Numbers Directly */
  case LVAL_FUN:
    x->fun = v->fun;
    break;
  case LVAL_NUM:
    x->num = v->num;
    break;

  /* Copy Strings using malloc and strcpy */
  case LVAL_ERR:
    x->err = malloc(strlen(v->err) + 1);
    strcpy(x->err, v->err);
    break;
  case LVAL_SYM:
    x->sym = malloc(strlen(v->sym) + 1);
    strcpy(x->sym, v->sym);
    break;

  /* Copy Lists by copying each sub-expression */
  case LVAL_SEXPR:
  case LVAL_QEXPR:
    x->count = v->count;
    x->cell = malloc(sizeof(lval *) * x->count);
    for (int i = 0; i < x->count; i++) {
      x->cell[i] = lval_copy(v->cell[i]);
    }
    break;
  }
  return x;
}

/*
 * Envroment structure encode a list of relationships between names and values
 */
struct lenv {
  int count;
  char **syms;
  lval **vals;
};

lenv *lenv_new(void) {
  lenv *env = malloc(sizeof(lenv));
  env->count = 0;
  env->syms = NULL;
  env->vals = NULL;
  return env;
}

void lenv_del(lenv *e) {
  for (int i = 0; i < e->count; i++) {
    free(e->syms[i]);
    lval_del(e->vals[i]);
  }
  free(e->syms);
  free(e->vals);
  free(e);
}

lval *lenv_get(lenv *e, lval *k) {
  if (k->type == LVAL_SYM) {
    for (int i = 0; i < e->count; i++) {
      if (strcmp(e->syms[i], k->sym) == 0) {
        return lval_copy(e->vals[i]);
      }
    }
  }
  return lval_err("unbound symbol!");
}

void lenv_put(lenv *e, lval *k, lval *v) {
  if (k->type != LVAL_SYM) {
    return;
  }
  for (int i = 0; i < e->count; i++) {
    if (strcmp(e->syms[i], k->sym) == 0) {
      lval_del(e->vals[i]);
      e->vals[i] = lval_copy(v);
      return;
    }
  }

  e->count++;
  e->vals = realloc(e->vals, sizeof(lval *) * e->count);
  e->syms = realloc(e->syms, sizeof(char *) * e->count);
  e->vals[e->count - 1] = lval_copy(v);
  e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
  strcpy(e->syms[e->count - 1], k->sym);
}

void lenv_add_builtin(lenv *e, char *name, lbuiltin func) {
  lval *k = lval_sym(name);
  lval *v = lval_fun(func);
  lenv_put(e, k, v);
  lval_del(k);
  lval_del(v);
}

#define LASSERT(args, cond, err)                                               \
  if (!(cond)) {                                                               \
    lval_del(args);                                                            \
    return lval_err(err);                                                      \
  }

lval *lval_eval_sexpr(lenv *e, lval *v);
lval *lval_eval(lenv *e, lval *v) {
  if (v->type == LVAL_SYM) {
    lval *x = lenv_get(e, v);
    lval_del(v);
    return x;
  }
  if (v->type == LVAL_SEXPR) {
    return lval_eval_sexpr(e, v);
  }
  /* All other lval types remain the same */
  return v;
}

lval *lval_pop(lval *v, size_t to_pop) {
  lval *res = v->cell[to_pop];
  memmove(&v->cell[to_pop], &v->cell[to_pop + 1],
          sizeof(lval *) * (v->count - to_pop - 1));

  v->count--;

  /* NOTE: reallocate the memory used */
  v->cell = realloc(v->cell, sizeof(lval *) * v->count);
  return res;
}

lval *lval_take(lval *v, size_t to_move) {
  /* NOTE: not the most efficient */
  lval *res = lval_pop(v, to_move);
  lval_del(v);
  return res;
}

lval *builtin_op(lenv *e, lval *a, char *op) {

  /* Ensure all elements are numbers */
  for (int i = 0; i < a->count; i++) {
    if (a->cell[i]->type != LVAL_NUM) {
      lval_del(a);
      return lval_err("Cannot operate on non-number!");
    }
  }

  /* Pop the first element */
  lval *x = lval_pop(a, 0);

  /* If no arguments and sub then perform unary negation */
  if ((strcmp(op, "-") == 0) && a->count == 0) {
    x->num = -x->num;
  }

  while (a->count > 0) {
    /* Pop the next element */
    lval *y = lval_pop(a, 0);
    if (strcmp(op, "+") == 0) {
      x->num += y->num;
      break;
    } else if (strcmp(op, "-") == 0) {
      x->num -= y->num;
      break;
    } else if (strcmp(op, "*") == 0) {
      x->num *= y->num;
      break;
    } else if (strcmp(op, "/") == 0) {
      if (y->num == 0) {
        lval_del(x);
        lval_del(y);
        x = lval_err("Division By Zero!");
        break;
      }
      x->num /= y->num;
      break;
    } else if (strcmp(op, "min") == 0) {
      x->num = x->num < y->num ? x->num : y->num;
    } else if (strcmp(op, "max") == 0) {
      x->num = x->num < y->num ? y->num : x->num;
    } else {
      lval_del(x);
      lval_del(y);
      x = lval_err("Unsupported operator");
      break;
    }
    lval_del(y);
  }
  if (a->count > 0) {
    lval_del(x);
    x = lval_err("Too many args");
  }
  lval_del(a);
  return x;
}

lval *builtin_add(lenv *e, lval *a) { return builtin_op(e, a, "+"); }
lval *builtin_sub(lenv *e, lval *a) { return builtin_op(e, a, "-"); }
lval *builtin_mul(lenv *e, lval *a) { return builtin_op(e, a, "*"); }
lval *builtin_div(lenv *e, lval *a) { return builtin_op(e, a, "/"); }
lval *builtin_min(lenv *e, lval *a) { return builtin_op(e, a, "min"); }
lval *builtin_max(lenv *e, lval *a) { return builtin_op(e, a, "max"); }

/* Support Q-Expression:
 * lispy> list 1 2 3 4
 * {1 2 3 4}
 * lispy> head (list 1 2 3 4)
 * {1}
 * lispy> eval {head (list 1 2 3 4)}
 * {1}
 * lispy> eval head (list 1 2 3 4)
 * 1
 * lispy> tail {tail tail tail}
 * {tail tail}
 * lispy> eval (tail {tail tail {5 6 7}})
 * {6 7}
 * lispy> eval (head {(+ 1 2) (+ 10 20)})
 * 3
 */
/*
 * Takes a Q-Expression and returns a Q-Expression with only of the first
 * element
 */
lval *builtin_head(lenv *e, lval *a) {
  LASSERT(a, a->count == 1, "Function 'head' passed too many arguments!");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "Function 'head' passed incorrect type!");
  LASSERT(a, a->cell[0]->count != 0, "Function 'head' passed {}!");

  lval *v = lval_take(a, 0);
  while (v->count > 1) {
    lval_del(lval_pop(v, 1));
  }
  return v;
}

/*
 * Takes a Q-Expression and returns a Q-Expression with the first element
 * removed
 */
lval *builtin_tail(lenv *e, lval *a) {
  LASSERT(a, a->count == 1, "Function 'tail' passed too many arguments!");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "Function 'tail' passed incorrect type!");
  LASSERT(a, a->cell[0]->count != 0, "Function 'tail' passed {}!");

  if (a->cell[0]->count == 0) {
    lval_del(a);
    return lval_err("Function 'tail' passed {}!");
  }

  /* Take first argument */
  lval *v = lval_take(a, 0);
  lval_del(lval_pop(v, 0));
  return v;
}

/*
 * Takes one or more arguments and returns a new Q-Expression containing the
 * arguments
 */
lval *builtin_list(lenv *e, lval *a) {
  a->type = LVAL_QEXPR;
  return a;
}

/*
 * Takes a Q-Expression and evaluates it as if it were a S-Expression
 */
lval *builtin_eval(lenv *e, lval *a) {
  LASSERT(a, a->count == 1, "Function 'eval' passed too many arguments!");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "Function 'eval' passed incorrect type!");
  lval *x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(e, x);
}

lval *lval_join(lval *x, lval *y) {
  while (y->count) {
    x = lval_add(x, lval_pop(y, 0));
  }
  lval_del(y);
  return x;
}

/*
 * Takes one or more Q-Expressions and returns a Q-Expression of them conjoined
 * together
 */
lval *builtin_join(lenv *e, lval *a) {
  for (int i = 0; i < a->count; i++) {
    LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
            "Function 'join' passed incorrect type.");
  }
  lval *x = lval_pop(a, 0);
  while (a->count) {
    x = lval_join(x, lval_pop(a, 0));
  }
  lval_del(a);
  return x;
}

lval *builtin_cons(lenv *e, lval *a) {
  LASSERT(a, a->count > 1, "Function 'cons' passed too few arguments!");
  LASSERT(a, a->cell[1]->type == LVAL_QEXPR && a->cell[0]->type == LVAL_NUM,
          "Function 'cons' passed wrong types!");
  a->type = LVAL_QEXPR;
  return a;
}

lval *builtin_def(lenv *e, lval *a) {
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "Function 'def' passed incorrect type!");

  lval *syms = a->cell[0];
  for (int i = 0; i < syms->count; i++) {
    LASSERT(a, syms->cell[i]->type == LVAL_SYM,
            "Function 'def' cannot define non-symbol");
  }
  /* Check correct number of symbols and values */
  LASSERT(a, syms->count == a->count - 1,
          "Function 'def' cannot define incorrect number of values to symbols");
  for (int i = 0; i < syms->count; i++) {
    lenv_put(e, syms->cell[i], a->cell[i + 1]);
  }
  lval_del(a);
  return lval_sexpr();
}

void lenv_add_builtins(lenv *e) {
  /* List Functions */
  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "tail", builtin_tail);
  lenv_add_builtin(e, "eval", builtin_eval);
  lenv_add_builtin(e, "join", builtin_join);
  lenv_add_builtin(e, "min", builtin_min);
  lenv_add_builtin(e, "max", builtin_max);

  /* Mathematical Functions */
  lenv_add_builtin(e, "+", builtin_add);
  lenv_add_builtin(e, "-", builtin_sub);
  lenv_add_builtin(e, "*", builtin_mul);
  lenv_add_builtin(e, "/", builtin_div);

  /* Variable Functions */
  lenv_add_builtin(e, "def", builtin_def);
}

lval *lval_eval_sexpr(lenv *e, lval *v) {

  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lval_eval(e, v->cell[i]);
  }
  /* Error checking */
  for (int i = 0; i < v->count; i++) {
    if (v->cell[i]->type == LVAL_ERR) {
      return lval_take(v, i);
    }
  }

  /* Empty expression */
  if (v->count == 0) {
    return v;
  }

  /* Single expression */
  if (v->count == 1) {
    return lval_take(v, 0);
  }

  /* Ensure first element is symbol */
  lval *f = lval_pop(v, 0);
  if (f->type != LVAL_FUN) {
    lval_del(f);
    lval_del(v);
    return lval_err("First element is not a function!");
  }

  /* Call builtin with operator */
  lval *result = f->fun(e, v);
  lval_del(f);
  return result;
}

int main(int argc, char **argv) {

  /* Create Some Parsers */
  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Symbol = mpc_new("symbol");
  mpc_parser_t *Sexpr = mpc_new("sexpr");
  mpc_parser_t *Qexpr = mpc_new("qexpr");
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
   *   () // empty expression
   *
   * p.s. number could be negative and we allow multiple preceding zeros
   */
  mpca_lang(
      MPCA_LANG_DEFAULT,
      "																										      \
			  number		: /-?[0-9]+/ ;													      \
				symbol		: /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;  	      \
				sexpr 		: '(' <expr>* ')';                            \
				qexpr     : '{' <expr>* '}';                            \
				expr			: <number> | <symbol> | <sexpr> | <qexpr> ;   \
				lispy		  : /^/ <expr>* /$/ ;					      						\
			",
      Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

  puts("Lispy Version 0.0.0.0.1");
  puts("Press Ctrl+c to Exit\n");

  lenv *e = lenv_new();
  lenv_add_builtins(e);

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
      lval *result = lval_eval(e, lval_read(r.output));
      lval_println(result);
      lval_del(result);

      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);
  }
  lenv_del(e);

  /* Undefine and delete the parsers */
  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
  return 0;
}
