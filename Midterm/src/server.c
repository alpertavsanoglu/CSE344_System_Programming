#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define BUFF_SIZE 1024
#define MAX_PATH_LENGTH 100
#define SV_FDR "/tmp/svdir.%d"
#define CL_FDR "/tmp/cldir.%d"

struct mission 
{
    pid_t pid;
    char miss[100];
};

int count_sm = 0, count_rdr = 0, num_of_cl = 0, count_cl = 0, count_or = 0;
char user_in[BUFF_SIZE],fd_nm[50][50],road[50],lg_fl[50];
pid_t cl_id[BUFF_SIZE],w_id[BUFF_SIZE];
sem_t* sm_ar[50][2];
sem_t* sm_cl;
sem_t* sm_sm;
sem_t* sm_lg;
static char sv_ff[(sizeof(SV_FDR) + 20)];
volatile sig_atomic_t cond_k = 0;

int check_num(char const str[]){						//For checking numbers validation
	int i = 0;
	while(str[i] != '\0'){
		if(isdigit(str[i]) == 0)
			return 0;
		i++;
	}
	return 1;
}

int symbolago(char str[], char tmp[4][50]){					//for tokenization
	int rem = 0;
	char *tem = strtok(str, " ");
	while(tem != NULL){
		strcpy(tmp[rem], tem);
		rem++;
		if(rem == 4)
			break;
		tem = strtok(NULL, " ");
	}
	return rem;
}

int helper_upload(char upload_str[4][50], char upload_dr[50]){			//helper function of upload command
	FILE* fd;
	char upload_FF[100];
	char new_dir[50] = "";
	char *tem = strtok(upload_dr, "/");
	while(tem != NULL) {
		strcat(new_dir, "../");
		tem = strtok(NULL, "/");
	}									//current directory to server's directory
	strcat(new_dir, upload_str[1]);
	if(access(new_dir, F_OK) != 0) 
		return -1;
	sprintf(upload_FF, "cp %s %s", new_dir, "./");
	fd = popen(upload_FF, "r");    						//open file for reading
	pclose(fd);								//close file
	return 0;
}

int helper_download(char download_str[4][50], char download_dr[50]){			//helper function of download command
	FILE* fd;									
	char download_FF[100];
	if(access(download_str[1], F_OK) != 0) 
		return -1;
	char new_dir[50] = "";
	char *tmp = strtok(download_dr, "/");
	while(tmp != NULL) {
		strcat(new_dir, "../");
		tmp = strtok(NULL, "/");						//for server directory
	}
	sprintf(download_FF, "cp %s %s", download_str[1], new_dir);
	fd = popen(download_FF, "r");							//open file for reading
	pclose(fd);									//close file
	return 0;
}

void help_cases(char help_str[50], char helper_out[]){					//helper function of help methot
	switch(help_str[0]){
		case 'l':
			if(strcmp("list", help_str) == 0)
				strcpy(helper_out, "list\n -display the list of files in Server's directory\n");
			else
				strcpy(helper_out, "invalid command\n");
		break;
		case 'r':
			if(strcmp("readF", help_str) == 0)
				strcpy(helper_out, "readF <file> <line #>\n -display the content of the <file> at the <line#>\n");
			else
				strcpy(helper_out, "invalid command\n");
		break;
		case 'w':
			if(strcmp("writeT", help_str) == 0)
				strcpy(helper_out, "writeT <file> <line #> <string (without spaces and \")> \n -write the <string> to the <line #> of the <file>\n");
			else
				strcpy(helper_out, "invalid command\n");
		break;
		case 'd':
			if(strcmp("download", help_str) == 0)
				strcpy(helper_out, "download <file>\n -download the <file> in Server's directory\n");
			else
				strcpy(helper_out, "invalid command\n");
		break;
		case 'u':
			if(strcmp("upload", help_str) == 0)
				strcpy(helper_out, "upload <file>\n -upload the <file> to Server's directory\n");
			else
				strcpy(helper_out, "invalid command\n");
		break;
		case 'q':
			if(strcmp("quit", help_str) == 0)
				strcpy(helper_out, "quit\n -terminate the program\n");
			else
				strcpy(helper_out, "invalid command\n");
		break;
		case 'k':
			if(strcmp("killServer", help_str) == 0)
				strcpy(helper_out, "killServer\n -terminate the Server\n");
			else
				strcpy(helper_out, "invalid command\n");
		break;
		case 'a':
			if(strcmp("archive", help_str) == 0)
				strcpy(helper_out, "archive <file.tar>\n -create an archive of the Server's current contents and store them in <file.tar>\n");
			else
				strcpy(helper_out, "invalid command\n");
		break;
		default:
			strcpy(helper_out, "invalid command\n");
		break;
	}
}

int helper_archive(char arch_name[50]){						//helper function for archive command
	pid_t pid;
	int helper_chck;
	pid = fork();								//fork operation
	if(pid == 0){
		char full_command[MAX_PATH_LENGTH];
		sprintf(full_command, "tar -cf %s .", arch_name);
		system(full_command);
		exit(EXIT_SUCCESS);
	}
	else{
		if(waitpid(pid, &helper_chck, 0) == -1){
			perror("waitpid");
			exit(EXIT_FAILURE);
		}
		if(WIFEXITED(helper_chck) && WEXITSTATUS(helper_chck) == EXIT_SUCCESS){		//error checks
			return 0;
		}
		else{
			return -1;
		}
	}
}

void sig_handler(int signum){								//Function for signal handling
	switch(signum){
		case SIGINT:
			puts("\nSIGINT is received...bye...\n");				//handle SIGINT
			cond_k = 1;
		break;
		case SIGTERM:
			puts("\nSIGTERM is received...bye...\n");				//handle SIGTERM
			cond_k = 1;
		break;
		case SIGUSR1:
			puts("Kill signal is received...bye...\n");				//handle SIGUSR1
			cond_k = 1;
		break;
	}
}

int sm_controller(char sm_str[]){								//check has semaphore or not
	int i = 0;
	while(i < count_sm){
		if(strcmp(sm_str, fd_nm[i]) == 0)
			return i;
		i++;
	}
	return -1;
}

int check_place(pid_t pid, pid_t str[], int val){						//check pid in array or not
	int i = 0;
	while(i < val){
		if (str[i] == pid)
			return 1;
		i++;
	}
	return 0;
}

void request(char miss_comment[], char user_in[]){					//function for operations
	char miss_str[100];
	char miss_sym[4][50];
	strcpy(miss_str, miss_comment);
	int req_rem = symbolago(miss_str, miss_sym);
	if(strcmp("help", miss_sym[0]) == 0 && (req_rem == 1 || req_rem == 2)){				//help command
		if(req_rem == 1)
			strcpy(user_in, "\nAvailable comments are : help, list, readF, writeT, upload, download, archive, quit, killServer"); 		//available commands list
		else {
			char help_str[BUFF_SIZE] = "";
			help_cases(miss_sym[1], help_str);
			strcpy(user_in, help_str);
		}
	}
	else if(strcmp("list", miss_comment) == 0){								//list command
		char list_str[BUFF_SIZE] = "";
		char tem_ls[BUFF_SIZE] = "";
		FILE *fp = popen("ls", "r");
		while(fgets(tem_ls, sizeof(tem_ls), fp) != NULL){
			strcat(list_str, tem_ls);
		}
		pclose(fp);
		strcpy(user_in, list_str); 
	}
	else if(strcmp("readF", miss_sym[0]) == 0 && (req_rem == 3 || req_rem == 2)){				//readF command
		char readF_ex[BUFF_SIZE] = "";
		int readF_rem = sm_controller(miss_sym[1]);
		if(readF_rem == -1){
			sem_wait(sm_sm);
			strcpy(fd_nm[count_sm], miss_sym[1]);
			readF_rem = count_sm;
			count_sm++;
			sem_post(sm_sm);
		}
		sem_wait(sm_ar[readF_rem][0]);
			count_rdr++;
		if(count_rdr == 1){
			sem_wait(sm_ar[readF_rem][1]);
		}
		sem_post(sm_ar[readF_rem][0]);
		FILE *fp = fopen(miss_sym[1], "r");
		if(fp == NULL){
			strcpy(readF_ex, "File not found");				//file didnt opened
			return; 
		}
		int read_line;  
		int exist_line_read = 1;
		char readF_str[BUFF_SIZE];
		strcpy(readF_ex, "");
		if(req_rem == 2)
			read_line = -1;
		else{
			if(check_num(miss_sym[2]) == 1)
				read_line = atoi(miss_sym[2]); 
			else{
				strcpy(readF_ex, "Invalid line number");			//line number is wrong
				return; 
			}
		}
		while(fgets(readF_str, sizeof(readF_str), fp) != NULL){
			if(req_rem == 3){
				if(exist_line_read == read_line){
					strcpy(readF_ex, readF_str);
					break;
				}
				exist_line_read++;
			}
			else
				strcat(readF_ex, readF_str);
		}
		fclose(fp);		
		sem_wait(sm_ar[readF_rem][0]);
		count_rdr--;
		if(count_rdr == 0){
			sem_post(sm_ar[readF_rem][1]);
		}
		sem_post(sm_ar[readF_rem][0]);
		strcpy(user_in, readF_ex); 
	}
	else if(strcmp("writeT", miss_sym[0]) == 0 && (req_rem == 3 || req_rem == 4)){					//writeT command
		char writeT_str[BUFF_SIZE] = "";
		int writeT_rem = sm_controller(miss_sym[1]);
		if(writeT_rem == -1){
			sem_wait(sm_sm);
			strcpy(fd_nm[count_sm], miss_sym[1]);
			writeT_rem = count_sm;
			count_sm++;
			sem_post(sm_sm);
		}
		sem_wait(sm_ar[writeT_rem][1]);
		char temp_writeT_fl[] = "temp.txt";
		char writeT_line[500];
		int write_line; 
		int exist_line_write = 0;
		int writeF_con = 0;
		FILE* write_in_fd = fopen(miss_sym[1], "r");
		FILE* write_tmp_fd = fopen(temp_writeT_fl, "w");
		if(write_in_fd == NULL){
			write_in_fd = fopen(miss_sym[1], "w+");
			writeF_con = 1;
			if(write_in_fd == NULL){
				perror("Failed to open file");					//file didnt opened
				exit(1);
			}
		}
		if(write_tmp_fd == NULL){
			perror("Failed to open file");				//file didnt opened
			exit(1);
		}
		if(req_rem == 4){
			if(check_num(miss_sym[2]) == 1)
				write_line = atoi(miss_sym[2]); 
			else{
				strcpy(writeT_str, "\nInvalid line number");		//line number is wrong
				return; 
			}
		}
		else
			write_line = -1;
		while(fgets(writeT_line, 500, write_in_fd)){
			exist_line_write++;
			if(exist_line_write == write_line){
				fputs(miss_sym[3], write_tmp_fd);
				fputs("\n", write_tmp_fd);
			} 
			fputs(writeT_line, write_tmp_fd);
		}
		if(req_rem == 3){
			if(writeF_con == 0)
				fputs("\n", write_tmp_fd);
		fputs(miss_sym[2], write_tmp_fd);
		}
		fclose(write_in_fd);
		fclose(write_tmp_fd);
		remove(miss_sym[1]);
		rename(temp_writeT_fl, miss_sym[1]);
		strcpy(writeT_str, "\nThe given string is written to the file");				
		sem_post(sm_ar[writeT_rem][1]);
		strcpy(user_in, writeT_str); 
	}
	else if(strcmp("archive", miss_sym[0]) == 0 && req_rem == 2){							//archive command
		int arc_rem = helper_archive(miss_sym[1]);
		if (arc_rem == 0)
			strcpy(user_in, "\nServer side files have been archived.");
		else
			strcpy(user_in, "\nError archiving server side files.");
	}
	else if(strcmp("upload", miss_sym[0]) == 0 && req_rem == 2){							//upload command
		int upload_rem = sm_controller(miss_sym[1]);
		if(upload_rem == -1){	
			sem_wait(sm_sm);
			strcpy(fd_nm[count_sm], miss_sym[1]);
			upload_rem = count_sm;
			count_sm++;
			sem_post(sm_sm);
		}
		sem_wait(sm_ar[upload_rem][0]);
		count_rdr++;
		if(count_rdr == 1){
			sem_wait(sm_ar[upload_rem][1]);
		}
		sem_post(sm_ar[upload_rem][0]);
		int upl_res = helper_upload(miss_sym, road);
		sem_wait(sm_ar[upload_rem][0]);
		count_rdr--;
		if(count_rdr == 0){
			sem_post(sm_ar[upload_rem][1]);
		}
		sem_post(sm_ar[upload_rem][0]);
		if(upl_res == 0)
			strcpy(user_in, "\nFile has been uploaded."); 
		else
			strcpy(user_in, "\nFile is not found."); 
		}
	else if(strcmp("download", miss_sym[0]) == 0 && req_rem == 2){					//download command
		int download_rem = sm_controller(miss_sym[1]);
		if(download_rem == -1){
			sem_wait(sm_sm);
			strcpy(fd_nm[count_sm], miss_sym[1]);
			download_rem = count_sm;
			count_sm++;
			sem_post(sm_sm);
		}
		sem_wait(sm_ar[download_rem][0]);
		count_rdr++;
		if(count_rdr == 1){
			sem_wait(sm_ar[download_rem][1]);
		}
		sem_post(sm_ar[download_rem][0]);
		int down_res = helper_download(miss_sym, road);
		sem_wait(sm_ar[download_rem][0]);
		count_rdr--;
		if(count_rdr == 0){
			sem_post(sm_ar[download_rem][1]);
		}
		sem_post(sm_ar[download_rem][0]);
		if(down_res == 0)
			strcpy(user_in, "\nFile has been downloaded."); 
		else
			strcpy(user_in, "\nFile is not found."); 
	}
	else if(strcmp("quit", miss_comment) == 0){							//quit command
		strcpy(user_in, "\nDisconnected, bye"); 
	}
	else if(strcmp("killServer", miss_comment) == 0){							//killServer command
		strcpy(user_in, "\nServer is terminated, bye"); 
	}
	else{
		strcpy(user_in, "\nThis command is not available, type help to see available ones."); 
	}
}

void server_func(){											//server function
	int ser_FD, reep_FD, cli_FD;
	char ser_tem_F[8], ser_tem_D[8];
	sm_cl = sem_open("/client", O_CREAT, 0644, 1);
	sm_sm = sem_open("/sem", O_CREAT, 0644, 1);
	sm_lg = sem_open("/log", O_CREAT, 0644, 1);
	for(int i = 0; i < 50; i++){
		sprintf(ser_tem_F, "/m%d", i);
		sprintf(ser_tem_D, "/w%d", i);
		sm_ar[i][0] = sem_open(ser_tem_F, O_CREAT, 0644, 1);
		sm_ar[i][1] = sem_open(ser_tem_D, O_CREAT, 0644, 1);
	}
	umask(0);
	snprintf(sv_ff, (sizeof(SV_FDR) + 20), SV_FDR,  getpid());
	mkfifo(sv_ff, S_IRUSR | S_IWUSR | S_IWGRP);
	ser_FD = open(sv_ff, O_RDONLY);
	if(ser_FD == -1){
		printf("Ctrl-C exit signal received\n");					//Handle ctrl+c signal
		exit(1);
	}
	reep_FD = open(sv_ff, O_WRONLY);
	if(signal(SIGPIPE, SIG_IGN) == SIG_ERR){						//hadle signal
		perror("signal error");
		exit(1);
	}
	while(cond_k == 0){
		char clie_FF[(sizeof(CL_FDR) + 20)];
		struct mission req;
		pid_t fork1;
		if(read(ser_FD, &req, sizeof(struct mission)) != sizeof(struct mission)){
			if(cond_k == 0){
				perror("read request error");						//error occured
				exit(1);
			}
			else
				break;
		}
		if(check_place(req.pid, cl_id, count_cl) == 0){
        		sem_wait(sm_cl);
			if(count_cl < num_of_cl){
				cl_id[count_cl++] = req.pid;
				printf("Client PID %d connected\n", req.pid);			//user connected
				fflush(stdout);
				snprintf(clie_FF, (sizeof(CL_FDR) + 20), CL_FDR, req.pid);
				cli_FD = open(clie_FF, O_WRONLY);
				strcpy(user_in, "positive");
				write(cli_FD, &user_in, sizeof(user_in));
				close(cli_FD);
				}
				else{
					printf("Connection request PID %d . Que is full!\n", req.pid);			//que is full for connection
					fflush(stdout);
					snprintf(clie_FF, (sizeof(CL_FDR) + 20), CL_FDR,  req.pid);
					cli_FD = open(clie_FF, O_WRONLY);
					strcpy(user_in, "negative");
					write(cli_FD, &user_in, sizeof(user_in));
					close(cli_FD);
					if(strcmp(req.miss, "1") == 0)
						w_id[count_or++] = req.pid;
				}
				sem_post(sm_cl);
				continue;
			}
	fork1 = fork();
	if(fork1 > 0){									//fork operation
		int status;
		pid_t child_pid;
		do{
			child_pid = wait(&status);
		}while (child_pid == -1 && errno == EINTR);
	}
	else if(fork1 == 0) {
		request(req.miss, user_in);
		snprintf(clie_FF, (sizeof(CL_FDR) + 20), CL_FDR,  req.pid);
		cli_FD = open(clie_FF, O_WRONLY);
		write(cli_FD, &user_in, sizeof(user_in));
		close(cli_FD);
		if(strcmp(req.miss, "killServer") == 0){				//Server terminated
			printf("From Client PID %d, ", req.pid);
			fflush(stdout);
			kill(getppid(), SIGUSR1);					//signal handle
		}
		exit(0);
	}
		sem_wait(sm_lg);
		int fd = open(lg_fl, O_WRONLY | O_APPEND);
		char text_pid[] = "Client Pid : ";
		int len = snprintf(NULL, 0, "%d", req.pid);
		char* pid_str = malloc(len+1);
		snprintf(pid_str, len+1, "%d", req.pid);
		write(fd, text_pid, strlen(text_pid));
		write(fd, pid_str, strlen(pid_str));
		write(fd, "\n", 1);
		char text_comm[] = "Comment : ";
		write(fd, text_comm, strlen(text_comm));
		write(fd, req.miss, strlen(req.miss));
		write(fd, "\n", 1);
		free(pid_str);      
		close(fd);	
		sem_post(sm_lg);
		if(strcmp(req.miss, "quit") == 0){
			sem_wait(sm_cl);
			int flag = 0;
			for(int i = 0; i < count_cl; i++){
				if(cl_id[i] == req.pid){
					flag = 1;
					if(i != count_cl-1)
						cl_id[i] = cl_id[i+1];
				}
				else if(flag == 1 && i != count_cl-1)
					cl_id[i] = cl_id[i+1];
			}
			count_cl--;
			printf("Client PID %d disconnected..\n", req.pid);						//user disconnected
			fflush(stdout);
			if(count_or != 0){
				char temp[50];
				sprintf(temp, "/%d",  w_id[0]);
				sem_t* sem = sem_open(temp, 0);
				sem_post(sem);
				cl_id[count_cl++] = w_id[0];
				printf("Client PID %d connected\n", w_id[0]);				//user connected
				fflush(stdout);
				for(int i = 0; i < count_or; i++){
					if(i != count_or-1)
						w_id[i] = w_id[i+1];
				}
				count_or--;
			}
			sem_post(sm_cl);
		}
	}
	for(int i = 0; i < count_cl; i++)
		kill(cl_id[i], SIGTERM);						//signal handle
	for(int i = 0; i < count_or; i++){
		kill(w_id[i], SIGTERM);								//signal handle
		char temp[50];
		sprintf(temp, "/%d", w_id[0]);
		sem_t* sem = sem_open(temp, 0);
		sem_post(sem);
	}
	close(ser_FD);					//closing semaphores directories
	close(reep_FD);
	unlink(sv_ff);
	sem_close(sm_cl);
	sem_close(sm_sm);
	sem_close(sm_lg);
	sem_unlink("/client");
	sem_unlink("/sem");
	sem_unlink("/log");
	for(int i = 0; i < count_sm; i++){
		sem_close(sm_ar[i][0]);						//closing semaphores and directories
		sem_close(sm_ar[i][1]);
		sprintf(ser_tem_F, "/m%d", i);
		sprintf(ser_tem_D, "/w%d", i);
		sem_unlink(ser_tem_F);
		sem_unlink(ser_tem_D);
	}
	exit(0);
}
int main(int argc, char const *argv[]){							//main function
	struct sigaction handle_sig;
	handle_sig.sa_handler = sig_handler;
	handle_sig.sa_flags = 0;
	sigemptyset(&handle_sig.sa_mask);

	if (sigaction(SIGINT, &handle_sig, NULL) == -1 || sigaction(SIGTERM, &handle_sig, NULL) == -1 || sigaction(SIGUSR1, &handle_sig, NULL) == -1) {		//signal handling
		perror("sigaction error");
		exit(1);
	}
	if(argc!=3 && strcmp(argv[0], "neHosServer")!=0){
		fprintf(stderr, "Usage:\n./neHosServer <dirname> <max. #ofClients>\n");					//printing usage
		exit(EXIT_FAILURE);
	}
	mkdir(argv[1], 0777);
	chdir(argv[1]);
	num_of_cl = atoi(argv[2]);
	printf("\nServer Started PID %d...\nwaiting for clients...\n", getpid());				//server started
	strcpy(road ,argv[1]);
	sprintf(lg_fl, "%d.log",getpid());
	int fd = open(lg_fl, O_WRONLY | O_CREAT, 0666);
	close(fd);
	server_func();
	return 0;
}
