// Sample Application 12

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

void *bar(void *arg) {
	int arr2[50];
	for (int i = 0; i < 50; i++) {
		arr2[i] = i * 10;
	}
	for (int i = 0; i < 25; i++) {
		printf("%d\t", arr2[i]);
	}
	printf("\n");
	print_hostname();
	printf("Bar: Entry\n");
	psu_thread_migrate(REMOTE_HOSTNAME);
	for (int i = 25; i < 50; i++) {
		printf("%d\t", arr2[i]);
	}
	printf("\n");
	print_hostname();
	printf("Bar: Exit\n");
	return NULL;
}

void *foo(void *arg) {
	int arr1[25];
	for (int i = 0; i < 25; i++) {
		arr1[i] = i;
		printf("%d\t", arr1[i]);
	}
	printf("\n");
	print_hostname();
	printf("Foo: Entry\n");
	bar(arg);
	for (int i = 0; i < 25; i++) {
		printf("%d\t", arr1[i]);
	}
	printf("\n");
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
