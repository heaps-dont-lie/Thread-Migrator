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
	print_hostname();
	switch (a) {
		case 10:
			printf("a = %d\n", a);
			break;
		case 100:
			printf("a = %d\n", a);
			break;
		default:
			break;
	}
	a *= 10;
	psu_thread_migrate(REMOTE_HOSTNAME);
	switch (a) {
		case 10:
			printf("a = %d\n", a);
			break;
		case 100:
			printf("a = %d\n", a);
			break;
		default:
			break;
	}
	a *= 10;
	switch (a) {
		case 10:
			printf("a = %d\n", a);
			break;
		case 100:
			printf("a = %d\n", a);
			break;
		default:
			printf("default a = %d\n", a);
			break;
	}
	print_hostname();
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
