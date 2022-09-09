/* An important point to be noted about this implementation is that the server
 * resumes the user function from the next instruction after the call to psu
 * thread migrate i.e. it resumes at the point immediately after the return of
 * psu thread migrate in the user function, instead of resuming within the psu
 * thread migrate function itself
 * This is based on the assumption that psu thread migrate has nothing to
 * execute at the server side, so from a correctness standpoint, nothing is
 * violated as nothing is skipped from having been executed on the server once 
 * the user function resumes
 * This assumption holds true since this implementation itself makes sure that
 * psu thread migrate has nothing to execute at the server side (psu thread
 * migrate does have some initial variable declarations but they pertain to the
 * following client side execution, and have no significance with respect to the
 * server side execution, and hence, their initialization at the server side can
 * be ignored)
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>

#include "psu_thread.h"

extern char REMOTE_HOSTNAME[255];

static const char *filename = "server_sock_info.in";
static const char *hostname_token = "hostname: ";
static const char *port_token = "port: ";
static const char *delim = ": ";
static const char *localhost_ip = "127.0.0.1";
static const char *localhost_alias = "localhost";

static ucontext_t uctx_curr;

psu_thread_info_t thread_info;

/* If any API or system call fails, print the error message and exit
 */
static void error(const char *msg) {
	perror(msg);
	exit(0);
}

/* Parses the server IP and port from the server_sock_info.in file located in
 * the current directory, since the remote hostname IP passed in from the
 * command line is not accessible in the psu thread init function because the
 * command line argument is copied into REMOTE_HOSTNAME after the call to psu
 * thread init in the app, so an extern declaration will not help (psu thread
 * init requires the remote hostname IP in order to establish the socket
 * connection, and moreover, at the server side, the command line will have
 * provided the client machine's IP, which is of no use)
 * The server IP in this file and the IP passed to the client when running from
 * the command line must be the same
 * The server port can be any value from 1024 to 65535 (both limits inclusive)
 */
static void get_server_socket_info(void) {
	FILE *server_sock_info_fp = fopen(filename, "r");
	if (server_sock_info_fp == NULL) {
#if ERROR_LEVEL
		error("Server socket information file open failed\n");
#endif
	}
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	int count = 0;
	char hostname[HOST_NAME_MAX];
	int port = -1;
	while ((read = getline(&line, &len, server_sock_info_fp)) != -1) {
		if (count > 1) {
#if ERROR_LEVEL
			fprintf(stderr, "Server socket information file has more lines than required\n");
#endif
			exit(0);
		}
		if (!count && strstr(line, hostname_token) == NULL) {
#if ERROR_LEVEL
			fprintf(stderr, "Server socket information file formatting incorrect, hostname: <host> not present on 1st line\n>");
#endif
			exit(0);
		}
		else if (!count) {
			char *token;
			token = strtok(line, delim);
			while (token != NULL) {
				strcpy(hostname, token);
				hostname[strcspn(hostname, "\n")] = 0;
				token = strtok(NULL, delim);
			}
#if INFO_LEVEL
			printf("Server hostname: %s\n", hostname);
#endif
			/* Storing the hostname read from the file as part of thread info
			 */
			strcpy(thread_info.hostname, hostname);
		}
		if (count && strstr(line, port_token) == NULL) {
#if ERROR_LEVEL
			fprintf(stderr, "Server socket information file formatting incorrect, port: <port> not present on 2nd line\n>");
#endif
			exit(0);
		}
		else if (count) {
			int token_offset = 0;
			char *token;
			token = strtok(line, delim);
			while (token != NULL) {
				if (token_offset) {
					port = atoi(token);
					if (port < PORT_MIN || port > PORT_MAX) {
#if ERROR_LEVEL
						fprintf(stderr, "Port number in server socket information file not within valid range\n");
#endif
						exit(0);
					}
				}
				token = strtok(NULL, delim);
				token_offset++;
			}
#if INFO_LEVEL
			printf("Server port: %d\n", port);
#endif
			/* Storing the port read from the file as part of thread info
			 */
			thread_info.port = port;
		}
		count++;
	}
	fclose(server_sock_info_fp);
	if (line) {
		free(line);
	}
}

/* Reads the ack from the socket, does error checking on it, and then returns it
 */
static int read_ack(int sock_fd, int debug_lineno) {
	int n = -1, ack = 0; 
	n = read(sock_fd, &ack, sizeof(int));
	if (n < 0) {
#if ERROR_LEVEL
		error("Read from socket failed\n");
#endif
	}
	else if (n < sizeof(int)) {
#if WARN_LEVEL
		fprintf(stdout, "%d: Read less number of bytes from the socket than expected\n", debug_lineno);
#endif
	}
	return ack;
}

/* Writes the ack to the socket, and does error checking on it
 */
static void write_ack(int sock_fd, int debug_lineno) {
	int n = -1, ack = 1;
	n = write(sock_fd, &ack, sizeof(int));
	if (n < 0) {
#if ERROR_LEVEL
		error("Write to socket failed\n");
#endif
	}
	else if (n < sizeof(int)) {
#if WARN_LEVEL
		fprintf(stdout, "%d: Written less number of bytes to the socket than expected\n", debug_lineno);
#endif
	}
}

/* Establishes the socket connection between the client and the server, and 
 * initializes the thread's context by storing the required information (socket
 * and mode) in a thread info struct
 */
void psu_thread_setup_init(int mode) {
	int sockfd, newsockfd, portno;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;
	struct hostent *server;
		
	get_server_socket_info();
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
#if ERROR_LEVEL
		error("Socket open failed\n");
#endif
	}
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(thread_info.port);

	if (!mode) {
		/** Getting the IP address for the server's hostname
		 */
		server = gethostbyname(thread_info.hostname);
		if (server == NULL) {
#if ERROR_LEVEL
			fprintf(stderr, "No such host\n");
#endif
			exit(0);
		}
		bcopy((char *) server->h_addr, (char *) &serv_addr.sin_addr.s_addr, server->h_length);
		/* Connecting to the server
		 */
		if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
#if ERROR_LEVEL
			error("Connection failed\n");
#endif
		}
		/* Storing the socket fd as part of thread info
		 */
		thread_info.sock_fd = sockfd;
	}
	else if (mode == 1) {
		serv_addr.sin_addr.s_addr = INADDR_ANY;
		/* Binding the server to its IP and the port
		 */
		if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
#if ERROR_LEVEL
			error("Bind failed\n");
#endif
		}
		/* Listening for connections
		 */
		listen(sockfd, 5);
		clilen = sizeof(cli_addr);
		/* Accepting the connection from the client
		 */
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
		/* Reusing the hostname struct member of thread info which previously
		 * contained the server hostname to now contain the client hostname,
		 * though not making the check if the connection request obtained is
		 * from the correct client or not here, since REMOTE_HOSTNAME is not yet
		 * available from the command line
		 */
		strcpy(thread_info.hostname, (char *) inet_ntoa(cli_addr.sin_addr));
		if (newsockfd < 0) {
#if ERROR_LEVEL
			error("Accept failed\n");
#endif
		}
		/* Storing the socket fd as part of thread info
		 */
		thread_info.sock_fd = newsockfd;
		close(sockfd);
	}
	else {
#if ERROR_LEVEL
		fprintf(stderr, "mode is incorrect\n");
#endif
		exit(0);
	}
	/* Storing the mode (0/1 -> client/server) as part of thread info
	 */
	thread_info.mode = mode;
	return;
}

/* At the client side, the context switch to the user function is made, whereas
 * at the server side, all the required information to recreate the stack state
 * in order to resume the user function (from where it was left off in the
 * client) is obtained from the client over the established socket connection, 
 * and then the context switch to this function is made
 */
int psu_thread_create(void *(*user_func)(void *), void *user_args) {
	int n = -1, user_func_offset = 0, prev_frame_bp_stack_index = -1;
	thread_info.user_func = (void (*)(void)) user_func;
#if DEBUG_LEVEL
	printf("thread info user_func- %lx\n", (size_t) thread_info.user_func);
	printf("user_func- %lx\n", (size_t) user_func);
#endif
	/* Getting the context for the user function
	 */
	if (getcontext(&thread_info.uctx_user_func) == -1) {
#if ERROR_LEVEL
		error("Get context in psu thread create failed\n");
#endif
	}
	/* Setting the stack for the user function's context
	 */
	thread_info.uctx_user_func.uc_stack.ss_sp = thread_info.user_func_stack;
	thread_info.uctx_user_func.uc_stack.ss_size = sizeof(thread_info.user_func_stack);
	thread_info.uctx_user_func.uc_link = &uctx_curr;
	if (!thread_info.mode) {
		/* Setting the user function to its context
		 */
		makecontext(&thread_info.uctx_user_func, thread_info.user_func, 1, user_args);
		/* Switching to the user function's context
		 */
		if (swapcontext(&uctx_curr, &thread_info.uctx_user_func) == -1) {
			error("Swap context from current context to user_func in psu thread create failed\n");
		}
	}
	else if (thread_info.mode == 1) {
		/* If the command line hostname was passed as localhost, convert to the
		 * corresponding IP- 127.0.0.1
		 */
		if (!strcmp(REMOTE_HOSTNAME, localhost_alias)) {
			strcpy(REMOTE_HOSTNAME, localhost_ip);
		}
		/* Check if the accepted connection is from the correct client or not
		 * Though ideally, this is not the appropriate location to make this
		 * check and it should happen immediately after the connection has been
		 * accepted, but since that is not possible as REMOTE_HOSTNAME is only 
		 * available after the call to psu thread init returns, this is the next
		 * closest location in psu thread create where this check can be made
		 */
		if (strcmp(thread_info.hostname, REMOTE_HOSTNAME)) {
#if ERROR_LEVEL
			fprintf(stdout, "Connection request not accepted from the correct client\n");
#endif
			close(thread_info.sock_fd);
			exit(0);
		}
		/* Receive the user function offset from the client (return address
		 * offset i.e. IP offset with respect to start of user function, to 
		 * resume from)
		 */
		n = read(thread_info.sock_fd, &user_func_offset, sizeof(int));
		if (n < 0) {
#if ERROR_LEVEL
			error("Read from socket failed\n");
#endif
		}
		else if (n < sizeof(int)) {
#if WARN_LEVEL
			fprintf(stdout, "%d: Read less number of bytes from the socket than expected\n", __LINE__);
#endif
		}
		write_ack(thread_info.sock_fd, __LINE__);
		int received_stack_size = 0;
		/* Receive the previous frame's stack size from the client
		 */
		n = read(thread_info.sock_fd, &received_stack_size, sizeof(int));
		if (n < 0) {
#if ERROR_LEVEL
			error("Read from socket failed\n");
#endif
		}
		else if (n < sizeof(int)) {
#if WARN_LEVEL
			fprintf(stdout, "%d: Read less number of bytes from the socket than expected\n", __LINE__);
#endif
		}
		write_ack(thread_info.sock_fd, __LINE__);
#if DEBUG_LEVEL
		printf("received stack size- %d\n", received_stack_size);
#endif
		size_t *received_stack = (size_t *) malloc(received_stack_size);
		if (received_stack == NULL) {
#if ERROR_LEVEL
			error("Malloc for received stack size failed\n");
#endif
		}
		/* Receive the previous frame's stack from the client
		 */
		n = read(thread_info.sock_fd, received_stack, received_stack_size);
		if (n < 0) {
#if ERROR_LEVEL
			error("Read from socket failed\n");
#endif
		}
		else if (n < received_stack_size) {
#if WARN_LEVEL
			fprintf(stdout, "%d: Read less number of bytes from the socket than expected\n", __LINE__);
#endif
		}
		write_ack(thread_info.sock_fd, __LINE__);
#if DEBUG_LEVEL
		printf("received stack-\n");
		for (int i = 0; i < received_stack_size / sizeof(size_t); i++) {
			printf("%lx\t", received_stack[i]);
		}
		printf("\n");
#endif
		size_t *bp_offsets = (size_t *) malloc(received_stack_size);
		/* Receive the previous frame's bp offsets from the client
		 */
		n = read(thread_info.sock_fd, bp_offsets, received_stack_size);
		if (n < 0) {
#if ERROR_LEVEL
			error("Read from socket failed\n");
#endif
		}
		else if (n < received_stack_size) {
#if WARN_LEVEL
			fprintf(stdout, "%d: Read less number of bytes from the socket than expected\n", __LINE__);
#endif
		}
		write_ack(thread_info.sock_fd, __LINE__);
#if DEBUG_LEVEL
		printf("bp offsets-\n");
		for (int i = 0; i < received_stack_size / sizeof(size_t); i++) {
			printf("%lu\t", bp_offsets[i]);
		}
		printf("\n");
#endif
		/* Receive the previous frame's stack base pointer's index from the
		 * client
		 */
		n = read(thread_info.sock_fd, &prev_frame_bp_stack_index, sizeof(int));
		if (n < 0) {
#if ERROR_LEVEL
			error("Read from socket failed\n");
#endif
		}
		else if (n < sizeof(int)) {
#if WARN_LEVEL
			fprintf(stdout, "%d: Read less number of bytes from the socket than expected\n", __LINE__);
#endif
		}
		write_ack(thread_info.sock_fd, __LINE__);
#if DEBUG_LEVEL
		printf("prev frame bp stack index- %d\n", prev_frame_bp_stack_index);
#endif
		/* Close the socket, since it is no longer needed
		 */
		close(thread_info.sock_fd);
		/* Get the user function's context
		 */
		if (getcontext(&thread_info.uctx_user_func) == -1) {
#if ERROR_LEVEL
			error("Get context in psu thread create failed\n");
#endif
		}
#if DEBUG_LEVEL
		printf("user func offset- %d\n", user_func_offset);
		printf("user func- %lx\n", (size_t) user_func);
		printf("user func + user func offset- %lx\n", (size_t) user_func + user_func_offset);
#endif
		/* Link the user function to its context
		 */
		makecontext(&thread_info.uctx_user_func, (void (*)(void)) user_func, 1, user_args);
		/* Perform migration of stack, setting of certain register values (IP,
		 * BP, SP) before swapping to the user function's context
		 */
		size_t curr_bp = thread_info.uctx_user_func.uc_mcontext.gregs[BP];
#if DEBUG_LEVEL
		printf("curr bp- %lx\n", curr_bp);
#endif
#ifdef __x86_64__
		size_t prev_ip = thread_info.user_func_stack[thread_info.uctx_user_func.uc_stack.ss_size / sizeof(size_t) - 2];
#else
		size_t prev_ip = thread_info.user_func_stack[thread_info.uctx_user_func.uc_stack.ss_size / sizeof(size_t) - 3];
#endif
#if DEBUG_LEVEL
		printf("prev ip- %lx\n", prev_ip);
#endif
		/* Migrating the stack by using both the received stack and the base
		 * pointer offsets
		 */
		int k = (received_stack_size / sizeof(size_t)) - 1;
		for (int i = thread_info.uctx_user_func.uc_stack.ss_size / sizeof(size_t) - 1, j = received_stack_size / sizeof(size_t) - 1; i >= (thread_info.uctx_user_func.uc_stack.ss_size - received_stack_size) / sizeof(size_t), j >= 0; i--, j--) {
			if (bp_offsets[k] != -1) {
				thread_info.user_func_stack[i] = (size_t) thread_info.uctx_user_func.uc_stack.ss_sp + bp_offsets[k];
			}
			else {
				thread_info.user_func_stack[i] = received_stack[j];
			}
			k--;
		}
#ifdef __x86_64__
		thread_info.user_func_stack[thread_info.uctx_user_func.uc_stack.ss_size / sizeof(size_t) - 2] = prev_ip;
#else
		thread_info.user_func_stack[thread_info.uctx_user_func.uc_stack.ss_size / sizeof(size_t) - 3] = prev_ip;
#endif
#if DEBUG_LEVEL
		printf("user func stack-\n");
		for (int i = (thread_info.uctx_user_func.uc_stack.ss_size - received_stack_size) / sizeof(size_t); i < thread_info.uctx_user_func.uc_stack.ss_size / sizeof(size_t); i++) {
			printf("%lx\t", thread_info.user_func_stack[i]);
		}
		printf("\n");
#endif
		thread_info.uctx_user_func.uc_mcontext.gregs[IP] = (greg_t) user_func + user_func_offset;
#ifdef __x86_64__
		thread_info.user_func_stack[(thread_info.uctx_user_func.uc_stack.ss_size / sizeof(size_t)) - 3] = curr_bp;
#else
		thread_info.user_func_stack[(thread_info.uctx_user_func.uc_stack.ss_size / sizeof(size_t)) - 4] = curr_bp;
#endif
		thread_info.uctx_user_func.uc_mcontext.gregs[BP] = (greg_t) &thread_info.user_func_stack[prev_frame_bp_stack_index];
		thread_info.uctx_user_func.uc_mcontext.gregs[SP] = (greg_t) &thread_info.user_func_stack[(thread_info.uctx_user_func.uc_stack.ss_size - received_stack_size) / sizeof(size_t)];
		/* Switch to the user function's context
		 */
		if (swapcontext(&uctx_curr, &thread_info.uctx_user_func) == -1) {
			error("Swap context from current context to user func in psu thread create failed\n");
		}
#if DEBUG_LEVEL
		printf("Psu thread create: Exit\n");
#endif
	}
	return 0; 
}

/* At the client side, all the required information for the user function to
 * resume in the server from the location the client left it off at is sent over
 * the established socket connection, whereas nothing is required to be done at
 * the server side
 * Over the socket connection, a request reply method is used wherein the server
 * sends an ACK once it successfully receives some piece of data sent by the
 * client, the client checks for the ACK, if present, sends the next piece of
 * data, and so on, whereas on the other hand, if for some reason, the client
 * sees a NACK, it exits
 */
void psu_thread_migrate(const char *hostname) {
	int n = -1, user_func_offset = 0, ack = 0;
	if (!thread_info.mode) {
		/* Get the user function's context
		 */
		if (getcontext(&thread_info.uctx_user_func) == -1) {
#if ERROR_LEVEL
			error("Get context in psu thread migrate failed\n");
#endif
		}
#if DEBUG_LEVEL
		printf("bp- %llx\n", (greg_t) thread_info.uctx_user_func.uc_mcontext.gregs[BP]);
		printf("ss_sp- %lx\n", (size_t) thread_info.uctx_user_func.uc_stack.ss_sp);
#endif
		int bp_offset = thread_info.uctx_user_func.uc_mcontext.gregs[BP] - (size_t) thread_info.uctx_user_func.uc_stack.ss_sp;
#if DEBUG_LEVEL
		printf("bp offset- %d\n", bp_offset);
#endif
		int ip_offset = bp_offset + sizeof(size_t);
		int ip_stack_index = bp_offset / sizeof(size_t) + 1;
#if DEBUG_LEVEL
		printf("ip offset- %d\n", ip_offset);
		printf("ip stack index- %d\n", ip_stack_index);
#endif
		size_t ip_value = ((size_t *) thread_info.uctx_user_func.uc_stack.ss_sp)[ip_stack_index];
#if DEBUG_LEVEL
		printf("user func- %lx\n", (size_t) thread_info.user_func);
		printf("ip value- %lx\n", ip_value);
#endif
		user_func_offset = ip_value - (size_t) thread_info.user_func;
#if DEBUG_LEVEL
		printf("user func offset- %d\n", user_func_offset);
#endif
		/* Send the user func offset to the server
		 */
		n = write(thread_info.sock_fd, &user_func_offset, sizeof(int));
		if (n < 0) {
#if ERROR_LEVEL
			error("Write to socket failed\n");
#endif
		}
		else if (n < sizeof(int)) {
#if WARN_LEVEL
			fprintf(stdout, "%d: Written less number of bytes to the socket than expected\n", __LINE__);
#endif
		}
		ack = read_ack(thread_info.sock_fd, __LINE__);
		if (ack == 1) {
#if INFO_LEVEL
			printf("ACK for IP received\n");
#endif
		}
		else if (!ack) {
#if ERROR_LEVEL
			error("NACK for IP received\n");
#endif
		}
		int prev_frame_sp_stack_index = bp_offset / sizeof(size_t) + 2;
		int stack_size_to_send = (thread_info.uctx_user_func.uc_stack.ss_size / sizeof(size_t) - prev_frame_sp_stack_index) * sizeof(size_t);
#if DEBUG_LEVEL
		printf("stack size to send- %d\n", stack_size_to_send);
#endif
		/* Send the previous frame's stack size to the server
		 */
		n = write(thread_info.sock_fd, &stack_size_to_send, sizeof(int));
		if (n < 0) {
#if ERROR_LEVEL
			error("Write to socket failed\n");
#endif
		}
		else if (n < sizeof(int)) {
#if WARN_LEVEL
			fprintf(stdout, "%d: Written less number of bytes to the socket than expected\n", __LINE__);
#endif
		}
		ack = read_ack(thread_info.sock_fd, __LINE__);
		if (ack == 1) {
#if INFO_LEVEL
			printf("ACK for previous frame's stack size received\n");
#endif
		}
		else if (!ack) {
#if ERROR_LEVEL
			error("NACK for previous frame's stack size received\n");
#endif
		}
#if DEBUG_LEVEL
		printf("prev frame stack-\n");
		for (int i = prev_frame_sp_stack_index; i < thread_info.uctx_user_func.uc_stack.ss_size / sizeof(size_t); i++) {
			printf("%lx\t", thread_info.user_func_stack[i]);
		}
		printf("\n");
#endif
		/* Send the previous frame's stack to the server
		 */
		n = write(thread_info.sock_fd, &thread_info.user_func_stack[prev_frame_sp_stack_index], stack_size_to_send);
		if (n < 0) {
#if ERROR_LEVEL
			error("Write to socket failed\n");
#endif
		}
		else if (n < stack_size_to_send) {
#if WARN_LEVEL
			fprintf(stdout, "%d: Written less number of bytes to the socket than expected\n", __LINE__);
#endif
		}
		ack = read_ack(thread_info.sock_fd, __LINE__);
		if (ack == 1) {
#if INFO_LEVEL
			printf("ACK for previous frame's stack received\n");
#endif
		}
		else if (!ack) {
#if ERROR_LEVEL
			error("NACK for previous frame's stack received\n");
#endif
		}
		/* Generate offsets from stack base for base pointer values stored in
		 * previous frame's stack
		 */
		size_t *bp_offsets = (size_t *) malloc(stack_size_to_send);
		int j = 0;
		for (int i = prev_frame_sp_stack_index; i < thread_info.uctx_user_func.uc_stack.ss_size / sizeof(size_t); i++) {
			if ((size_t) thread_info.uctx_user_func.uc_stack.ss_sp <= thread_info.user_func_stack[i] && thread_info.user_func_stack[i] <= &thread_info.user_func_stack[thread_info.uctx_user_func.uc_stack.ss_size]) {
				bp_offsets[j] = thread_info.user_func_stack[i] - (size_t) thread_info.uctx_user_func.uc_stack.ss_sp;
			}
			else {
				/* Since it's a size_t (unsigned) array, -1 would actually be
				 * the largest unsigned value that can fit in the range, but
				 * this is okay, as we are only concerned about representing an
				 * impossible offset, and this will still suffice
				 * 0 can't be used because it could be a valid offset value
				 */
				bp_offsets[j] = -1;
			}
			j++;
		}
#if DEBUG_LEVEL
		printf("bp offsets-\n");
		for (j = 0; j < stack_size_to_send / sizeof(size_t); j++) {
			printf("%lu\t", bp_offsets[j]);
		}
		printf("\n");
#endif
		/* Send the previous frame's bp offsets to the server
		 */
		n = write(thread_info.sock_fd, bp_offsets, stack_size_to_send);
		if (n < 0) {
#if ERROR_LEVEL
			error("Write to socket failed\n");
#endif
		}
		else if (n < stack_size_to_send) {
#if WARN_LEVEL
			fprintf(stdout, "%d: Written less number of bytes to the socket than expected\n", __LINE__);
#endif
		}
		ack = read_ack(thread_info.sock_fd, __LINE__);
		if (ack == 1) {
#if INFO_LEVEL
			printf("ACK for previous frame's bp offsets received\n");
#endif
		}
		else if (!ack) {
#if ERROR_LEVEL
			error("NACK for previous frame's bp offsets received\n");
#endif
		}
		int prev_frame_bp_stack_index = (((size_t *) thread_info.uctx_user_func.uc_stack.ss_sp)[bp_offset / sizeof(size_t)] - (size_t) thread_info.uctx_user_func.uc_stack.ss_sp) / sizeof(size_t);
#if DEBUG_LEVEL
		printf("prev frame bp stack index- %d\n", prev_frame_bp_stack_index);
#endif
		/* Send previous frame's stack base pointer's index to the server
		 */
		n = write(thread_info.sock_fd, &prev_frame_bp_stack_index, sizeof(int));
		if (n < 0) {
#if ERROR_LEVEL
			error("Write to socket failed\n");
#endif
		}
		else if (n < sizeof(int)) {
#if WARN_LEVEL
			fprintf(stdout, "%d: Written less number of bytes to the socket than expected\n", __LINE__);
#endif
		}
		ack = read_ack(thread_info.sock_fd, __LINE__);
		if (ack == 1) {
#if INFO_LEVEL
			printf("ACK for previous frame BP stack index received\n");
#endif
		}
		else if (!ack) {
#if ERROR_LEVEL
			error("NACK for previous frame BP stack index received\n");
#endif
		}
		close(thread_info.sock_fd);
		/* Switching to the main's context
		 */
		if (swapcontext(&thread_info.uctx_user_func, &uctx_curr) == -1) {
			error("Swap context from user_func to main's context in psu thread create failed\n");
		}
	}
	return;
}
