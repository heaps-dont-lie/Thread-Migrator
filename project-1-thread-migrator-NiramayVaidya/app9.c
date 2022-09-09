// Sample Application 9

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

void *foo5(void *arg) {
	int f = 5;
	printf("arg = %d\n", (int *) arg);
	print_hostname();
	printf("f = %d\n", f);
	printf("Foo5: Entry\n");
	psu_thread_migrate(REMOTE_HOSTNAME);
	f++;
	print_hostname();
	printf("f = %d\n", f);
	printf("Foo5: Exit\n");
	return NULL;
}

void *foo4(void *arg) {
	int e = 4;
	printf("arg = %d\n", (int *) arg);
	print_hostname();
	printf("e = %d\n", e);
	printf("Foo4: Entry\n");
	foo5(((int *) arg) + 50);
	e++;
	print_hostname();
	printf("e = %d\n", e);
	printf("Foo4: Exit\n");
	return NULL;
}

void *foo3(void *arg) {
	int d = 3;
	printf("arg = %d\n", (int *) arg);
	print_hostname();
	printf("d = %d\n", d);
	printf("Foo3: Entry\n");
	foo4(((int *) arg) + 40);
	d++;
	print_hostname();
	printf("d = %d\n", d);
	printf("Foo3: Exit\n");
	return NULL;
}

void *foo2(void *arg) {
	int c = 2;
	printf("arg = %d\n", (int *) arg);
	print_hostname();
	printf("c = %d\n", c);
	printf("Foo2: Entry\n");
	foo3(((int *) arg) + 30);
	c++;
	print_hostname();
	printf("c = %d\n", c);
	printf("Foo2: Exit\n");
	return NULL;
}

void *foo1(void *arg) {
	int b = 1;
	printf("arg = %d\n", (int *) arg);
	print_hostname();
	printf("b = %d\n", b);
	printf("Foo1: Entry\n");
	foo2(((int *) arg) + 20);
	b++;
	print_hostname();
	printf("b = %d\n", b);
	printf("Foo1: Exit\n");
	return NULL;
}

void *foo(void *arg) {
	int a = 0;
	printf("arg = %d\n", (int *) arg);
	print_hostname();
	printf("a = %d\n", a);
	printf("Foo: Entry\n");
	foo1(((int *) arg) + 10);
	a++;
	print_hostname();
	printf("a = %d\n", a);
	printf("Foo: Exit\n");
	return NULL;
}

int main(int argc, char *argv[]) {
	if (argc < 3) {
		printf("Waiting for goodbye\n");
		return 0;
	}

	psu_thread_setup_init(atoi(argv[2]));

	strcpy(REMOTE_HOSTNAME, argv[1]);
	psu_thread_create(foo, NULL);

	printf("Main: Exit\n");

	return 0;
}
