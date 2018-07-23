#include <stdio.h>
#include <stdlib.h>
#include "mpc/mpc.h"

/* If we are compiling on Windows compile these functions */
#ifdef _WIN32
#include <string.h>

static char buffer[2048];

/* Fake readline function */
char* readline(char* prompt) {
	fputs(prompt, stdout);
	fgets(buffer, 2048, stdin);
	char* cpy = malloc(strlen(buffer) + 1);
	strcpy(cpy, buffer);
	cpy[strlen(cpy) - 1] = '\0';
	return cpy;
}

/* Fake add_history function */
void add_history(char* unused) {}

/* Otherwise include the editline headers */
#else
#include <editline/readline.h>
#endif

long eval_op(long x, char* op, long y) {
	if (strcmp(op, "+") == 0) { return x + y; }
	if (strcmp(op, "-") == 0) { return x - y; }
	if (strcmp(op, "*") == 0) { return x * y; }
	if (strcmp(op, "/") == 0) { return x / y; }

	/* min and max could be evaluated recursively */
	if (strcmp(op, "min") == 0) { return x < y ? x : y;}
	if (strcmp(op, "max") == 0) { return x < y ? y : x;}
	return 0;
}

long eval(mpc_ast_t* t) {

	/* If tagged as number return it directly */
	if (strstr(t->tag, "number")) {
		return atoi(t->contents);
	}

	/* operator is always the second child */
	char* op = t->children[1]->contents;

	/* starting evaluating */
	long x = eval(t->children[2]);

	/* iterate the remaining children and combining */
	int i = 3;
	while (strstr(t->children[i]->tag, "expr")) {
		x = eval_op(x, op, eval(t->children[i]));
		i++;
	}
	return x;
}

int main(int argc, char** argv) {

	/* Create Some Parsers */
	mpc_parser_t* Number = mpc_new("number");
	mpc_parser_t* Operator = mpc_new("operator");
	mpc_parser_t* Expr = mpc_new("expr");
	mpc_parser_t* Lispy = mpc_new("lispy");

	/* Define them with the following language */
	/* number could be negative and we allow multiple preceding zeros */
	mpca_lang(MPCA_LANG_DEFAULT,
			"																										  	 \
			  number		: /-?[0-9]+/ ;													 		 \
				operator	: '+' | '-' | '*' | '/' | \"min\" | \"max\"; \
				expr			: <number> | '(' <operator> <expr>+ ')'; 		 \
				lispy		  : /^/ <operator> <expr>+ /$/ ;					     \
			",
			Number, Operator, Expr, Lispy);

	puts("Lispy Version 0.0.0.0.1");
	puts("Press Ctrl+c to Exit\n");

	while(1) {
		// output and get input
		char* input = readline("lispy> ");

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
			mpc_ast_t* a = r.output;
			printf("Result is %li \n", eval(a));
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
