#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>
#include <time.h>

#define MAX_BUF 1024

void log_record(const char* msg, const char* log_name){				//for saving event to log file
	pid_t p = fork();
	if(p == -1){
		perror("fork failed");							//fork failed
		exit(EXIT_FAILURE);
	}
	else if (p == 0){
		int open_file = open(log_name, O_WRONLY | O_CREAT | O_APPEND, 0644);				//open file for write log info
		if (open_file == -1) {
			perror("Error opening log file");
			exit(EXIT_FAILURE);
		}
		time_t timer = time(NULL);							//in log file for time information
		struct tm *timestrc = localtime(&timer);
		char time_info[64];
		strftime(time_info, sizeof(time_info), "%Y-%m-%d %H:%M:%S", timestrc);
		char msg_log[MAX_BUF];
		int size_log = snprintf(msg_log, MAX_BUF, "[%s] %s\n", time_info, msg);			////for write info to log file
		if (size_log < 0){
			perror("Error formatting log msg");
			close(open_file);
			exit(EXIT_FAILURE);
		}
	        if (write(open_file, msg_log, size_log) == -1){				//checking write info to log file
			perror("Error writing to log file");
			close(open_file);
			exit(EXIT_FAILURE);
		}
		close(open_file);
		exit(EXIT_SUCCESS);
	}
	else{
		wait(NULL);
	}
}

void listCommands(){								// list all of the instructons
    printf("\nCommands List\n\n");
    printf("gtuStudentGrades \"filename\" 				- Create a file to store student grades. (e.g. gtuStudentGrades “grades.txt”)\n");
    printf("addStudentGrade \"Name Surname\" \"Grade\" \"filename\" 	- Add or update a student's grade. (e.g. addStudentGrade “Alper Tavsanoglu” “AA” “grades.txt”)\n");
    printf("searchStudent \"Name Surname\" \"filename\" 		- Search for a student's grade. (e.g. searchStudent “Alper Tavsanoglu” “grades.txt”)\n");
    printf("sortAll \"filename\" 					- Sort the student grades by name or grade in ascending or descending order. (e.g. sortAll “gradest.txt”)\n");
    printf("showAll \"filename\" 					- Display all student grades stored in the file. (e.g. showAll “gradest.txt”)\n");
    printf("listGrades \"filename\" 					- Display the first 5 student grades from the file. (e.g. listGrades “gradest.txt”)\n");
    printf("listSome numOfEntries pageNumber \"filename\" 		- List a specific range of student grades. (e.g. listSome 5 2 “gradest.txt”)\n");
    printf("gtuStudentGrades					- List avaiable commands. (e.g. gtuStudentGrades)\n");
    printf("exit							- Exit the program. (e.g. exit)\n\n");
    log_record("Usage Printed.", "system.log");	//save event to log file
}

typedef struct{
	char name[50];
	char grade[3];
}Student;

int ascen_name(const void *a, const void *b){						//for ascending order of name
	return strcmp(((Student *)a)->name, ((Student *)b)->name);
}
int descen_name(const void *a, const void *b){						//for descending order of name
	return ascen_name(b, a);
}

int grade_ascen(const void *a, const void *b){						//for ascending order of grade
	Student *studentA = (Student *)a;
	Student *studentB = (Student *)b;
	int comp_grade = strcmp(studentA->grade, studentB->grade);				//compare grades
	if (comp_grade == 0) {
		return ascen_name(a, b);
	}
	return comp_grade;
}
int grade_descen(const void *a, const void *b){						//for descending order of grade
    return grade_ascen(b, a);
}

void sortStudentGrades(const char* file_name){				//for sorting student info
	pid_t p = fork();
	if (p == -1){
		perror("fork failed");
		log_record("Fork Failed.(sort)", "system.log");			//save event to log file
		exit(EXIT_FAILURE);
	}
	else if (p == 0){
		int open_file = open(file_name, O_RDONLY);
		if (open_file == -1){
			perror("File cannot be opened");
			log_record("File could not be opened.(sort-No such file or directory)", "system.log");			//save event to log file
			exit(EXIT_FAILURE);
		}
	Student students[100];
	int count = 0;
	char str[MAX_BUF];
	ssize_t read_byt;
	while ((read_byt = read(open_file, str, MAX_BUF - 1)) > 0){							//open file for reading
								str[read_byt] = '\0';
								char* line = strtok(str, "\n");
								while (line != NULL && count < 100) {							//read file for sort
									sscanf(line, "%49[^,], %2s", students[count].name, students[count].grade);
									count++;
									line = strtok(NULL, "\n");
								}
	}
	close(open_file);
	int sort_choice, sort_order;
	printf("Enter 1 to sort by name, 2 to sort by grade: ");				//choice sort by name or grade
	scanf("%d", &sort_choice);
	printf("For ascending order enter 1, for descending enter 2: ");			//choice sort by name or grade
	scanf("%d", &sort_order);
	if (sort_choice == 1){
		if (sort_order == 1){												//if user choice 1 for name sort and ascending sort
			qsort(students, count, sizeof(Student), ascen_name);
			log_record("Sorted completed: student grades in ascending order by name.", "system.log");		//save event to log file
		}
		else{
			qsort(students, count, sizeof(Student), descen_name);							//if user choice 2 for name sort and descending sort
			log_record("Sorted completed: student grades in descending order by name.", "system.log");		//save event to log file
		}
	}
	else if (sort_choice == 2){
		if (sort_order == 1){												//if user choice 2 for grade sort and ascending sort
			qsort(students, count, sizeof(Student), grade_ascen);
			log_record("Sorted completed: student grades in ascending order by grade.", "system.log");		//save event to log file
		}
		else{
			qsort(students, count, sizeof(Student), grade_descen);							//if user choice 2 for grade sort and descending sort
			log_record("Sorted completed: student grades in descending order by grade.", "system.log");		//save event to log file
		}
	}
	else{
		printf("Invalid sort option.\n");											//if enter wrong sort option
		log_record("Entered sort option is invalid.", "system.log");					//save event to log file
		exit(EXIT_FAILURE);
	}
	open_file = open(file_name, O_WRONLY | O_TRUNC);								//open file for write sorted student info 
	if (open_file == -1){
		perror("File cannot be opened for writing");
		log_record("File could not be opened.(sort-No such file or directory)", "system.log");			//save event to log file
		exit(EXIT_FAILURE);
	}
	for (int i = 0; i < count; i++){									//write sorted student info to file
		dprintf(open_file, "%s, %s\n", students[i].name, students[i].grade);
		printf("%s, %s\n", students[i].name, students[i].grade);
	}
	close(open_file);
	log_record("Sorted students writed to the file.", "system.log");					//save event to log file
	printf("Students Sorted Successfully.\n");
	exit(EXIT_SUCCESS);
	}
	else{
		wait(NULL);
	}
}

void createFile(const char* file_name){					//For Creating File
	pid_t p = fork();
	if (p == -1){
		perror("fork failed");
		log_record("Fork Failed.(createfile func)", "system.log");		//save event to log file
		exit(EXIT_FAILURE);							// Failed fork operation
	}
	else if (p == 0){
		int open_file = open(file_name, O_CREAT | O_RDWR  | O_TRUNC, 0644);		// creat file for student grades
		if (open_file == -1){
			perror("File cannot be created");
			log_record("File could not be opened.(createfile func-No such file or directory)", "system.log");		//save event to log file
			exit(EXIT_FAILURE);
		}
		close(open_file);
		log_record("File created.", "system.log");					//save event to log file
		printf("File Created Successfully.\n");
		exit(EXIT_SUCCESS);
	}
	else{
		wait(NULL);									//for child proccess
	}
}

void add_update_student_to_file(const char* name, const char* grade, const char* file_name){		//for adding or updating student
	pid_t p = fork();
	if (p == -1){
		perror("fork failed");							// Failed fork operation
		log_record("Fork failed.(addStudentGrade)", "system.log");			//save event to log file
		exit(EXIT_FAILURE);
	}
	else if (p == 0){
		int open_file = open(file_name, O_RDWR | O_CREAT, 0644);			// creat file for student grades
		if (open_file == -1){
			perror("File cannot be opened");
			log_record("File could not be opened.(addStudentGrade-No such file or directory)", "system.log");		//save event to log file
			exit(EXIT_FAILURE);
		}
		int flag = 0;					//for checking student already existing
		char str[MAX_BUF];
		ssize_t read_byt;
		off_t indx = 0;
		while ((read_byt = read(open_file, str, MAX_BUF - 1)) > 0){
			str[read_byt] = '\0';
			char* line = strtok(str, "\n");
			while (line != NULL){
				if (strstr(line, name) != NULL){				//check for existing student
					flag = 1;
					lseek(open_file, indx, SEEK_SET);
					char cond[MAX_BUF];
					snprintf(cond, MAX_BUF, "%s, %s\n", name, grade);
					write(open_file, cond, strlen(cond));
					break;
				}
				indx += strlen(line) + 1; 					// Move to the next line
				line = strtok(NULL, "\n");
			}
			if (flag) break;
		}
		if (!flag){											//for checking existing student
			char cond[MAX_BUF];
			snprintf(cond, MAX_BUF, "%s, %s\n", name, grade);
			write(open_file, cond, strlen(cond));
		}
		close(open_file);
		log_record("Added/Updated student grade completed.", "system.log");			//save event to log file
		printf("Student Added/Updated Successfully.\n");
		exit(EXIT_SUCCESS);
	}
	else{
	wait(NULL);							//for child proccess
	}
}

void searchStudentGrade(const char* name, const char* file_name){			//for searching student in the file
	pid_t p = fork();
	if (p == -1){
		perror("fork failed");							// Failed fork operation
		log_record("Fork failed.(searchStudent)", "system.log");			//save event to log file
		exit(EXIT_FAILURE);
	}
	else if (p == 0){
		int open_file = open(file_name, O_RDONLY);
		if (open_file == -1){
			perror("File cannot be opened");
			log_record("File could not be opened.(searchStudent-No such file or directory)", "system.log");			//save event to log file
			exit(EXIT_FAILURE);
		}
		char str[MAX_BUF];
		ssize_t read_byt;
		int flag = 0;
		while ((read_byt = read(open_file, str, MAX_BUF - 1)) > 0){
			str[read_byt] = '\0';
			char* line = str;
			while (line){								// check existing student in file
				char* next = strchr(line, '\n');
				if (next) *next = '\0';
					char* chck = strchr(line, ',');				// student stored in file like Alper Tavsanoglu, AA
					if (chck){
						*chck = '\0';
						char* dup_chck = line;
						while (isspace((unsigned char)*dup_chck)) dup_chck++;
							char* last = dup_chck + strlen(dup_chck) - 1;
							while (last > dup_chck && isspace((unsigned char)*last)) last--;
								*(last + 1) = '\0';
							if (strcmp(dup_chck, name) == 0){
								printf("%s, %s\n", dup_chck, chck + 1);			//if student in file flag 1
								flag = 1;
								break;
							}
					}
					if (next) *next = '\n';
 						line = next ? (next + 1) : NULL;
			}
			if (flag) break;
		}
		if (!flag){									// student is not in file
			printf("Student not found.\n");
		}
		close(open_file);
		if (flag){
			log_record("Search completed: student found.", "system.log");			//save event to log file
			printf("Search Completed and Student Found Successfully.\n");
		}
		else{
			log_record("Search completed: student not found.", "system.log");			//save event to log file
		}
		exit(EXIT_SUCCESS);
		}
		else{
			wait(NULL);							//for child proccess
	}
}

void showAll(const char* file_name){				//for display whole students with grades
	pid_t p = fork();
	if (p == -1){
		perror("fork failed");						//fork failed
		log_record("Fork failed.(showAll)", "system.log");			//save event to log file
		exit(EXIT_FAILURE);
	}
	else if (p == 0){
		int open_file = open(file_name, O_RDONLY);				//open file for read student's grade informations
		if (open_file == -1) {
			perror("File cannot be opened");
			log_record("File could not be opened.(showAll-No such file or directory)", "system.log");			//save event to log file
			exit(EXIT_FAILURE);
		}
		printf("All Student Grades\n");
		char str[MAX_BUF];
		ssize_t read_byt;
		while ((read_byt = read(open_file, str, MAX_BUF - 1)) > 0){				//display students info
			str[read_byt] = '\0';
			printf("%s", str);						//print name and grades
		}
		close(open_file);
		log_record("Displayed all student grades.", "system.log");			//save event to log file
		printf("Student Grades Displayed Successfully.\n");
		exit(EXIT_SUCCESS);
	}
	else{
		wait(NULL);									//for child proccess
	}
}

void listGrades(const char* file_name){							//for display first 5 student
	pid_t p = fork();
	if (p == -1){
		perror("fork failed");						//fork failed
		log_record("Fork failed.(listGrades)", "system.log");					//save event to log file
		exit(EXIT_FAILURE);
	}
	else if (p == 0){
		int open_file = open(file_name, O_RDONLY);				//open file for read student's grade informations
		if (open_file == -1){
			perror("File cannot be opened");
			log_record("File could not be opened.(listGrades-No such file or directory)", "system.log");			//save event to log file
			exit(EXIT_FAILURE);
		}
		printf("First Five Student Grades\n");
		char str[MAX_BUF];
		ssize_t read_byt;
		int flag = 0;
		while ((read_byt = read(open_file, str, MAX_BUF - 1)) > 0 && flag < 5){
											str[read_byt] = '\0';
											char* line = strtok(str, "\n");
											while (line != NULL && flag < 5){
															printf("%s\n", line);			//print students
															line = strtok(NULL, "\n");
															flag++;
											}
		}
		close(open_file);
		log_record("Displayed the first five student grades.", "system.log");			//save event to log file
		printf("Displayed Successfully.\n");
		exit(EXIT_SUCCESS);
	}
	else{
		wait(NULL);								//for child proccess
	}
}

void listSome(const char* file_name, int numofEntries, int pageNumber){
	pid_t p = fork();
	if (p == -1){
		perror("fork failed");
		log_record("Fork failed.(listSome)", "system.log");				//save event to log file
		exit(EXIT_FAILURE);
	}
	else if (p == 0){
		int open_file = open(file_name, O_RDONLY);				//open file for read student's grade informations
		if (open_file == -1){
			perror("File cannot be opened");
			log_record("File could not be opened.(listSome-No such file or directory)(", "system.log");			//save event to log file
			exit(EXIT_FAILURE);
		}
		int starting_point = (pageNumber - 1) * numofEntries;				//calculate starting and ending points
		int ending_point = starting_point + numofEntries;
		int flag_point = 0;
		char str[MAX_BUF];
		ssize_t read_byt;
		printf("\nStudents\n");
		while ((read_byt = read(open_file, str, MAX_BUF - 1)) > 0){				
									str[read_byt] = '\0';
									char* line = strtok(str, "\n");
									while (line != NULL){
											if (flag_point >= starting_point && flag_point < ending_point){
												printf("%s\n", line);						//print student's info
											}
											flag_point++;
											if (flag_point >= ending_point){
												break;
											}
											line = strtok(NULL, "\n");
									}
									if (flag_point >= ending_point){
										break;
									}
		}
		close(open_file);
		char logMessage[MAX_BUF];
		snprintf(logMessage, MAX_BUF, "Displayed student grades from entry %d to %d.", starting_point + 1, ending_point);
		log_record(logMessage, "system.log");									//save event to log file
		exit(EXIT_SUCCESS);
	}
	else{
		wait(NULL);								//for child proccess
	}
}

int main(){
	char command[100];
	char name[50];
	char grade[3];
	char filename[50];
	int numofEntries, pageNumber;
	printf("\nWelcome to Student Grade Management System\n");
	printf("Enter gtuStudentGrades command for avaiable commands\n");
while(1){
		printf("\nEnter Command -> ");
		if(fgets(command, sizeof(command), stdin) == NULL){
								printf("Error reading command. Exiting.\n");
								log_record("Error reading command.", "system.log");
								break;
		}
	command[strcspn(command, "\n")] = 0;
	if(sscanf(command, "gtuStudentGrades \"%49[^\"]\"", filename) == 1){							createFile(filename);}
	else if (sscanf(command, "addStudentGrade \"%49[^\"]\" \"%2s\" \"%49[^\"]\"", name, grade, filename) == 3){		add_update_student_to_file(name, grade, filename);}
	else if (sscanf(command, "searchStudent \"%49[^\"]\" \"%49[^\"]\"", name, filename) == 2){				searchStudentGrade(name, filename);}
	else if (sscanf(command, "sortAll \"%49[^\"]\"", filename) == 1){							sortStudentGrades(filename);}
	else if (sscanf(command, "showAll \"%49[^\"]\"", filename) == 1){							showAll(filename);}
	else if (sscanf(command, "listGrades \"%49[^\"]\"", filename) == 1){							listGrades(filename);}
	else if (sscanf(command, "listSome %d %d \"%49[^\"]\"", &numofEntries, &pageNumber, filename) == 3){			listSome(filename, numofEntries, pageNumber);}
	else if (strcmp(command, "gtuStudentGrades") == 0){									listCommands();}
	else if (strcmp(command, "exit") == 0){ printf("Exiting program.\n");
						log_record("Program Finished.", "system.log");
						break;}
	else{
		printf("Unknown command or incorrect format.\n");
		log_record("Wrong Command Type.", "system.log");
	}
}
return EXIT_SUCCESS;
}
