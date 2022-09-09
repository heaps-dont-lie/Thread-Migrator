// Sample Application 8

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "psu_thread.h"

char REMOTE_HOSTNAME[255];

void print_hostname() {
	char buffer[100];
	int ret;
	if ((ret = gethostname(buffer, sizeof(buffer))) == -1) {
		perror("gethostname");
		exit(1);
	}
	printf("Hostname: %s\n", buffer);
}

void *foo(void *arg) {
	int a = (int *) arg;
	int b = 1, c = 2, d = 3, e = 4;
	print_hostname();
	printf("Foo: Value of A = %d\n", a);
	printf("Foo: Value of B = %d\n", b);
	printf("Foo: Value of C = %d\n", c);
	printf("Foo: Value of D = %d\n", d);
	printf("Foo: Value of E = %d\n", e);
	a++;
	b++;
	c++;
	d++;
	e++;
	psu_thread_migrate(REMOTE_HOSTNAME);
	print_hostname();
	int f = 6, g = 7, h = 8;
	printf("Foo: Value of A = %d\n", a);
	printf("Foo: Value of B = %d\n", b);
	printf("Foo: Value of C = %d\n", c);
	printf("Foo: Value of D = %d\n", d);
	printf("Foo: Value of E = %d\n", e);
	printf("Foo: Value of F = %d\n", f);
	printf("Foo: Value of G = %d\n", g);
	printf("Foo: Value of H = %d\n", h);
	return NULL;
}

int main(int argc, char *argv[]) {
	if (argc < 3) {
		printf("Waiting for goodbye\n");
		return 0;
	}

	psu_thread_setup_init(atoi(argv[2]));

	strcpy(REMOTE_HOSTNAME, argv[1]);
	psu_thread_create(foo, (void *) 10);

	printf("Main: Exit\n");

	return 0;
}
