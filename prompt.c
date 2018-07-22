#include <stdio.h>
#include <stdlib.h>

// static qualifier makes this variable local to this file.
static char input[2048];

int main(int argc, char** argv) {
	puts("Lisp Version 0.0.0.0.1");
	puts("Press Ctrl+c to Exit\n");

	while (1) {
		fputs("lispy> ", stdout); 
		fgets(input, 2048, stdin);
		printf("No you're a %s", input);
	}
	return 0;
}
