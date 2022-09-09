#ifndef PSU_THREAD_H
#define PSU_THREAD_H

#ifndef __USE_GNU
#define __USE_GNU
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ucontext.h>
#include <pthread.h>
#include <limits.h>
#include <signal.h>

#ifdef __x86_64__
#ifdef __USE_GNU
#define BP REG_RBP
#define SP REG_RSP
#define IP REG_RIP
#else
#define BP 10
#define SP 15
#define IP 16
#endif
#else
#ifdef __USE_GNU
#define BP REG_EBP
#define SP REG_ESP
#define IP REG_EIP
#define USP REG_UESP
#else
#define BP 6
#define SP 7
#define IP 14
#define USP 17
#endif
#endif

#define DEBUG_LEVEL 0
#define INFO_LEVEL 0
#define WARN_LEVEL 1
#define ERROR_LEVEL 1

#define PORT_MIN 1024
#define PORT_MAX 65535

typedef struct psu_thread_info {
	int mode;
	char hostname[HOST_NAME_MAX];
	int port;
	int sock_fd;
	ucontext_t uctx_user_func;
	size_t user_func_stack[SIGSTKSZ / sizeof(size_t)];
	void (*user_func)(void);
} psu_thread_info_t;

void psu_thread_setup_init(int mode);

int psu_thread_create(void *(*user_func)(void *), void *user_args);

void psu_thread_migrate(const char *hostname);

#endif /* PSU_THREAD_H  */
