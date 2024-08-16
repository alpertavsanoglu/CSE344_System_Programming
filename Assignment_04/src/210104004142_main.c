#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <sys/time.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>

#define MAX_PATH 1024
#define BUFFER_SIZE 4096

// Struct to store source and destination paths for file tasks
typedef struct{
	char srcPath[MAX_PATH];
	char destPath[MAX_PATH];
}FileTask;

// Shared buffer to hold file tasks and synchronization primitives
typedef struct{
	FileTask *tasks;
	int bufferSize;
	int numTasks;
	int nextTask;
	int done;
	pthread_mutex_t mutex;
	pthread_cond_t cond_var;
}SharedBuffer;

volatile sig_atomic_t terminate = 0;	// Signal termination flag

// Struct to hold statistics about the copying process
typedef struct{
	int numRegularFiles;
	int numFifoFiles;
	int numDirectories;
	int numSymbolicLinks;
	size_t totalBytesCopied;
}Statistics;

Statistics stats;	// Global statistics variable

void sig_handler(int signum){											//function for handling signal
	if (signum == SIGINT || signum == SIGTERM)
		terminate = 1;
}

// Function to clean up allocated resources
void cleanup_resources(SharedBuffer *sharedBuffer){
	free(sharedBuffer->tasks);				// Free the tasks buffer
	pthread_mutex_destroy(&sharedBuffer->mutex);		// Destroy the mutex
	pthread_cond_destroy(&sharedBuffer->cond_var);		// Destroy the condition variable
	//printf("Resources cleaned up\n");
}

// Worker thread function to process file tasks from the buffer
void *worker(void *arg){
	SharedBuffer *sharedBuffer = (SharedBuffer *)arg;
	while(!terminate){						// Loop until termination flag is set
		pthread_mutex_lock(&sharedBuffer->mutex);
		// Wait for tasks to be available or for the done flag to be set
		while(sharedBuffer->nextTask >= sharedBuffer->numTasks && !sharedBuffer->done && !terminate){
			pthread_cond_wait(&sharedBuffer->cond_var, &sharedBuffer->mutex);		// Wait for condition variable
		}
		if(terminate || (sharedBuffer->nextTask >= sharedBuffer->numTasks && sharedBuffer->done)){
			pthread_mutex_unlock(&sharedBuffer->mutex);
			break;
		}
		// Get the next task from the buffer
		FileTask task = sharedBuffer->tasks[sharedBuffer->nextTask++];
		pthread_mutex_unlock(&sharedBuffer->mutex);
		// Open source file
		int srcFD = open(task.srcPath, O_RDONLY);
		if(srcFD == -1){
			fprintf(stderr, "Failed to open source file %s: %s\n", task.srcPath, strerror(errno));
			continue;
		}
		// Open destination file with truncation
		int destFD = open(task.destPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if(destFD == -1){
			fprintf(stderr, "Failed to open destination file %s: %s\n", task.destPath, strerror(errno));
			close(srcFD);
			continue;
		}
		// Copy data from source to destination
		char buffer[BUFFER_SIZE];
		ssize_t bytesRead, bytesWritten;
		size_t totalBytes = 0;
		while((bytesRead = read(srcFD, buffer, sizeof(buffer))) > 0){
			bytesWritten = write(destFD, buffer, bytesRead);
			if(bytesWritten != bytesRead){
				fprintf(stderr, "Error writing to destination file %s: %s\n", task.destPath, strerror(errno));
				break;
			}
			totalBytes += bytesWritten;
		}
		close(srcFD);
		close(destFD);
		// Update statistics
		pthread_mutex_lock(&sharedBuffer->mutex);
		stats.totalBytesCopied += totalBytes;
		pthread_mutex_unlock(&sharedBuffer->mutex);
	}
	return NULL;
}

// Function to check if a directory exists
int check_directory_exists(const char *path){
	struct stat info;
	if(stat(path, &info) != 0){
		fprintf(stderr, "Cannot access %s: %s\n", path, strerror(errno));
		return 0;
	}
	else if(info.st_mode & S_IFDIR){
		return 1;
	}
	else{
		fprintf(stderr, "%s is not a directory\n", path);
		return 0;
	}
}
// Function to add a task to the shared buffer
void add_task(SharedBuffer *sharedBuffer, const char *srcPath, const char *destPath){
	pthread_mutex_lock(&sharedBuffer->mutex);				// Lock the mutex
	// Expand the task buffer if necessary
	if(sharedBuffer->numTasks >= sharedBuffer->bufferSize){
		sharedBuffer->bufferSize *= 2;
		sharedBuffer->tasks = realloc(sharedBuffer->tasks, sharedBuffer->bufferSize * sizeof(FileTask));
		if(sharedBuffer->tasks == NULL){
			fprintf(stderr, "Failed to expand task buffer\n");
			exit(EXIT_FAILURE);
		}
	}
	// Add the new task to the buffer
	snprintf(sharedBuffer->tasks[sharedBuffer->numTasks].srcPath, MAX_PATH, "%s", srcPath);
	snprintf(sharedBuffer->tasks[sharedBuffer->numTasks].destPath, MAX_PATH, "%s", destPath);
	sharedBuffer->numTasks++;
	pthread_cond_signal(&sharedBuffer->cond_var);			// Signal that a new task is available
	pthread_mutex_unlock(&sharedBuffer->mutex);			// Unlock the mutex
}

// Function to load tasks from a directory into the buffer
void load_tasks_from_directory(const char *srcDir, const char *destDir, SharedBuffer *sharedBuffer){
	DIR *dir = opendir(srcDir);				// Open the source directory
	if(dir == NULL){
		fprintf(stderr, "Failed to open directory %s: %s\n", srcDir, strerror(errno));
		return;
	}
	struct dirent *entry;
	while((entry = readdir(dir)) != NULL){							// Read each entry in the directory
		if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
			continue;
		}
		char srcPath[MAX_PATH];
		char destPath[MAX_PATH];
		snprintf(srcPath, MAX_PATH, "%s/%s", srcDir, entry->d_name);			// Create source path
		snprintf(destPath, MAX_PATH, "%s/%s", destDir, entry->d_name);			// Create destination path
		if(entry->d_type == DT_DIR){			// If entry is a directory
			mkdir(destPath, 0755);			// Create corresponding directory in destination
			stats.numDirectories++;			// Increment directory count
			load_tasks_from_directory(srcPath, destPath, sharedBuffer);		// Recursively load tasks from the directory
		}
		else if(entry->d_type == DT_REG){			// If entry is a regular file
			add_task(sharedBuffer, srcPath, destPath);	// Add task to buffer
			stats.numRegularFiles++;			// Increment regular file count
		}
		else if(entry->d_type == DT_LNK){		// If entry is a symbolic link
			stats.numSymbolicLinks++;		// Increment symbolic link count
		}
		else if(entry->d_type == DT_FIFO){		// If entry is a FIFO
			stats.numFifoFiles++;			// Increment FIFO count
		}
	}
	closedir(dir);				// Close the directory
}

// Function to check if a path is a directory
int is_directory(const char *path){
	struct stat path_stat;
	stat(path, &path_stat);			// Get file status
	return S_ISDIR(path_stat.st_mode);		// Return if it's a directory
}

// Function to handle source and destination paths, adding tasks to the buffer
void handle_path(const char *src, const char *dest, SharedBuffer *sharedBuffer){
	if(is_directory(src)){						// If the source path is a directory
		load_tasks_from_directory(src, dest, sharedBuffer);			// Load tasks from the directory
	}
	else{						// If the source path is a file
		mkdir(dest, 0755);			// Ensure the destination directory exists
		char *lastSlash = strrchr(src, '/');		// Find the last slash in the source path
		char destFile[MAX_PATH];
		if(lastSlash){
			snprintf(destFile, MAX_PATH, "%s/%s", dest, lastSlash + 1);		// Create destination file path
		}
		else{
			snprintf(destFile, MAX_PATH, "%s/%s", dest, src);		// Create destination file path
		}
		add_task(sharedBuffer, src, destFile);			// Add task to buffer
		stats.numRegularFiles++;			// Count this file as a regular file
	}
}

// Main function to initialize and manage the directory copying utility
int main(int argc, char *argv[]){
	if(argc != 5){
		fprintf(stderr, "Usage: %s <buffer size> <number of workers> <source path> <destination directory>\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	int numWorkers = atoi(argv[2]);
	int userBufferSize = atoi(argv[1]);
	char *srcPath = argv[3];
	char *destDir = argv[4];
	struct sigaction handle_sig;
	handle_sig.sa_handler = sig_handler;
	handle_sig.sa_flags = 0;
	sigemptyset(&handle_sig.sa_mask);
	if (sigaction(SIGINT, &handle_sig, NULL) == -1 || sigaction(SIGTERM, &handle_sig, NULL) == -1){			//signal handling
		perror("error sigaction");
	}
	SharedBuffer sharedBuffer;
	sharedBuffer.bufferSize = userBufferSize;
	sharedBuffer.tasks = malloc(sharedBuffer.bufferSize * sizeof(FileTask));
	sharedBuffer.numTasks = 0;
	sharedBuffer.nextTask = 0;
	sharedBuffer.done = 0;
	pthread_mutex_init(&sharedBuffer.mutex, NULL);
	pthread_cond_init(&sharedBuffer.cond_var, NULL);
	memset(&stats, 0, sizeof(stats));				// Initialize statistics
	
	if(!check_directory_exists(destDir)){
		cleanup_resources(&sharedBuffer);
		return 1;
	}
	
	struct timeval start, end;
	gettimeofday(&start, NULL);			// Start timing
	handle_path(srcPath, destDir, &sharedBuffer);		// Load tasks from the source path
	pthread_t threads[numWorkers];
	
	for(int i = 0; i < numWorkers; i++){
		pthread_create(&threads[i], NULL, worker, &sharedBuffer);		// Create worker threads
	}
	
	pthread_mutex_lock(&sharedBuffer.mutex);
	sharedBuffer.done = 1;
	pthread_cond_broadcast(&sharedBuffer.cond_var);		// Notify workers that task loading is done
	pthread_mutex_unlock(&sharedBuffer.mutex);
	
	for (int i = 0; i < numWorkers; i++) {
		pthread_join(threads[i], NULL);		// Wait for all worker threads to complete
	}

	gettimeofday(&end, NULL);		// End timing
	double elapsed_time = (end.tv_sec - start.tv_sec) * 1000.0;
	elapsed_time += (end.tv_usec - start.tv_usec) / 1000.0;
	long seconds = (long)(elapsed_time / 1000.0);
	long minutes = seconds / 60;
	seconds %= 60;
	long milliseconds = (long)(elapsed_time) % 1000;
	// Print statistics
	printf("\n---------------STATISTICS--------------------\n");
	printf("Consumers: %d - Buffer Size: %d\n", numWorkers, userBufferSize);
	printf("Number of Regular Files: %d\n", stats.numRegularFiles);
	printf("Number of FIFO Files: %d\n", stats.numFifoFiles);
	printf("Number of Directories: %d\n", stats.numDirectories);
//	printf("Number of Symbolic Links: %d\n", stats.numSymbolicLinks);
	printf("TOTAL BYTES COPIED: %zu\n", stats.totalBytesCopied);
	printf("TOTAL TIME: %02ld:%02ld.%03ld (min:sec.mili)\n", minutes, seconds, milliseconds);

	cleanup_resources(&sharedBuffer);		// Clean up resources

	return 0;
}
