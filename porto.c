#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <string.h>
#include <sys/time.h>
#include <signal.h>
#include "merce.h"

int port_id;
int master_msgq;
int num_merci;
int *shm_ptr_req;
struct merce *shm_ptr_aval;
int day = 0;
int docks;
int occupied_docks;
int * spoiled;

void reporthandler();
void endreporthandler();


int main (int argc, char * argv[]) {
	struct mesg_buffer {
    	long mesg_type;
    	char mesg_text[100];
	};

	struct mesg_buffer message; 
	port_id = atoi(argv[4]);
	docks = atoi(argv[7]);
	int shm_id_aval, shm_id_req;
	int master_sem_id = atoi(argv[2]);
	int msgq_porto = atoi(argv[3]);
	int fill = atoi(argv[9]);
	int loadtime = atoi(argv[10]);
	num_merci = atoi(argv[11]);
	master_msgq = atoi(argv[12]);
	key_t mem_key;
	spoiled = malloc((1 +num_merci) * sizeof(int));
	struct sembuf sops;
	struct merce *available;
	struct merce *requested;
	struct position pos;

	//setup shared memory access
	if((int) (shm_id_aval = atoi(argv[1])) < 0) {
		printf("*** shmget error porto aval ***\n");
		exit(1);
	}
	if((struct merce *) (shm_ptr_aval = (struct merce *) shmat(shm_id_aval, NULL, 0)) == -1) {
		printf("*** shmat error porto aval ***\n");
		exit(1);
	}
	if((int) (shm_id_req = atoi(argv[8])) < 0) {
		printf("*** shmget error porto req ***\n");
		exit(1);
	}
	if((int *) (shm_ptr_req = (int *) shmat(shm_id_req, NULL, 0)) == -1) {
		printf("*** shmat error porto req ***\n");
		exit(1);
	}

	//setup signal handlers
	signal(SIGUSR2, reporthandler);
	signal(SIGINT, endreporthandler);

	//initialize spoiled
	for(int i = 0; i < num_merci + 1; i++) {
		spoiled[i] = 0;
	}

	sops.sem_num = 0;
	sops.sem_flg = 0;
	sops.sem_op = -1;
	semop(master_sem_id, &sops, 1);

	//start handling ships
	occupied_docks = 0;
	char ship_id[30];
	char operation[20];
	char text[20];
	int queue[docks * 2];
	int front = -1;
	int rear = -1;

	while(1) {
		while(msgrcv(msgq_porto, &message, (sizeof(long) + sizeof(char) * 100), 1, 0) == -1) {
			//loop until message is received
		}
		//printf("MESSAGE RECEIVED BY PORT : %s\n", message.mesg_text);
		strcpy(operation, strtok(message.mesg_text, ":"));
		strcpy(ship_id, strtok(NULL, ":"));

		if(strcmp(operation, "dockrq") == 0) {
			if(rear == (docks * 2) - 1) {
				strcpy(message.mesg_text, "denied:0:0:0");
				msgsnd(atoi(ship_id), &message, (sizeof(long) + sizeof(char) * 100), 0);
			} else {
				if(front == -1) {
					front = 0;
				}
				rear += 1;
				queue[rear] = atoi(ship_id);
				printf("PORT %s ADDED A SHIP TO QUEUE\n", argv[4]);
			}

			
		} else if(strcmp(operation, "dockfree") == 0) {
			printf("PORT %s HAS FINISHED SERVING A SHIP\n", argv[4]);
			strcpy(message.mesg_text, "freetogo");
			msgsnd(atoi(ship_id), &message, (sizeof(long) + sizeof(char) * 100), 0);
			removeSpoiled(shm_ptr_aval, shm_ptr_req[0]);
			occupied_docks -= 1;
		}

		if(occupied_docks < docks && front != -1) {
			printf("PORT %s STARTED SERVING A SHIP\n", argv[4]);
			occupied_docks += 1;
			strcpy(message.mesg_text, "accept");
			strcat(message.mesg_text, ":");
			sprintf(text, "%d", shm_id_req);
			strcat(message.mesg_text, text);
			strcat(message.mesg_text, ":");
			sprintf(text, "%d", shm_id_aval);
			strcat(message.mesg_text, text);
			strcat(message.mesg_text, ":");
			sprintf(text, "%d", loadtime);
			strcat(message.mesg_text, text);
			removeSpoiled(shm_ptr_aval, shm_ptr_req[0]);
			msgsnd(queue[front], &message, (sizeof(long) + sizeof(char) * 100), 0);
			front++;
			if(front > rear) {
				front = -1;
				rear = -1;
			}
		}

		printf("PORT AVAILABLE: |");
		for(int j = 0; j < shm_ptr_req[0]; j++) {
			if(shm_ptr_aval[j].type == 0) {
				j = shm_ptr_req[0];
			} else if(shm_ptr_aval[j].qty > 0) {
				printf(" %d TONS OF %d |", shm_ptr_aval[j].qty, shm_ptr_aval[j].type);
			}
		}
		printf("\n");

		printf("PORT REQUESTS: |");
		for(int j = 1; j < num_merci * 3 + 1; j++) {
			printf(" %d TONS OF %d |", shm_ptr_req[j], j);
			if(j % num_merci == 0) {
				printf("\n");
			}
		}
		printf("\n");
	}

	exit(0);
}

void removeSpoiled(struct merce *available, int limit) {
	struct timeval currenttime;
	gettimeofday(&currenttime, NULL);
	for(int i = 0; i < limit; i++) {
		if(available[i].type > 0 && available[i].qty > 0) {
			if(available[i].spoildate.tv_sec < currenttime.tv_sec) {
				printf("REMOVED %d TONS OF %d FROM PORT DUE TO SPOILAGE\n", available[i].qty, available[i].type);
				spoiled[available[i].type] += available[i].qty;
				available[i].type = -1;
				available[i].qty = 0;
			} else if(available[i].spoildate.tv_sec == currenttime.tv_sec) {
				if(available[i].spoildate.tv_usec <= currenttime.tv_usec) {
					printf("REMOVED %d TONS OF %d FROM PORT DUE TO SPOILAGE\n", available[i].qty, available[i].type);
					spoiled[available[i].type] += available[i].qty;
					available[i].type = -1;
					available[i].qty = 0;
				}
			}
		}
	}
}

void reporthandler() {
	struct mesg_buffer message;
	message.mesg_type = 1;
	char temp[20];
	int tot = 0;

	day++;

	strcpy(message.mesg_text, "p");
	strcat(message.mesg_text, ":");
	sprintf(temp, "%d", port_id);		//port id
	strcat(message.mesg_text, temp);
	strcat(message.mesg_text, ":");
	sprintf(temp, "%d", day);			//current day
	strcat(message.mesg_text, temp);
	strcat(message.mesg_text, ":");
	for(int i = (num_merci * 2) + 1; i <= (num_merci * 3); i++) {
		tot += shm_ptr_req[i];
	}
	sprintf(temp, "%d", tot);
	strcat(message.mesg_text, temp);
	tot = 0;
	strcat(message.mesg_text, ":");		//merce sent
	for(int i = num_merci + 1; i <= (num_merci * 2); i++) {
		tot += shm_ptr_req[i];
	}
	sprintf(temp, "%d", tot);
	strcat(message.mesg_text, temp);	//merce received
	strcat(message.mesg_text, ":");
	sprintf(temp, "%d", docks);
	strcat(message.mesg_text, temp);	//total docks
	strcat(message.mesg_text, ":");
	sprintf(temp, "%d", occupied_docks);
	strcat(message.mesg_text, temp);	//occupied docks

	msgsnd(master_msgq, &message, (sizeof(long) + sizeof(char) * 100), 0);
}

void endreporthandler() {
	//printf("TERMINATING PORTO...\n");
	removeSpoiled(shm_ptr_aval, num_merci * day);
	struct mesg_buffer message;
	message.mesg_type = 1;
	char temp[20];

	strcpy(message.mesg_text, "P");
	for(int i = 1; i < num_merci + 1; i++) {
		strcat(message.mesg_text, ":");
		sprintf(temp, "%d", spoiled[i]);
		strcat(message.mesg_text, temp);
	}

	msgsnd(master_msgq, &message, (sizeof(long) + sizeof(char) * 100), 0);

	exit(0);
}