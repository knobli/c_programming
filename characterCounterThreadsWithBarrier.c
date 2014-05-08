#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>


#define ERROR_SIZE 16384

enum exit_type { PROCESS_EXIT, THREAD_EXIT, NO_EXIT };

static long counter[256];
static pthread_t threadIds[256];
pthread_barrier_t bar;

struct attr {
  char* fileName;
  int counterNr;
};

long readFile(char *fileName);

void cleanup() {
	printf("Wait for threads\n");
	int i;
	for(i = 0; i < 256; i++){
		pthread_t threadId = threadIds[i];
		if(threadId > 0){
			pthread_join(threadId, NULL);
		}
	}
}

void exit_by_type(enum exit_type et) {
  switch (et) {
  case PROCESS_EXIT:
    exit(1);
    break;
  case THREAD_EXIT:
    pthread_exit(NULL);
    break;
  case NO_EXIT:
    printf("continuing\n");
    break;
  default:
    printf("unknown exit_type=%d\n", et);
    exit(2);
    break;
  }
}

/* helper function for dealing with errors */
void handle_error_myerrno(long return_code, int myerrno, const char *msg, enum exit_type et) {
  if (return_code < 0) {
    char extra_msg[ERROR_SIZE];
    char error_msg[ERROR_SIZE];
    const char *error_str = strerror(myerrno);
    if (msg != NULL) {
      sprintf(extra_msg, "%s\n", msg);
    } else {
      extra_msg[0] = '\000';
    }
    sprintf(error_msg, "%sreturn_code=%ld\nerrno=%d\nmessage=%s\n", extra_msg, return_code, myerrno, error_str);
    write(STDOUT_FILENO, error_msg, strlen(error_msg));
    cleanup();
    exit_by_type(et);
  }
}

void handle_thread_error(int retcode, const char *msg, enum exit_type et) {
  if (retcode != 0) {
    handle_error_myerrno(-abs(retcode), retcode, msg, et);
  }
}

void *threadMethod(void* param) {
	struct attr *p_atr = (struct attr* )param;
	char* fileName = p_atr->fileName;
	int index = p_atr->counterNr;
	printf("Start reading %s\n", fileName);
	long counterResult = readFile(fileName);
	counter[index] = counterResult;
	printf("Reading finished for index %d\n", index);

	pthread_barrier_wait(&bar);
}

int main(int argc, char *argv[]) {
	if(argc < 2){
		printf("Please enter a file name\n");
		exit(1);
	}

	if(argc > 257){
		printf("Too many files, limit is 256\n");
		exit(1);
	}

	int ret = pthread_barrier_init(&bar, NULL, argc);
	handle_thread_error(ret, "Could not init barrier", PROCESS_EXIT);

	int fileNumber;
	struct attr atr;
	for(fileNumber = 1; fileNumber < argc; fileNumber++){
		char* fileName = argv[fileNumber];
		printf("Start thread for file: %s\n", fileName);
		pthread_t thread_id;
		atr.fileName = fileName;
		atr.counterNr = (fileNumber - 1);
		if(pthread_create(&thread_id, NULL, (void*) threadMethod, (void*) &atr) != 0){
			fprintf(stderr, "pthread_create failed.\n");
		} else {
			fprintf(stdout, "pthread_create success.\n");
			threadIds[fileNumber - 1] = thread_id;
		}
	}

	pthread_barrier_wait(&bar);

	printf("Collect char counts of threads\n");
	int i;
	long total = 0;
	for(i = 0; i < 256; i++){
		total = total + counter[i];
	}
	printf("Total count is: %ld\n", total);

	ret = pthread_barrier_destroy(&bar);
	handle_thread_error(ret, "Could not destory barrier", PROCESS_EXIT);

	cleanup();
	exit(0);
}

long readFile(char *fileName)
{
	if( access( fileName, F_OK ) == -1 ) {
		printf("File does not exist\n");
		exit(1);
	}
	long counter = 0;
    FILE *file;
    file = fopen(fileName, "r");
    char inputChar = '\0';
    while(inputChar != EOF){
    	inputChar = (unsigned char) fgetc(file);
    	counter = counter + inputChar;
    };
    printf("Count of thread is: %ld\n", counter);
    return counter;
}
