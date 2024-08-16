#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <time.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#define FIFO1 "fifo1"
#define FIFO2 "fifo2"

volatile sig_atomic_t check_child = 0;  				// Counter for child process terminations

void terminate(int signum){						// Terminate signal for prooceding messages
	printf("Terminating proceeding messages.\n");
	exit(0);
}

void cleanup_fifos(){							//For cleaning FIFOs
	unlink(FIFO1);							//Clean FIFO1
	unlink(FIFO2);							//Clean FIFO2
}

void sig_hand(int signum){												//Function for signalhand
	pid_t pid;
	int status;
	while((pid = waitpid(-1, &status, WNOHANG)) > 0){								//This call reaps zombie children (if any).
		if(WIFEXITED(status)){											// showing that the child exited normally
			printf("Child with PID %d exited with status %d.\n", pid, WEXITSTATUS(status));			//print signal message
		}
		else if(WIFSIGNALED(status)){										// indicating the process was terminated by a signal.
			printf("Child with PID %d killed by signal %d.\n", pid, WTERMSIG(status));			// print signal message
		}
		check_child++;  											// Increment the counter when a child exits
	}
}

void sig_hand_create(int signum, void (*handler)(int)){									//create signalhand with function
	struct sigaction sa;	
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = handler;
	if(sigaction(signum, &sa, NULL) == -1){										// The program uses sigaction to handle SIGCHLD
		perror("Error setting up signal handler");
		exit(EXIT_FAILURE);
	}
}

void child_process1(){													//process child1 
	
	int fd1 = open(FIFO1, O_RDONLY);										//open FIFO1 to read
	if(fd1 < 0){
		perror("Error opening FIFO1");										//print error message
		exit(1);
	}

	int fd2 = open(FIFO2, O_WRONLY);										//open FIFO2 to write
	if(fd2 < 0){
		perror("Error opening FIFO2");										// print error message
		close(fd1);
		exit(1);
	}

	int arr_size, sum = 0,i,j;
	if(read(fd1, &arr_size, sizeof(arr_size)) < 0){									//check for error while reading number or random numbers
		perror("Error reading number of elements");								//print error message
		close(fd1);
		close(fd2);
		exit(1);
	}

	int numbers[arr_size];
	if(read(fd1, numbers, arr_size * sizeof(int)) < 0){								//check for error while reading random numbers
		perror("Error reading elements");									//print error message
		close(fd1);
		close(fd2);
		exit(1);
	}

	for(i = 0; i < arr_size; i++){											//sum random number
		sum += numbers[i];
	}
	
	printf("Child 1 - Sum: %d\n", sum);										//display child1 result
	flock(fd2, LOCK_EX);												//lock FIFO2
	if(write(fd2, &sum, sizeof(sum)) < 0){										//write result to FIFO2
		perror("Error writing sum");
	}
	flock(fd2, LOCK_UN);												//unlock FIFO2

	close(fd1);
	close(fd2);
	exit(0);
}

void child_process2(){													//process child1
	
	sleep(2);
	int fd2 = open(FIFO2, O_RDONLY);										//open FIFO2 to read
	if(fd2 < 0){
		perror("Error opening FIFO2");										//print error message
		exit(1);
	}

	char command[10] = {0};
	int arr_size, product = 1, rec_sum,i,j;

	if(read(fd2, command, sizeof(command)) < 0){									//check for error while reading command
		perror("Error reading command");									//print error message
		close(fd2);
		exit(1);
	}
	if(strcmp(command, "multiply") != 0){										//check for error while compared command
		printf("Child 2 - Command not recognized.\n");								//print error message
		exit(1);
	}
	printf("Child 2 - Command: %s\n", command);
	read(fd2, &arr_size, sizeof(arr_size));										//read number of random numbers
	int numbers[arr_size];
	read(fd2, numbers, arr_size * sizeof(int));									//read random numbers

	for(i = 0; i < arr_size; i++){											//multiply random numbers with each other
		product *= numbers[i];
	}

	printf("Child 2 - Product: %d\n", product);									//print child2 result
	read(fd2, &rec_sum, sizeof(rec_sum));										//take child1's result
	printf("Child 2 - Sum received: %d\n", rec_sum);

	int res_all = product + rec_sum;										// "child1 result" + "child2 result"
	printf("Final Result: %d\n", res_all);

	close(fd2);
	exit(0);
}

int main(int argc, char *argv[]){
	if(argc != 2){													//check command line argument
		printf("Missing integer argument.\n");
		return 1;
	}
	int arr_size = atoi(argv[1]);
	if(arr_size <= 0){
		printf("Wrong integer argument.\n");
		return 1;
	}
	cleanup_fifos();												//clean old FIFO if exist
	if(mkfifo(FIFO1, 0666) < 0 || mkfifo(FIFO2, 0666) < 0){
		perror("Error creating FIFOs");
		exit(1);
	}

	sig_hand_create(SIGCHLD, sig_hand);										//for create signalhand
	sig_hand_create(SIGTERM, terminate);

	pid_t child1 = fork();												//fork first child
	if(child1 == 0){
		sleep(10);												//sleep for 10 second
		child_process1();											//child1 func call
	}
	else if(child1 < 0){
		perror("Error forking child 1");									//print error message while forking
		exit(1);
	}
	
	pid_t child2 = fork();												//fork second child
	if(child2 == 0){
		sleep(10);												//sleep for 10 second
		child_process2();											//child2 func call
	}
	else if(child2 < 0){
		perror("Error forking child 2");									//print error message while forking
		exit(1);
	}

	pid_t p_time = fork();
	if(p_time == 0){
		sig_hand_create(SIGTERM, terminate);
		while(1){
			sleep(2);
			printf("proceeding\n");										//print proceeding message every 2 sec
		}
	}
	
	int fd1 = open(FIFO1, O_WRONLY);										//open FIFO1 for write
	if(fd1 < 0){
		perror("Error opening FIFO1");										//print error message
		exit(1);
	}
	
	int fd2 = open(FIFO2, O_WRONLY);										//open FIFO2 for write
	if(fd2 < 0){
		perror("Error opening FIFO2");										//print error message
		exit(1);
	}
	
	int numbers[arr_size];
	srand(time(NULL));												//for generate random numbers
	printf("Generated Numbers: ");
	for(int i = 0; i < arr_size; i++){
		numbers[i] = rand() % 10;										// Random numbers 0 to 9
		printf("%d ", numbers[i]);										//display random numbers
	}
	printf("\n");

	write(fd2, "multiply\0", 10);											//write multiply command to FIFO2
	write(fd1, &arr_size, sizeof(arr_size));									//write numbers to FIFOs
	write(fd1, numbers, arr_size * sizeof(int));
	write(fd2, &arr_size, sizeof(arr_size));
	write(fd2, numbers, arr_size * sizeof(int));

	close(fd1);
	close(fd2);

	while (check_child < 2) {
		sleep(1);
	}

	kill(p_time, SIGTERM);
	waitpid(p_time, NULL, 0);  											// Wait for the timer process to exit

	printf("All child processes have completed.\n");
	cleanup_fifos();												//for cleaning FIFOs
	return 0;
}
