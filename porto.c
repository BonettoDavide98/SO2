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

void sighandler() {
	printf("SEGNALE RICEVUTO DA PORTO\n");
}


int main (int argc, char * argv[]) {
	struct mesg_buffer {
    	long mesg_type;
    	char mesg_text[100];
	};

	srand(time(NULL));

	struct mesg_buffer message; 
	port_id = atoi(argv[4]);
	int docks = 1 + (rand() % atoi(argv[7]));
	int shm_id_aval, shm_id_req;
	int sem_id = atoi(argv[2]);
	int msgq_porto = atoi(argv[3]);
	int fill = atoi(argv[9]);
	int loadtime = atoi(argv[10]);
	struct merce *shm_ptr_aval;
	int *shm_ptr_req;
	int num_merci = atoi(argv[11]);
	key_t mem_key;
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

	//printf("POSIZIONE PORTO %d = %s %s\n", atoi(argv[4]), argv[5], argv[6]);

	/*for(int i = 0; i < 3; i++) {
		sops.sem_op = -1;
		semop(sem_id, &sops, 1);

        //printf("PORTO %d HAS: %d OF %d\n", atoi(argv[4]), shm_ptr_aval[i].qty, i);
        //printf("PORTO %d REQUESTS: %d OF %d\n", atoi(argv[4]), shm_ptr_req[i].qty, i);

		sleep(0);

		sops.sem_op = 1;
		semop(sem_id, &sops, 1);
	}*/

	signal(SIGUSR2, sighandler);

	//start handling ships
	int occupied_docks = 0;
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
		printf("MESSAGE RECEIVED BY PORT : %s\n", message.mesg_text);
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
			removeSpoiled(shm_ptr_aval, port_id, shm_ptr_req[0]);
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
			removeSpoiled(shm_ptr_aval, port_id, shm_ptr_req[0]);
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
		for(int j = 1; j < num_merci + 1; j++) {
			if(shm_ptr_req[j] > 0) {
				printf(" %d TONS OF %d |", shm_ptr_req[j], j);
			}
		}
		printf("\n");
	}

	exit(0);
}

void removeSpoiled(struct merce *available, int portid, int limit) {
	struct timeval currenttime;
	gettimeofday(&currenttime, NULL);
	for(int i = 0; i < limit; i++) {
		if(available[i].type > 0 && available[i].qty > 0) {
			if(available[i].spoildate.tv_sec < currenttime.tv_sec) {
				printf("REMOVED %d TONS OF %d FROM PORT %d DUE TO SPOILAGE\n", available[i].qty, available[i].type, portid);
				available[i].type = -1;
				available[i].qty = 0;
			} else if(available[i].spoildate.tv_sec == currenttime.tv_sec) {
				if(available[i].spoildate.tv_usec <= currenttime.tv_usec) {
					printf("REMOVED %d TONS OF %d FROM PORT %d DUE TO SPOILAGE\n", available[i].qty, available[i].type, portid);
					available[i].type = -1;
					available[i].qty = 0;
				}
			}
		}
	}
}
