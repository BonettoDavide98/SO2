#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <signal.h>
#include "merce.h"

#define NAVE "nave.o"
#define PORTO "porto.o"
#define TIMER "timer.o"

int main (int argc, char ** argv) {
	struct mesg_buffer {
    	long mesg_type;
    	char mesg_text[100];
	};

	//read parameters from file
	FILE *inputfile;
	struct parameters parameters;

	if(argc == 2) {
		if((inputfile = fopen(argv[1], "r")) == NULL) {
			printf("Error in opening file\n");
			return(1);
		}
		if(read_parameters_from_file(inputfile, &parameters) == 0) {
			printf("Invalid parameters");
		}
	} else {
		printf("Usage: master <filepath>\n");
		return(1);
	}

	struct mesg_buffer message;
	int sem_id, status;
	pid_t child_pid, *kid_pids;
	struct sembuf sops;
    char *args[13];
	char *argss[11];
	char *argst[5];
	int *ports_shm_id_aval = malloc(parameters.SO_PORTI * sizeof(ports_shm_id_aval));
	struct merce **ports_shm_ptr_aval = malloc(parameters.SO_PORTI * sizeof(ports_shm_ptr_aval));
	int *ports_shm_id_req = malloc(parameters.SO_PORTI * sizeof(ports_shm_id_req));
	int **ports_shm_ptr_req = malloc(parameters.SO_PORTI * sizeof(ports_shm_ptr_req));
	struct position *ports_positions = malloc(parameters.SO_PORTI * sizeof(struct position));
    char shm_id_str[3*sizeof(int)+1];
    char shm_id_req_str[3*sizeof(int)+1];
    char sem_id_str[3*sizeof(int)+1];
    char msgq_id_str[3*sizeof(int)];

	int master_msgq;
	char master_msgq_str[3*sizeof(int)+1];

	int *msgqueue_porto = malloc(parameters.SO_PORTI * sizeof(int));
	int *msgqueue_nave = malloc(parameters.SO_NAVI * sizeof(int));

	srand(time(NULL));

	for(int i = 0; i < 13; i++) {
		args[i] = malloc(20);
	}

	for(int i = 0; i < 11; i++) {
		argss[i] = malloc(20);
	}

	for(int i = 0; i < 5; i++) {
		argst[i] = malloc(20);
	}

	sem_id = semget(IPC_PRIVATE, 1, 0600);
	semctl(sem_id, 0, SETVAL, 0);

	sops.sem_num = 0;
	sops.sem_flg = 0;

	kid_pids = malloc((parameters.SO_PORTI + parameters.SO_NAVI) * sizeof(*kid_pids));

	if((master_msgq = msgget(IPC_PRIVATE, IPC_CREAT | 0600)) == -1) {
		printf("*** master msgqueue error ***\n");
		exit(1);
	}

	ports_positions[0].x = 0;
	ports_positions[0].y = 0;

	ports_positions[1].x = parameters.SO_LATO;
	ports_positions[1].y = 0;

	ports_positions[2].x = 0;
	ports_positions[2].y = parameters.SO_LATO;

	ports_positions[3].x = parameters.SO_LATO;
	ports_positions[3].y = parameters.SO_LATO;

	int a,b,tot,temp;
	//create ports
	for(int i = 0; i < parameters.SO_PORTI; i++) {
		if((int) (ports_shm_id_aval[i]  = shmget(IPC_PRIVATE, sizeof(int), 0600)) < 0) {
			printf("*** shmget aval error ***\n");
			exit(1);
		}

		ports_shm_ptr_aval[i] = malloc((10 * parameters.SO_MERCI) * sizeof(struct merce));
		if((struct merce *) (ports_shm_ptr_aval[i] = (struct merce *) shmat(ports_shm_id_aval[i], NULL, 0)) == -1) {
			printf("*** shmat aval error ***\n");
			exit(1);
		}

		if((int) (ports_shm_id_req[i]  = shmget(IPC_PRIVATE, sizeof(int), 0600)) < 0) {
			printf("*** shmget req error ***\n");
			exit(1);
		}

		ports_shm_ptr_req[i] = malloc((parameters.SO_MERCI * 2 + 1) * sizeof(int));
		if((int *) (ports_shm_ptr_req[i] = (int *) shmat(ports_shm_id_req[i], NULL, 0)) == -1) {
			printf("*** shmat req error ***\n");
			exit(1);
		}

		if((msgqueue_porto[i] = msgget(IPC_PRIVATE, 0600)) == -1) {
			printf("*** msgqueue_porto error ***\n");
			exit(1);
		}
		
		if(i > 3) {
			ports_positions[i].x = (rand() / (double)RAND_MAX) * parameters.SO_LATO;
			ports_positions[i].y = (rand() / (double)RAND_MAX) * parameters.SO_LATO;
		}
		
		//printf("PORT CREATED IN %f %f\n", ports_positions[i].x, ports_positions[i].y);

		for(int j = 0; j < parameters.SO_MERCI; j++) {
			ports_shm_ptr_aval[i][j].type = 0;
			ports_shm_ptr_aval[i][j].qty = 0;
		}

		for(int j = 0; j < parameters.SO_MERCI * 2 + 1; j++) {
			ports_shm_ptr_req[i][j] = 0;
		}
		
		int *temparray = malloc((parameters.SO_MERCI + 1) * sizeof(int));
		for(int j = 0; j < parameters.SO_MERCI + 1; j++) {
			temparray[j] = 0;
		}

		tot = 0;
		
		while(tot + parameters.SO_SIZE <= parameters.SO_FILL/parameters.SO_PORTI) {
			temp = 1 + (rand() % parameters.SO_SIZE);
			temparray[1 + (rand() % parameters.SO_MERCI)] += temp;
			tot += temp;
		}

		for(int j = 0; j < parameters.SO_MERCI + 1; j++) {
			printf("TEMP %d: %d\n", j, temparray[j]);
		}

		a = 0;
		for(int j = 1; j < parameters.SO_MERCI + 1; j++) {
			switch(rand() % 2) {
				case 0:
					ports_shm_ptr_aval[i][a].type = j;
					ports_shm_ptr_aval[i][a].qty = temparray[j];
					gettimeofday(&ports_shm_ptr_aval[i][a].spoildate, NULL);
					ports_shm_ptr_aval[i][a].spoildate.tv_sec += rand() % (parameters.SO_MAX_VITA - parameters.SO_MIN_VITA);
					printf("ADDED %d TONS OF %d TO PORT %d\n", temparray[j], j, i);
					printf("%d %d\n", ports_shm_ptr_aval[i][a].qty, ports_shm_ptr_aval[i][a].type);
					a += 1;
					break;
				case 1:
					ports_shm_ptr_req[i][j] = temparray[j];
					printf("ADDED REQUEST OF %d TONS OF %d TO PORT %d\n", temparray[j], j, i);
					break;
			}
		}
		ports_shm_ptr_req[i][0] = a;
		free(temparray);
	}

	//create ships
	for(int j = 0; j < parameters.SO_NAVI; j++) {
		if((msgqueue_nave[j] = msgget(IPC_PRIVATE, IPC_CREAT | 0600)) == -1) {
			printf("*** msgqueue error ***\n");
			exit(1);
		}
	}

	//start port processes
	for(int i = 0; i < parameters.SO_PORTI; i++) {
		strcpy(args[0], PORTO);
		sprintf(args[1], "%d", ports_shm_id_aval[i]);
		sprintf(args[2], "%d", sem_id);
		sprintf(args[3], "%d", msgqueue_porto[i]);
		sprintf(args[4], "%d", i);
		sprintf(args[5], "%f", ports_positions[i].x);
		sprintf(args[6], "%f", ports_positions[i].y);
		sprintf(args[7], "%d", parameters.SO_BANCHINE);
		sprintf(args[8], "%d", ports_shm_id_req[i]);
		sprintf(args[9], "%d", parameters.SO_FILL);
		sprintf(args[10], "%d", parameters.SO_LOADSPEED);
		sprintf(args[11], "%d", (parameters.SO_MERCI + 1));
    	args[12] = NULL;

		switch(kid_pids[i] = fork()) {
			case -1:
				break;
			case 0:
    			execve(PORTO, args, NULL);
				break;
			default:
				break;
		}
	}

	//start ship processes
	for(int j = 0; j < parameters.SO_NAVI; j++) {
		strcpy(argss[0], NAVE);
		sprintf(argss[1], "%d", msgqueue_nave[j]);
		sprintf(argss[2], "%d", j);
		sprintf(argss[3], "%f", (rand() / (double)RAND_MAX) * parameters.SO_LATO);
		sprintf(argss[4], "%f", (rand() / (double)RAND_MAX) * parameters.SO_LATO);
		sprintf(argss[5], "%d", parameters.SO_SPEED);
		sprintf(argss[6], "%d", master_msgq);
		sprintf(argss[7], "%d", parameters.SO_CAPACITY);
		sprintf(argss[8], "%d", parameters.SO_STORM_DURATION);
		sprintf(argss[9], "%d", (parameters.SO_MERCI + 1));
		argss[10] = NULL;

		//printf("TEST: %s %s\n", x, y);
		
		switch(kid_pids[parameters.SO_PORTI + j] = fork()) {
			case -1:
				break;
			case 0:
    			execve(NAVE, argss, NULL);
				break;
			default:
				break;
		}
	}

	//start timer process
	strcpy(argst[0], TIMER);
	sprintf(argst[1], "%d", 30);
	sprintf(argst[2], "%d", parameters.SO_NAVI);
	sprintf(argst[3], "%d", parameters.SO_PORTI);
	argst[4] = NULL;

	switch(kid_pids[parameters.SO_PORTI + parameters.SO_NAVI] = fork()) {
		case -1:
			break;
		case 0:
    		execve(TIMER, argst, NULL);
			break;
		default:
			break;
	}

	sops.sem_op = 1;
	//semop(sem_id, &sops, SIGUSR1);

	char idin[10];
	char posx_str[20];
	char posy_str[20];
	char merce[20];
	char string_out[100];
	int idfind;
	char x[20];
	char y[20];

	//handle messages
	while(1) {
		msgrcv(master_msgq, &message, (sizeof(long) + sizeof(char) * 100), 1, 0);
		strcpy(idin, strtok(message.mesg_text, ":"));
		strcpy(posx_str, strtok(NULL, ":"));
		strcpy(posy_str, strtok(NULL, ":"));
		strcpy(merce, strtok(NULL, ":"));
		printf("MASTER PARSED ID: %s, POSX: %s, POSY: %s, MERCE: %s\n", idin, posx_str, posy_str, merce);
		kill(kid_pids[atoi(idin) + parameters.SO_PORTI], SIGUSR1);
		idfind = getRequesting(posx_str, posy_str, ports_positions, ports_shm_ptr_req, atoi(merce), parameters.SO_PORTI);

		message.mesg_type = 1;
		if(idfind < 0) {
			strcpy(message.mesg_text, "terminate");
			msgsnd(msgqueue_nave[atoi(idin)], &message, (sizeof(long) + sizeof(char) * 100), 0);
		}
		sprintf(x, "%f", ports_positions[idfind].x);
		sprintf(y, "%f", ports_positions[idfind].y);
		sprintf(msgq_id_str, "%d", msgqueue_porto[idfind]);
		strcpy(message.mesg_text, msgq_id_str);
		strcat(message.mesg_text, ":");
		strcat(message.mesg_text, x);
		strcat(message.mesg_text, ":");
		strcat(message.mesg_text, y);
		msgsnd(msgqueue_nave[atoi(idin)], &message, (sizeof(long) + sizeof(char) * 100), 0);
	}

	while((child_pid = wait(&status)) != -1) {
		dprintf(2, "Pid=%d. Sender (PID=%d) terminated with status 0x%04X\n", getpid(), child_pid, status);
	}

	//close messagequeues
	for(int i = 0; i < parameters.SO_PORTI; i++) {
		msgctl(msgqueue_porto[i], IPC_RMID, NULL);
	}
	for(int i = 0; i < parameters.SO_NAVI; i++) {
		msgctl(msgqueue_nave[i], IPC_RMID, NULL);
	}

	msgctl(master_msgq, IPC_RMID, NULL);

	//close semaphore and shared memories
	semctl(sem_id, 0, IPC_RMID);
	shmctl(ports_shm_id_aval, IPC_RMID, NULL);

	exit(0);
}

//gets the closed port that has a request for the specified merce; if the merce is not specified or no port has a request for it returns a random port
int getRequesting(char *posx_s, char *posy_s, struct position * portpositions, struct merce ** portsrequests, int merce, int nporti) {
	struct position currpos;
	struct position minpos;
	minpos.x = 1000000;
	minpos.y = 1000000;
	sscanf(posx_s, "%lf", &currpos.x);
	sscanf(posy_s, "%lf", &currpos.y);
	int imin = 0;
	for(int i = 0; i < nporti; i++) {
		//printf("CHECKING PORT %d:\n", i);
		for(int j = 0; j < 50 && portsrequests[i][j].type > 0; j++) {
		//printf("REQUEST %d: TYPE: %d QTY: %d\n", j, portsrequests[i][j].type, portsrequests[i][j].qty);
			if(portsrequests[i][j].type == merce && portsrequests[i][j].qty > 0) {
				if(sqrt(pow((portpositions[i].x - currpos.x),2) + pow((portpositions[i].y - currpos.y),2)) < sqrt(pow((minpos.x - currpos.x),2) + pow((minpos.y - currpos.y),2))) {
					imin = i;
					minpos.x = portpositions[i].x;
					minpos.y = portpositions[i].y;
				}
			}
		}
	}
	if(merce == 0) {
		//printf("NO MERCE SPECIFIED, RETURNING RANDOM PORT\n");
		return rand() % nporti;
	}
	if(imin >= 0) {
		//printf("FOUND CLOSEST PORT %d REQUESTING MERCE %d\n", imin, merce);
		return imin;
	}
	//printf("NO REQUESTS OF MERCE %d, RETURNING RANDOM PORT\n", merce);
	return rand() % nporti;
}

//load parameters from an input file; parameters must be separated by comma
int read_parameters_from_file(FILE *inputfile, struct parameters * parameters) {
	char string[256];
	char data[10];
	fgets(&string, 256, inputfile);

	strcpy(data, strtok(string, ","));
	parameters->SO_NAVI = atoi(data);
	if(parameters->SO_NAVI < 1) {
		printf("SO_NAVI must be >= 1\n");
		return 0;
	}

	strcpy(data, strtok(NULL, ","));
	parameters->SO_PORTI = atoi(data);
	if(parameters->SO_PORTI < 4) {
		printf("SO_PORTI must be >= 4\n");
		return 0;
	}

	strcpy(data, strtok(NULL, ","));
	parameters->SO_MERCI = atoi(data);

	strcpy(data, strtok(NULL, ","));
	parameters->SO_SIZE = atoi(data);

	strcpy(data, strtok(NULL, ","));
	parameters->SO_MIN_VITA = atoi(data);

	strcpy(data, strtok(NULL, ","));
	parameters->SO_MAX_VITA = atoi(data);

	strcpy(data, strtok(NULL, ","));
	parameters->SO_LATO = atoi(data);

	strcpy(data, strtok(NULL, ","));
	parameters->SO_SPEED = atoi(data);

	strcpy(data, strtok(NULL, ","));
	parameters->SO_CAPACITY = atoi(data);

	strcpy(data, strtok(NULL, ","));
	parameters->SO_BANCHINE = atoi(data);

	strcpy(data, strtok(NULL, ","));
	parameters->SO_FILL = atoi(data);

	strcpy(data, strtok(NULL, ","));
	parameters->SO_LOADSPEED = atoi(data);

	strcpy(data, strtok(NULL, ","));
	parameters->SO_DAYS = atoi(data);

	strcpy(data, strtok(NULL, ","));
	parameters->SO_STORM_DURATION = atoi(data);

	strcpy(data, strtok(NULL, ","));
	parameters->SO_SWELL_DURATION = atoi(data);

	strcpy(data , strtok(NULL, ","));
	parameters->SO_MAELSTORM = atoi(data);

	return 1;
}