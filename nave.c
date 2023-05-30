#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include <signal.h>
#include "merce.h"

int stormduration;
long stormtosleep = -1;
int end = 0;
int currentplace = 0;	//sea = 0 port = 1
int hascargo = 0;	//no = 0 yes = 1

int master_msgq;
int shipid;
int day = 0;
struct merce * cargo;
int * spoiled;
int num_merci;

void stormhandler();
void reporthandler();
void endreporthandler();
void sleepForStorm();

int main (int argc, char * argv[]) {
	master_msgq =atoi(argv[6]);
	shipid = atoi(argv[2]);
	struct mesg_buffer message;
	struct position pos;
	double speed = atoi(argv[5]);
	char posx_str[20];
	strcpy(posx_str, argv[3]);
	char posy_str[20];
	strcpy(posy_str, argv[4]);
	sscanf(argv[3], "%lf", &pos.x);
	sscanf(argv[4], "%lf", &pos.y);
	num_merci = atoi(argv[9]);
	int max_slots = num_merci * 5;
	struct sembuf sops;
	int master_sem_id = atoi(argv[10]);
	spoiled = malloc((1 + num_merci) * sizeof(int));

	stormduration = atoi(argv[8]);
	char string_out[100];

	char msgq_id_porto[20];
	char shm_id_porto_req[20];
	int *shm_ptr_porto_req;
	char shm_id_porto_aval[20];
	struct merce *shm_ptr_porto_aval;
	char destx[20];
	char desty[20];
	struct position dest;
	struct timespec tv1, tv2;
	long traveltime;
	char text[20];
	int fill;
	int loadtime;
	int tonstomove = 0;
	long sleeptime;

	cargo = malloc(max_slots * sizeof(struct merce));
	int cargocapacity = atoi(argv[7]);
	int cargocapacity_free = cargocapacity;

	//initialize cargo
	for(int c = 0; c < max_slots; c++) {
		cargo[c].type = 0;
		cargo[c].qty = 0;
	}

	//initialize spoiled
	for(int i = 0; i < num_merci + 1; i++) {
		spoiled[i] = 0;
	}

	int randomportflag = 0;
	message.mesg_type = 1;

	signal(SIGUSR1, stormhandler);
	signal(SIGUSR2, reporthandler);
	signal(SIGINT, endreporthandler);

	//wait for semaphore
	sops.sem_num = 0;
	sops.sem_flg = 0;
	sops.sem_op = -1;
	semop(master_sem_id, &sops, 1);

	//ship loop, will last until interrupted by an external process
	while(1) {
		//ask master the closest port that asks for my largest merce
		removeSpoiled(cargo);
		strcpy(message.mesg_text, argv[2]);
		strcat(message.mesg_text, ":");
		sprintf(posx_str, "%f", pos.x);
		strcat(message.mesg_text, posx_str);
		strcat(message.mesg_text, ":");
		sprintf(posy_str, "%f", pos.y);
		strcat(message.mesg_text, posy_str);
		strcat(message.mesg_text, ":");
		if(randomportflag == 0) {
			sprintf(text, "%d", getLargestCargo(cargo, max_slots));
			strcat(message.mesg_text, text);
		} else {
			randomportflag = 0;
			strcat(message.mesg_text, "0");
		}
		//printf("SHIP %s ASKING MASTER %s\n", argv[2], message.mesg_text);
		msgsnd(master_msgq, &message, (sizeof(long) + sizeof(char) * 100), 0);

		//wait for master answer
		while(msgrcv(atoi(argv[1]), &message, (sizeof(long) + sizeof(char) * 100), 1, 0) == -1) {
			//loop until message is received
		}
		//printf("SHIP %s RECEIVED : %s\n", argv[2], message.mesg_text);

		//parse answer and go to specified location
		strcpy(msgq_id_porto, strtok(message.mesg_text, ":"));
		strcpy(destx, strtok(NULL, ":"));
		strcpy(desty, strtok(NULL, ":"));
		sscanf(destx, "%lf", &dest.x);
		sscanf(desty, "%lf", &dest.y);
		//calculate travel time
		traveltime = (long) ((sqrt(pow((dest.x - pos.x),2) + pow((dest.y - pos.y),2)) / speed * 1000000000));
		tv1.tv_nsec = traveltime % 1000000000;
		tv1.tv_sec = (int) ((traveltime - tv1.tv_nsec) / 1000000000);
		//printf("SHIP %s SETTING COURSE TO %s %s, ETA: %d,%ld DAYS\n", argv[2], destx, desty, tv1.tv_sec, tv1.tv_nsec);
		//travel
		while(nanosleep(&tv1, &tv2) == -1) {
			sleepForStorm();
			tv1 = tv2;
		}
		pos.x = dest.x;
		pos.y = dest.y;
		strcpy(posx_str, destx);
		strcpy(posy_str, desty);
		sleepForStorm();
		//printf("SHIP %s ARRIVED AT PORT IN %f %f, SENDING DOCKING REQUEST ...\n", argv[2], pos.x, pos.y);
		
		//send dock request to port
		strcpy(message.mesg_text, "dockrq");
		strcat(message.mesg_text, ":");
		strcat(message.mesg_text, argv[1]);
		//printf("MESSAGE FROM SHIP : %s\n", message.mesg_text);
		msgsnd(atoi(msgq_id_porto), &message, (sizeof(long) + sizeof(char) * 100), 0);

		//wait for port answer
		while(msgrcv(atoi(argv[1]), &message, (sizeof(long) + sizeof(char) * 100), 1, 0) == -1) {
			//loop until message is received
		}
		strcpy(text, strtok(message.mesg_text, ":"));
		strcpy(shm_id_porto_req, strtok(NULL, ":"));
		strcpy(shm_id_porto_aval, strtok(NULL, ":"));
		loadtime = atoi(strtok(NULL, ":"));

		//decide what to do based on port answer
		if(strcmp(text, "accept") == 0) {
			//if port accepted the request, start loading and unloading cargo
			currentplace = 1;
			removeSpoiled(cargo);
			if((int *) (shm_ptr_porto_req = (int *) shmat(atoi(shm_id_porto_req), NULL, 0)) == -1) {
				printf("*** shmat error nave req ***\n");
				exit(1);
			}
			if((struct merce *) (shm_ptr_porto_aval = (struct merce *) shmat(atoi(shm_id_porto_aval), NULL, 0)) == -1) {
				printf("*** shmat error nave aval ***\n");
				exit(1);
			}

			tonstomove = unloadCargo(cargo, shm_ptr_porto_req, max_slots, num_merci);

			cargocapacity_free = cargocapacity;
			for(int i = 0; i < max_slots; i++) {
				if(cargo[i].type == 0) {
					i = max_slots;
				} else if(cargo[i].type > 0 && cargo[i].qty > 0) {
					cargocapacity_free = cargocapacity_free - cargo[i].qty;
				}
			}

			int splitton = cargocapacity_free / num_merci;
			int flag = 1;

			while(flag) {
				flag = 0;
				for(int i = 0; i < shm_ptr_porto_req[0] && cargocapacity_free > 0 && shm_ptr_porto_aval[i].type != 0; i++) {
					if(shm_ptr_porto_aval[i].type > 0 && shm_ptr_porto_aval[i].qty > 0) {
						if(splitton > cargocapacity_free) {
							splitton = cargocapacity_free;
						}

						if(shm_ptr_porto_aval[i].qty > splitton) {
							tonstomove += loadCargo2(cargo, shm_ptr_porto_aval[i].type, splitton, shm_ptr_porto_aval[i].spoildate, max_slots);
							shm_ptr_porto_aval[i].qty -= splitton;
							cargocapacity_free -= splitton;
							printf("SENT %d TONS OF MERCE %d\n", splitton, shm_ptr_porto_aval[i].type);
							shm_ptr_porto_req[shm_ptr_porto_aval[i].type + (num_merci * 2)] += splitton;
							flag = 1;
						} else {
							tonstomove += loadCargo(cargo, shm_ptr_porto_aval[i], max_slots);
							cargocapacity_free -= shm_ptr_porto_aval[i].qty;
							printf("SENT %d TONS OF MERCE %d\n", shm_ptr_porto_aval[i].qty, shm_ptr_porto_aval[i].type);
							shm_ptr_porto_req[shm_ptr_porto_aval[i].type + (num_merci * 2)] += shm_ptr_porto_aval[i].qty;
							shm_ptr_porto_aval[i].type = -1;
							shm_ptr_porto_aval[i].qty = -1;
							flag = 1;
						}
					}
				}
				if(cargocapacity_free <= 0) {
					flag = 0;
				}
			}

			//sleep for loadtime * tonstomove
			if(tonstomove > 0) {
				sleeptime = (long) (tonstomove / loadtime * 1000000000);
				tv1.tv_nsec = sleeptime % 1000000000;
				tv1.tv_sec = (int) ((sleeptime - tv1.tv_nsec) / 1000000000);
				printf("SHIP %s LOADING/OFFLOADING %d TONS, ETA: %d.%ld DAYS\n", argv[2], tonstomove, tv1.tv_sec, tv1.tv_nsec);
				skipStorm(tv1.tv_sec, tv1.tv_nsec);
				while(nanosleep(&tv1, &tv2) == -1) {
					skipStorm(tv2.tv_sec, tv2.tv_nsec);
					tv1 = tv2;
				}
			}

			strcpy(message.mesg_text, "dockfree");
			strcat(message.mesg_text, ":");
			strcat(message.mesg_text, argv[1]);
			msgsnd(atoi(msgq_id_porto), &message, (sizeof(long) + sizeof(char) * 100), 0);
			//aspetto risposta da porto prima ti ripartire
			while(msgrcv(atoi(argv[1]), &message, (sizeof(long) + sizeof(char) * 100), 1, 0) == -1) {
				//loop until message is received
			}
			currentplace = 0;
			//printf("RIPARTITA\n");

			hascargo = 0;
			printf("SHIP %s CARGO: |", argv[2]);
			for(int i = 0; i < max_slots; i++) {
				if(cargo[i].type == 0) {
					i = max_slots;
				} else if(cargo[i].qty > 0 && cargo[i].type > 0) {
					//i = max_slots;
					hascargo = 1;
					printf(" %d TONS OF %d |", cargo[i].qty, cargo[i].type);
				}
			}
			printf("\n");
		} else {
			//if port declined access, ask master for a different port
			//printf("SHIP %s HAS BEEN DENIED DOCKING BECAUSE THE QUEUE WAS TOO LONG");
			randomportflag = 1;
		}
	}

	exit(0);
}

//returns largest type of merce loaded in cargo
int getLargestCargo(struct merce * cargo, int max_slots) {
	int max = 0;
	int imax = 0;

	for(int i = 0; i < max_slots; i++) {
		if(cargo[i].type == 0) {
			return imax;
		} else if(cargo[i].type > 0 && cargo[i].qty > max) {
			max = cargo[i].qty;
			imax = cargo[i].type;
		}
	}

	return imax;
}

//remove spoiled merci
void removeSpoiled(struct merce *available) {
	struct timeval currenttime;
	gettimeofday(&currenttime, NULL);
	for(int i = 0; i < num_merci * 5; i++) {
		if(available[i].type > 0 && available[i].qty > 0) {
			if(available[i].spoildate.tv_sec < currenttime.tv_sec) {
				printf("REMOVED %d TONS OF %d FROM SHIP DUE TO SPOILAGE\n", available[i].qty, available[i].type);
				spoiled[available[i].type] += available[i].qty;
				available[i].type = -1;
				available[i].qty = -1;
			} else if(available[i].spoildate.tv_sec == currenttime.tv_sec) {
				if(available[i].spoildate.tv_usec <= currenttime.tv_usec) {
					printf("REMOVED %d TONS OF %d FROM SHIP DUE TO SPOILAGE\n", available[i].qty, available[i].type);
					spoiled[available[i].type] += available[i].qty;
					available[i].type = -1;
					available[i].qty = -1;
				}
			}
		}
	}
}

int loadCargo(struct merce * cargo, struct merce mercetoload, int max_slots) {
	for(int i = 0; i < max_slots; i++) {
		if(cargo[i].type == mercetoload.type && cargo[i].spoildate.tv_sec == mercetoload.spoildate.tv_sec && cargo[i].spoildate.tv_usec == mercetoload.spoildate.tv_usec) {
			cargo[i].qty += mercetoload.qty;
			return mercetoload.qty;
		}
		if(cargo[i].type <= 0) {
			cargo[i] = mercetoload;
			return mercetoload.qty;
		}
	}
	return 0;
}

int loadCargo2(struct merce * cargo, int type, int qty, struct timeval spoildate, int max_slots) {
	for(int i = 0; i < max_slots; i++) {
		if(cargo[i].type == type && cargo[i].spoildate.tv_sec == spoildate.tv_sec && cargo[i].spoildate.tv_usec == spoildate.tv_usec) {
			cargo[i].qty += qty;
			return qty;
		}
		if(cargo[i].type <= 0) {
			cargo[i].type = type;
			cargo[i].qty = qty;
			cargo[i].spoildate = spoildate;
			return qty;
		}
	}
	return 0;
}

int unloadCargo(struct merce * cargo, int * requests, int max_slots, int num_merci) {
	int tonstomove = 0;
	for(int i = 0; i < max_slots; i++) {
		if(cargo[i].type == 0) {
			return 0;
		} else {
			if(cargo[i].type > 0 && cargo[i].qty > 0) {
				if(cargo[i].qty >= requests[cargo[i].type]) {
					cargo[i].qty -= requests[cargo[i].type];
					if(cargo[i].qty == 0) {
						cargo[i].type = -1;
					}
					requests[cargo[i].type + num_merci] += requests[cargo[i].type];
					tonstomove += requests[cargo[i].type];
					requests[cargo[i].type] = -1;
				} else {
					requests[cargo[i].type] -= cargo[i].qty;
					requests[cargo[i].type + num_merci] += cargo[i].qty;
					tonstomove += cargo[i].qty;
					cargo[i].type = -1;
					cargo[i].qty = -1;
				}
			}
		}
	}
	return tonstomove;
}

void sleepForStorm() {
	if(stormtosleep > 0) {
		printf("STORM! SLEEPING FOR %d HOURS\n", stormduration);
		struct timespec sleep, sleepremaining;
		sleep.tv_nsec = stormtosleep % 1000000000;
		sleep.tv_sec = (int) ((stormtosleep - sleep.tv_nsec) / 1000000000);
		stormtosleep = 0;
		while(nanosleep(&sleep, &sleepremaining) == -1) {
			if(stormtosleep > 0) {
				sleepremaining.tv_nsec += stormtosleep % 1000000000;
				sleepremaining.tv_sec += (int) ((stormtosleep - sleep.tv_nsec) / 1000000000);
			}
			sleep = sleepremaining;
		}
	}
}

void skipStorm(int secs, long nanosecs) {
	if(stormtosleep > 0) {
		stormtosleep -= (secs * 1000000000) + nanosecs;
	}
}

void stormhandler() {
	if(stormtosleep <= 0) {
		stormtosleep = stormduration;
	} else {
		stormtosleep += stormduration;
	}
}

void reporthandler() {
	struct mesg_buffer message;
	message.mesg_type = 1;
	char temp[20];

	strcpy(message.mesg_text, "s");
	strcat(message.mesg_text, ":");
	sprintf(temp, "%d", day);
	strcat(message.mesg_text, temp);
	strcat(message.mesg_text, ":");
	if(currentplace == 0) {
		if(hascargo == 1) {
			strcat(message.mesg_text, "0");		//s:day:0	in sea with cargo
		} else {
			strcat(message.mesg_text, "1");		//s:day:1	in sea without cargo
		}
	} else {
		strcat(message.mesg_text, "2");			//s:day:2	in port
	}

	msgsnd(master_msgq, &message, (sizeof(long) + sizeof(char) * 100), 0);

	day++;
}

void endreporthandler() {
	//printf("TERMINATING NAVE...\n");
	removeSpoiled(cargo);
	struct mesg_buffer message;
	message.mesg_type = 1;
	char temp[20];

	strcpy(message.mesg_text, "S");
	for(int i = 1; i < num_merci + 1; i++) {
		strcat(message.mesg_text, ":");
		sprintf(temp, "%d", spoiled[i]);
		strcat(message.mesg_text, temp);
	}

	msgsnd(master_msgq, &message, (sizeof(long) + sizeof(char) * 100), 0);

	exit(0);
}