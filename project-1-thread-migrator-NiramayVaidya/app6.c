// Sample Application 6

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
	print_hostname();
	printf("Foo5: Entry\n");
	psu_thread_migrate(REMOTE_HOSTNAME);
	print_hostname();
	printf("Foo5: Exit\n");
	return NULL;
}

void *foo4(void *arg) {
	print_hostname();
	printf("Foo4: Entry\n");
	foo5(arg);
	print_hostname();
	printf("Foo4: Exit\n");
	return NULL;
}

void *foo3(void *arg) {
	print_hostname();
	printf("Foo3: Entry\n");
	foo4(arg);
	print_hostname();
	printf("Foo3: Exit\n");
	return NULL;
}

void *foo2(void *arg) {
	print_hostname();
	printf("Foo2: Entry\n");
	foo3(arg);
	print_hostname();
	printf("Foo2: Exit\n");
	return NULL;
}

void *foo1(void *arg) {
	print_hostname();
	printf("Foo1: Entry\n");
	foo2(arg);
	print_hostname();
	printf("Foo1: Exit\n");
	return NULL;
}

void *foo(void *arg) {
	print_hostname();
	printf("Foo: Entry\n");
	foo1(arg);
	print_hostname();
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
