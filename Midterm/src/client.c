#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <semaphore.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ipc.h>

#define BUFF_SIZE 1024
#define DT_LEN 100
#define SV_FDR "/tmp/svdir.%d"
#define CL_FDR "/tmp/cldir.%d"
#define CONNECT_MSG "connect"
#define TRY_CONNECT_MSG "tryConnect"

struct mission{
	pid_t pid;
	char miss[DT_LEN];
};

volatile sig_atomic_t cond_k = 0;

void sig_handler(int signum){											//function for handling signal
	if (signum == SIGINT || signum == SIGTERM)
		cond_k = 1;
}

void errExit(char * errMessage){										//function for printing perror message fastly
	perror(errMessage);
	exit(1);
}

void client_func(const char sv_no[], int cond){									//client function
	int sv_fd, cl_fd, cond_w = 0;
	struct mission job;
	char user_in[DT_LEN], sv_ff[(sizeof(SV_FDR) + 20)], cl_ff[(sizeof(CL_FDR) + 20)],checker[2],inf[BUFF_SIZE];;
	umask(0);
	snprintf(cl_ff, (sizeof(CL_FDR) + 20), CL_FDR, getpid());
	if(mkfifo(cl_ff, S_IRUSR | S_IWUSR | S_IWGRP) == -1 && errno != EEXIST){
		errExit("error mkfifo");
	}
	job.pid = getpid();
	snprintf(sv_ff, (sizeof(SV_FDR) + 20), SV_FDR, atoi(sv_no));
	sv_fd = open(sv_ff, O_WRONLY);
	printf("\nWaiting for Que..\n");							//user waiting for que so entered connect only
	fflush(stdout);
	sprintf(checker, "%d", cond);
	strcpy(job.miss, checker);
	write(sv_fd, &job, sizeof(struct mission));
	cl_fd = open(cl_ff, O_RDONLY);
	read(cl_fd, &inf,sizeof(inf));
	close(cl_fd);
	if(strcmp(inf, "negative") == 0){
		if(cond == 2){
			printf("Que is FULL... bye...\n");							//que is full user terminated so enterede tryConnect
			fflush(stdout);
		}
		else{
			char temp[DT_LEN];
			sprintf(temp, "/%d", getpid());
			sem_t* sem_1 = sem_open(temp, O_CREAT, 0644, 0);
			if(sem_wait(sem_1) == -1){
				if(cond_k == 1){
					printf("Server is Terminated... bye...\n");						//Terminate server cond_k = 1
					fflush(stdout);
				}
			}
			if(cond_k == 0)
				cond_w = 1;
			sem_close(sem_1);
			sem_unlink(temp);
		}
	}
	if(strcmp(inf, "positive") == 0 || cond_w == 1){
		printf(" Connection established:\n");									//connection positive
		fflush(stdout);
        	while(cond_k == 0){
			printf("Enter comment: ");
			fflush(stdout);
			fgets(user_in, sizeof(user_in), stdin);
			user_in[strcspn(user_in, "\n")] = '\0';
			strcpy(job.miss, user_in);
			if(cond_k == 1){
				printf("\nServer Terminated... bye...\n");						//Terminate server cond_k = 1
				fflush(stdout);
					break;
			}
			write(sv_fd, &job, sizeof(struct mission));
			cl_fd = open(cl_ff, O_RDONLY);
			read(cl_fd, &inf,sizeof(inf));
			close(cl_fd);
			printf("%s\n", inf);
			fflush(stdout);
			if(strcmp(user_in, "quit") == 0 || strcmp(user_in, "killServer") == 0)				//user enter quit or killServer command
				break;
			}
	}
	if(cond_k == 1){
		strcpy(job.miss, "quit");
		write(sv_fd, &job, sizeof(struct mission));
		cl_fd = open(cl_ff, O_RDONLY);
		read(cl_fd, &inf,sizeof(inf));
		close(cl_fd);
		printf("%s\n", inf);
		fflush(stdout);
	}
	close(sv_fd);
	unlink(cl_ff);	
}

int main(int argc, char const *argv[]){							//main function
	struct sigaction handle_sig;
	handle_sig.sa_handler = sig_handler;
	handle_sig.sa_flags = 0;
	sigemptyset(&handle_sig.sa_mask);
	if (sigaction(SIGINT, &handle_sig, NULL) == -1 || sigaction(SIGTERM, &handle_sig, NULL) == -1){			//signal handling
		errExit("error sigaction");
	}
	if(argc!=3 && strcmp(argv[0], "neHosClient")!=0){
		fprintf(stderr, "Usage:\n./neHosClient <connect/tryConnect> ServerPID\n");				//print usage
		exit(EXIT_FAILURE);
	}
	int cond = 0;
	if (strcmp(CONNECT_MSG, argv[1]) == 0)	cond = 1;
	else if (strcmp(TRY_CONNECT_MSG, argv[1]) == 0)	cond = 2;
	else{
		perror("Wrong Connection Type (<connect/tryConnect>)");						//wrong user input
		return 1;
	}
	client_func(argv[2], cond);
	return 0;
}
