#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ucontext.h>
#include <pthread.h>

/* New Libraries ----------------*/
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

/*MACROS -------------------*/
#define PORT 8080
#define STKFRSZ 8192
#define IP 14//16 //14
#define SP 7//15 //7
#define BP 6//10 //6
#define WRD 4//8 //4

/* New Global variables/structures --------------*/

/*This struct stores necessary information of client and 
 * server to make the connection. 
 * Global is required in order to access in thread_migrate()*/
struct client_server_info{
	int mode;
	int sckt_cli;
	struct sockaddr_in server_addr;
	struct hostent *server;
}ntwrk_info;

/* Stores necessary information required to create a thread*/
typedef struct psu_thread_info{
        int user_stack[STKFRSZ];
        ucontext_t usr;
} psu_thread_info_t;

/*This global variable stores the address where the context of the thread is saved*/
psu_thread_info_t *thread_info_addr;
static ucontext_t curr;
int bpMig_bpUf; // size difference between the start of psu_thread_migrate() frame and start of user_func() frame.
int rsp_index;  // index value of the SP for the psu_thread_migrate() frame
int rbp_index;  // index value of the BP for the psu_thread_migrate() frame
int bp_lst[25]; // assumed 25 recursion depth.

/*Function declarations*/
void stack_migrate(int stk_ptr, int rsp_rbp);
void generate_and_send_bplist(int size, int start_index);

/*
RIP, EIP = 16, 14 
RSP, ESP = 15, 7
RBP, EBP = 10, 6
*/

//you write the code here
void psu_thread_setup_init(int mode)
{
	//Read from a file to set up the socket connection between the client and the server
	if (mode == 0){ //Client
		ntwrk_info.mode = mode;
		
		/* 
 		* LOCAL HOST SET-UP	
 		* ------------------------------------------
		*ntwrk_info.server_addr.sin_family = AF_INET;
		*ntwrk_info.server_addr.sin_port = htons(PORT);
		* For the time being I have implemented everything on the local host. PLEASE CHANGE THE SETUP LATER!!!
 		*IP address of the server. This info is not known and will be known in thread_migrate() 
		*ntwrk_info.server_addr.sin_addr.s_addr = INADDR_ANY; 
		*/
	}
	
	if (mode == 1){ //Server

		ntwrk_info.mode = mode;
		int sckt_svr;
		sckt_svr = socket(AF_INET, SOCK_STREAM, 0);
		
		struct sockaddr_in server_addr;
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(PORT);
		server_addr.sin_addr.s_addr = INADDR_ANY;

		bind(sckt_svr, (struct sockaddr*) &server_addr, sizeof(server_addr));
		listen(sckt_svr, 2);
	
		int conn_fd = accept(sckt_svr, NULL, NULL);
		psu_thread_info_t *thread =(psu_thread_info_t*)malloc(sizeof(psu_thread_info_t));
		int sp_rsp;
		int sp_rbp;
		int rsp_rbp;
		int mig_uf;
		long cw, sw, tag, datasel;
		thread_info_addr = thread;

		recv(conn_fd, &thread->usr, sizeof(ucontext_t), 0);
		recv(conn_fd, &sp_rsp, sizeof(int), 0);
		recv(conn_fd, &sp_rbp, sizeof(int), 0);
		recv(conn_fd, &rsp_rbp, sizeof(int), 0);
		recv(conn_fd, &mig_uf, sizeof(int), 0);		

		bpMig_bpUf = mig_uf;
		rsp_index = (int)(sp_rsp/4);
                rbp_index = rsp_index + (int)(rsp_rbp/4);
		int stk_buff[STKFRSZ-rsp_index];
		recv(conn_fd, stk_buff, sizeof(stk_buff), 0);
		
		int size; //not using at the moment
		recv(conn_fd, &size, sizeof(int), 0);
		recv(conn_fd, bp_lst, sizeof(bp_lst), 0);

		int j = 0;
		for (int i = 0; i < STKFRSZ; i++){ //change this according to stk_buff
			if (i < rsp_index){
				thread->user_stack[i] = 0;
			}
			else{
				thread->user_stack[i] = stk_buff[j];
				j++;
			}

		}
		
		thread->usr.uc_stack.ss_sp = thread->user_stack;
                thread->usr.uc_stack.ss_size = sizeof(thread->user_stack);
		
		thread->usr.uc_mcontext.gregs[SP] = thread->usr.uc_stack.ss_sp + sp_rsp;
		thread->usr.uc_mcontext.gregs[BP] = thread->usr.uc_stack.ss_sp + sp_rbp;
		

		close(sckt_svr);
	}

	// Write an else statement later to handle any errors in input value of Mode.
	 
	return;
}

int psu_thread_create(void * (*user_func)(void*), void *user_args)
{
	// make thread related setup
	// create thread and start running the function based on *user_func
	if (ntwrk_info.mode == 0){ //client
		psu_thread_info_t *thread = (psu_thread_info_t*) malloc(sizeof(psu_thread_info_t));
		thread_info_addr = thread;
		if(getcontext(&thread->usr) == -1){
			printf("An error has occurred!\n");
			exit(0);
		}
		thread->usr.uc_stack.ss_sp = thread->user_stack;
		thread->usr.uc_stack.ss_size = sizeof(thread->user_stack);
		thread->usr.uc_link = &curr;
		makecontext(&thread->usr, user_func, 1, user_args);
		if(swapcontext(&curr, &thread->usr) == -1){
			printf("An error has occured\n");
			exit(0);
		}
		//printf("Exiting Client Thread Create after freeing the allocated memory\n");
		free(thread);	
	}

	else{ //server

		unsigned int temp1 = thread_info_addr->usr.uc_mcontext.gregs[IP];
		unsigned int temp2 = thread_info_addr->usr.uc_mcontext.gregs[SP];
		unsigned int temp3 = thread_info_addr->usr.uc_mcontext.gregs[BP];
		if(getcontext(&thread_info_addr->usr) == -1){
                	printf("An error has occurred!\n");
                        exit(0);
                }
		thread_info_addr->usr.uc_mcontext.gregs[IP] = (greg_t)temp1;
		thread_info_addr->usr.uc_mcontext.gregs[SP] = (greg_t)temp2;
		thread_info_addr->usr.uc_mcontext.gregs[BP] = (greg_t)temp3;
		thread_info_addr->usr.uc_link = &curr;
		makecontext(&thread_info_addr->usr, (void*)thread_info_addr->usr.uc_mcontext.gregs[IP], 1, user_args); // 1, user_args
                thread_info_addr->usr.uc_mcontext.gregs[SP] = (greg_t)temp2;
                thread_info_addr->usr.uc_mcontext.gregs[BP] = (greg_t)temp3;
		if(swapcontext(&curr, &thread_info_addr->usr) == -1){
                	printf("An error has occured\n");
                        exit(0);
                }
		//printf("Exiting Server Thread Create after freeing the allocated memory\n");
		free(thread_info_addr);
	}
	return 0; 
}

void psu_thread_migrate(const char *hostname)
{
	getcontext(&thread_info_addr->usr);
	if(ntwrk_info.mode == 0){
		ntwrk_info.sckt_cli = socket(AF_INET, SOCK_STREAM, 0);
                ntwrk_info.server = gethostbyname(hostname);
                if (ntwrk_info.server == NULL){
                        fprintf(stderr,"ERROR, no such host");
                        exit(0);
                }
                bzero((char *) &ntwrk_info.server_addr, sizeof(ntwrk_info.server_addr));
                ntwrk_info.server_addr.sin_family = AF_INET;
                bcopy((char *)ntwrk_info.server->h_addr, (char *)&ntwrk_info.server_addr.sin_addr.s_addr, ntwrk_info.server->h_length);
                ntwrk_info.server_addr.sin_port = htons(PORT);

		if (connect(ntwrk_info.sckt_cli, (struct sockaddr *) &ntwrk_info.server_addr, sizeof(ntwrk_info.server_addr)) == -1){
        		printf("There was an error while connecting\n");
                	exit(1);
        	}
		int sp_rsp = (unsigned int)thread_info_addr->usr.uc_mcontext.gregs[SP] - (unsigned int)thread_info_addr->usr.uc_stack.ss_sp;
		int sp_rbp = (unsigned int)thread_info_addr->usr.uc_mcontext.gregs[BP] - (unsigned int)thread_info_addr->usr.uc_stack.ss_sp;
		int rsp_rbp = (unsigned int)thread_info_addr->usr.uc_mcontext.gregs[BP] - (unsigned int)thread_info_addr->usr.uc_mcontext.gregs[SP];
		int mig_uf = *((int*)((int)thread_info_addr->usr.uc_mcontext.gregs[BP])) - (int)thread_info_addr->usr.uc_mcontext.gregs[BP];
		rsp_index = (int)(sp_rsp/4);
		rbp_index = rsp_index + (int)(rsp_rbp/4);
		int size = STKFRSZ - rbp_index;

		send(ntwrk_info.sckt_cli, &thread_info_addr->usr, sizeof(ucontext_t), 0);
		send(ntwrk_info.sckt_cli, &sp_rsp, sizeof(int), 0);
		send(ntwrk_info.sckt_cli, &sp_rbp, sizeof(int), 0);
		send(ntwrk_info.sckt_cli, &rsp_rbp, sizeof(int), 0);
		send(ntwrk_info.sckt_cli, &mig_uf, sizeof(int), 0);
		stack_migrate((int)(sp_rsp/4), (int)(rsp_rbp/4));
		generate_and_send_bplist(size, rbp_index);

		close(ntwrk_info.sckt_cli);
		setcontext(&curr);
	}

	else{
		int i = 0;
		int index = rbp_index;
		while(bp_lst[i] != 0){
			thread_info_addr->user_stack[index] = (int)(&thread_info_addr->user_stack[index]) + bp_lst[i];
			index = index + (int)(bp_lst[i]/4);
			i++;
		}
	}
	return;
}

void stack_migrate(int stk_ptr, int rsp_rbp){
	int sz = STKFRSZ-stk_ptr;
	int stk_buff[sz];
	int j = stk_ptr;
	for(int i = 0; i < sz; i++){
		stk_buff[i] = thread_info_addr->user_stack[j];
		j++;
	}
	send(ntwrk_info.sckt_cli, stk_buff, sizeof(stk_buff), 0);
	return;
}

void generate_and_send_bplist(int size, int start_index){
	int bp_list[size];
	int ptr1 = thread_info_addr->user_stack[start_index];
	int ptr2 = (int)&thread_info_addr->user_stack[start_index];
	int flag = 1;
	for(int i = 0; i < size; i++){
		if ((ptr1 < &thread_info_addr->user_stack[0])||(ptr1 > &thread_info_addr->user_stack[STKFRSZ-1])||(ptr2 < &thread_info_addr->user_stack[0])||(ptr2 > &thread_info_addr->user_stack[STKFRSZ-1])||ptr1 <= ptr2){
			flag = 0;
		}
		if (flag){
			bp_list[i] = ptr1-ptr2;
			start_index = start_index + (int)(bp_list[i]/4);
			ptr1 = thread_info_addr->user_stack[start_index];
			ptr2 = (int)&thread_info_addr->user_stack[start_index];
		}
	
		else{
			bp_list[i] = 0;
		}
	}
	send(ntwrk_info.sckt_cli, &size, sizeof(int), 0);
	send(ntwrk_info.sckt_cli, bp_list, sizeof(bp_list), 0);
	return;
}
