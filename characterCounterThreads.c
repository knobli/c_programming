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
pthread_mutex_t mutex;

struct attr {
  char* fileName;
};

void *readFile(char *fileName);
void increaseCharCount(unsigned char c);
void printResult();
void printResultOfChar(unsigned char c);

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

/* helper function for dealing with errors */
void handle_error(long return_code, const char *msg, enum exit_type et) {
  int myerrno = errno;
  handle_error_myerrno(return_code, myerrno, msg, et);
}

void *threadMethod(void* param) {
	struct attr *p_atr = (struct attr* )param;

	printf("Start reading\n");
	readFile(p_atr->fileName);
	printf("Reading finished\n");
	printResult();
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

	int fileNumber;
	struct attr atr;
	for(fileNumber = 1; fileNumber < argc; fileNumber++){
		char* fileName = argv[fileNumber];
		printf("Start thread for file: %s\n", fileName);
		pthread_t thread_id;
		atr.fileName = fileName;
		if(pthread_create(&thread_id, NULL, (void*) threadMethod, (void*) &atr) != 0){
			fprintf(stderr, "pthread_create failed.\n");
		} else {
			fprintf(stdout, "pthread_create success.\n");
			threadIds[fileNumber - 1] = thread_id;
		}
	}
	cleanup();
	exit(0);
}

void *readFile(char *fileName)
{
	if( access( fileName, F_OK ) == -1 ) {
		printf("File does not exist\n");
		exit(1);
	}
    FILE *file;
    file = fopen(fileName, "r");
    char inputChar = '\0';
    while(inputChar != EOF){
    	inputChar = (char)fgetc(file);
    	increaseCharCount(inputChar);
    };
}

void increaseCharCount(unsigned char c){
	int returnCode;
	returnCode = pthread_mutex_lock(&mutex);
	handle_thread_error(returnCode, "Could not lock mutex", THREAD_EXIT);

    int currCounter = ++counter[c];
    //printf("Increase counter for %c (%d) and is now %d\n", c, c, currCounter);
    struct timespec tim, tim2;
       tim.tv_sec = 0;
       tim.tv_nsec = 100000;
    nanosleep(&tim , &tim2);

    returnCode = pthread_mutex_unlock(&mutex);
    handle_thread_error(returnCode, "Could not release mutex", THREAD_EXIT);
}

void printResult(){
	int i;
	for(i=0; i < 256; i++){
		printResultOfChar(i);
	}
}

void printResultOfChar(unsigned char c){
	int currCounter = counter[c];
	if(currCounter > 0){
		if(c < 33 || c > 126 ){
			printf("Thread<%ld>: Counter for special character (%d) is now %d\n", (long) pthread_self(), c, currCounter);
		} else {
			printf("Thread<%ld>: Counter for %c (%d) is: %d\n", (long) pthread_self(), c, c, currCounter);
		}
	}
}
