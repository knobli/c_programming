#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>


#define SEMPERM 0600
#define PERM 0600
#define ERROR_SIZE 16384

int shmid_for_cleanup = 0;
int semid_for_cleanup = 0;

struct data {
  long counter[256];
};
const int SIZE = sizeof(struct data);

void handle_error(int return_code, const char *msg);
int create_sem(key_t key, const char *txt, const char *etxt);
int create_shm(key_t key, const char *txt, const char *etxt);
void show_shm_ctl(int shm_id, const char *txt);
void *readFile(char *fileName, struct data *shm_data, int sem_id);
void increaseCharCount(unsigned char c, struct data *shm_data, int sem_id);
void printResult(struct data *shm_data);
void printResultOfChar(unsigned char c, struct data *shm_data);

void cleanup() {
  if (shmid_for_cleanup > 0) {
    int retcode = shmctl(shmid_for_cleanup, IPC_RMID, NULL);
    handle_error(retcode, "removing of shared memory failed");
    shmid_for_cleanup = 0;
  }

  if (semid_for_cleanup > 0) {
    int retcode = semctl(semid_for_cleanup, 1, IPC_RMID, 0);
    handle_error(retcode, "removing of semaphore failed");
    semid_for_cleanup = 0;
  }
}

/* helper function for dealing with errors */
void handle_error(int return_code, const char *msg) {
  if (return_code < 0) {
    char extra_txt[ERROR_SIZE];
    char error_msg[ERROR_SIZE];
    char *extra_msg = extra_txt;
    int myerrno = errno;
    const char *error_str = strerror(myerrno);
    if (msg != NULL) {
      sprintf(extra_msg, "%s\n", msg);
    } else {
      extra_msg = "";
    }
    sprintf(error_msg, "%sreturn_code=%d\nerrno=%d\nmessage=%s\n", extra_msg, return_code, myerrno, error_str);
    write(STDOUT_FILENO, error_msg, strlen(error_msg));
    cleanup();
    exit(1);
  }
}

int main(int argc, char *argv[]) {
	if(argc != 2){
		printf("Please enter a file name\n");
		exit(1);
	}

	FILE *f;
	f = fopen("semref.dat", "w");
	fwrite("X", 1, 1, f);
	fclose(f);

	key_t sem_key = ftok("./semref.dat", 1);
	handle_error(sem_key, "ftok for semaphore failed");

	int sem_id = create_sem(sem_key, "create semaphore", "semget failed");
	semid_for_cleanup = sem_id;

	int i;
	for(i = 0; i < 250; i++){
		int retcode = semctl(sem_id, i, SETVAL , 1);
		if(retcode < 0){
			printf("Init semaphore %d\n", i);
			handle_error(retcode, "Could not initialize semaphore to 1");
		}

	}

	f = fopen("shmref.dat", "w");
	fwrite("X", 1, 1, f);
	fclose(f);

	key_t shm_key = ftok("./shmref.dat", 1);
	handle_error(shm_key, "ftok shared memory failed");

	int shm_id = create_shm(shm_key, "create shared memory", "shmget failed");
	struct data *shm_data = (struct data *) shmat(shm_id, NULL, 0);

	show_shm_ctl(shm_id, "Show shared memory information:");

	shmid_for_cleanup = shm_id;

	printf("Start reading\n");
	readFile(argv[1], shm_data, sem_id);
	printf("Reading finished\n");
	printResult(shm_data);

	printf("Detach the shared memory\n");
	shmdt(shm_data);

	//cleanup();
	exit(0);
}

void *readFile(char *fileName, struct data *shm_data, int sem_id)
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
    	increaseCharCount(inputChar, shm_data, sem_id);
    };
}

void increaseCharCount(unsigned char c, struct data *shm_data, int sem_id){
	if(c > 126 ){
		return;
	}
	struct sembuf sema;
	int returnCode;
    sema.sem_num = c - 1;
    sema.sem_flg = SEM_UNDO;
    sema.sem_op  = -1;
    returnCode=semop(sem_id, &sema, 1);
	if(returnCode < 0){
		printf("Catching semaphore %d\n", c - 1);
		handle_error(returnCode, "Could not catch semaphore");
	}

    int currCounter = ++shm_data->counter[c];
    //printf("Increase counter for %c (%d) and is now %d\n", c, c, currCounter);
    struct timespec tim, tim2;
       tim.tv_sec = 0;
       tim.tv_nsec = 1000000;
    nanosleep(&tim , &tim2);

    sema.sem_op  = 1;
    returnCode=semop(sem_id, &sema, 1);
    handle_error(returnCode, "Could not release semaphore");
}

int create_sem(key_t key, const char *txt, const char *etxt){
	int semaphore_id = semget(key, 250, IPC_CREAT | SEMPERM);
	handle_error(semaphore_id, etxt);
	printf("%s: shm_id=%d key=%ld\n", txt, semaphore_id, (long) key);
	return semaphore_id;
}

int create_shm(key_t key, const char *txt, const char *etxt) {
  int shm_id = shmget(key, SIZE, IPC_CREAT | PERM);
  handle_error(shm_id, etxt);
  printf("%s: shm_id=%d key=%ld\n", txt, shm_id, (long) key);
  return shm_id;
}

void show_shm_ctl(int shm_id, const char *txt) {
  int retcode;
  struct shmid_ds shmctl_data;
  retcode = shmctl(shm_id, IPC_STAT, &shmctl_data);
  handle_error(retcode, "child shmctl failed");
  struct ipc_perm perms = shmctl_data.shm_perm;
  printf("%s: key=%ld uid=%d gid=%d cuid=%d cgid=%d mode=%d seq=%d\n", txt, (long) perms.__key, (int) perms.uid, (int) perms.gid, (int) perms.cuid, (int) perms.cgid, (int) perms.mode, (int)perms.__seq);
}

void printResult(struct data *shm_data){
	int i;
	for(i=0; i < 250; i++){
		printResultOfChar(i, shm_data);
	}
}

void printResultOfChar(unsigned char c, struct data *shm_data){
	int currCounter = shm_data->counter[c];
	if(currCounter > 0){
		if(c < 33 || c > 126 ){
			printf("Increase counter for special character (%d) and is now %d\n", c, c, currCounter);
		} else {
			printf("Increase counter for %c (%d) and is now %d\n", c, c, currCounter);
		}
	}
}
