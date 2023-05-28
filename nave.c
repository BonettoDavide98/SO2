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
#include "merce.h"

int storm = 0;
int stormduration;
int end = 0;
int currentplace = 0;	//sea = 0 port = 1
int hascargo = 0;	//no = 0 yes = 1

void sighandler() {
	storm = 1;
}

void sleepForStorm();

int main (int argc, char * argv[]) {
	struct mesg_buffer {
    	long mesg_type;
    	char mesg_text[100];
	};
	struct mesg_buffer message;
	struct position pos;
	double speed = atoi(argv[5]);
	char posx_str[20];
	strcpy(posx_str, argv[3]);
	char posy_str[20];
	strcpy(posy_str, argv[4]);
	sscanf(argv[3], "%lf", &pos.x);
	sscanf(argv[4], "%lf", &pos.y);
	int num_merci = atoi(argv[9]);
	int max_slots = num_merci * 5;

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

	struct merce *cargo = malloc(max_slots * sizeof(struct merce));
	int cargocapacity = atoi(argv[7]);
	int cargocapacity_free = cargocapacity;

	//initialize cargo
	for(int c = 0; c < max_slots; c++) {
		cargo[c].type = 0;
		cargo[c].qty = 0;
	}

	int randomportflag = 0;
	message.mesg_type = 1;

	signal(SIGUSR1, sighandler);

	//ship loop, will last until interrupted by an external process
	while(1) {
		//ask master the closest port that asks for my largest merce
		sleep(1);
		removeSpoiled(cargo, atoi(argv[2]));
		strcpy(message.mesg_text, argv[2]);
		strcat(message.mesg_text, ":");
		strcat(message.mesg_text, posx_str);
		strcat(message.mesg_text, ":");
		strcat(message.mesg_text, posy_str);
		strcat(message.mesg_text, ":");
		if(randomportflag == 0) {
			sprintf(text, "%d", getLargestCargo(cargo));
			strcat(message.mesg_text, text);
		} else {
			randomportflag = 0;
			strcat(message.mesg_text, "0");
		}
		msgsnd(atoi(argv[6]), &message, (sizeof(long) + sizeof(char) * 100), 0);

		//wait for master answer
		msgrcv(atoi(argv[1]), &message, (sizeof(long) + sizeof(char) * 100), 1, 0);
		printf("SHIP %s RECEIVED : %s\n", argv[2], message.mesg_text);

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
		printf("SHIP %s SETTING COURSE TO %s %s, ETA: %d,%ld DAYS\n", argv[2], destx, desty, tv1.tv_sec, tv1.tv_nsec);
		//travel
		nanosleep(&tv1, &tv2);
		pos.x = dest.x;
		pos.y = dest.y;
		strcpy(posx_str, destx);
		strcpy(posy_str, desty);
		sleepForStorm();
		printf("SHIP %s ARRIVED AT PORT IN %f %f, SENDING DOCKING REQUEST ...\n", argv[2], pos.x, pos.y);
		
		//send dock request to port
		strcpy(message.mesg_text, "dockrq");
		strcat(message.mesg_text, ":");
		strcat(message.mesg_text, argv[1]);
		printf("MESSAGE FROM SHIP : %s\n", message.mesg_text);
		msgsnd(atoi(msgq_id_porto), &message, (sizeof(long) + sizeof(char) * 100), 0);

		//wait for port answer
		msgrcv(atoi(argv[1]), &message, (sizeof(long) + sizeof(char) * 100), 1, 0);
		strcpy(text, strtok(message.mesg_text, ":"));
		strcpy(shm_id_porto_req, strtok(NULL, ":"));
		strcpy(shm_id_porto_aval, strtok(NULL, ":"));
		loadtime = atoi(strtok(NULL, ":"));

		//decide what to do based on port answer
		if(strcmp(text, "accept") == 0) {
			//if port accepted the request, start loading and unloading cargo
			removeSpoiled(cargo, atoi(argv[2]));
			if((int *) (shm_ptr_porto_req = (int *) shmat(atoi(shm_id_porto_req), NULL, 0)) == -1) {
				printf("*** shmat error nave req ***\n");
				exit(1);
			}
			if((struct merce *) (shm_ptr_porto_aval = (struct merce *) shmat(atoi(shm_id_porto_aval), NULL, 0)) == -1) {
				printf("*** shmat error nave aval ***\n");
				exit(1);
			}

			for(int k = 0; k < max_slots; k++) {
				if(cargo[k].type == 0) {
					k = max_slots;
				} else {
					if(cargo[k].type > 0 && cargo[k].qty > 0 && shm_ptr_porto_req[cargo[k].type] > 0) {
						if(cargo[k].qty >= shm_ptr_porto_req[cargo[k].type]) {
							shm_ptr_porto_req[cargo[k].type + num_merci - 1] += shm_ptr_porto_req[cargo[k].type];
							cargo[k].qty -= shm_ptr_porto_req[cargo[k].type];
							shm_ptr_porto_req[cargo[k].type] = 0;
							if(cargo[k].qty == 0) {
								cargo[k].type = -1;
							}
						} else {
							shm_ptr_porto_req[cargo[k].type + num_merci - 1] += cargo[k].qty;
							shm_ptr_porto_req[cargo[k].type] -= cargo[k].qty;
							cargo[k].qty = 0;
							cargo[k].type = -1;
						}
					}
				}
			}

			cargocapacity_free = cargocapacity;
			for(int i = 0; i < max_slots; i++) {
				if(cargo[i].type == 0) {
					i = max_slots;
				} else if(cargo[i].type > 0 && cargo[i].qty > 0) {
					cargocapacity_free = cargocapacity_free - cargo[i].qty;
				}
			}

			for(int i = 0; i < shm_ptr_porto_req[0] && cargocapacity_free > 0; i++) {
				if(shm_ptr_porto_aval[i].type != 0 && shm_ptr_porto_aval[i].qty > 0) {
					if(cargocapacity_free >= shm_ptr_porto_aval[i].qty) {
						for(int j = 0; j < max_slots; j++) {
							if(cargo[j].type == -1 || cargo[j].type == 0) {
								printf("SHIP %s LOADING %d TONS OF %d\n", argv[2], shm_ptr_porto_aval[i].qty, shm_ptr_porto_aval[i].type);
								cargo[j].type = shm_ptr_porto_aval[i].type;
								cargo[j].qty = shm_ptr_porto_aval[i].qty;
								cargo[j].spoildate.tv_sec = shm_ptr_porto_aval[i].spoildate.tv_sec;
								cargo[j].spoildate.tv_usec = shm_ptr_porto_aval[i].spoildate.tv_usec;
								cargocapacity_free -= cargo[j].qty;
								shm_ptr_porto_aval[i].type = -1;
								shm_ptr_porto_aval[i].qty = 0;
								j = max_slots;
							}
						}
					} else {
						for(int j = 0; j < max_slots; j++) {
							if(cargo[j].type == -1 || cargo[j].type == 0) {
								printf("SHIP %s LOADING %d TONS OF %d\n",  argv[2], cargocapacity_free, shm_ptr_porto_aval[i].type);
								cargo[j].type = shm_ptr_porto_aval[i].type;
								shm_ptr_porto_aval[i].qty -= cargocapacity_free;
								cargo[j].qty = cargocapacity_free;
								cargo[j].spoildate.tv_sec = shm_ptr_porto_aval[i].spoildate.tv_sec;
								cargo[j].spoildate.tv_usec = shm_ptr_porto_aval[i].spoildate.tv_usec;
								cargocapacity_free = 0;
								j = max_slots;
							}
						}
					}
				}
			}


			sleep(1);

			strcpy(message.mesg_text, "dockfree");
			strcat(message.mesg_text, ":");
			strcat(message.mesg_text, argv[1]);
			msgsnd(atoi(msgq_id_porto), &message, (sizeof(long) + sizeof(char) * 100), 0);
			//aspetto risposta da porto prima ti ripartire
			msgrcv(atoi(argv[1]), &message, (sizeof(long) + sizeof(char) * 100), 1, 0);
			printf("RIPARTITA\n");

			printf("SHIP %s CARGO: |", argv[2]);
			for(int i = 0; i < max_slots; i++) {
				if(cargo[i].type == 0) {
					i = max_slots;
				} else if(cargo[i].qty > 0 && cargo[i].type > 0) {
					printf(" %d TONS OF %d |", cargo[i].qty, cargo[i].type);
				}
			}
			printf("\n");
		} else {
			//if port declined access, ask master for a different port
			printf("SHIP %s HAS BEEN DENIED DOCKING BECAUSE THE QUEUE WAS TOO LONG");
			randomportflag = 1;
		}
	}

	exit(0);
}

//returns largest type of merce loaded in cargo
int getLargestCargo(struct merce * cargo) {
	int label = -1;
	int temp = 0;
	int maxlabel;
	int max = -1;

	for (int i = 0; i < 20; i++) {
		if(cargo[i].type != maxlabel && cargo[i].type > 0 && cargo[i].qty > 0) {
			label = cargo[i].type;
			for(int j = i; j < 20; j++) {
				if(cargo[j].type == label && cargo[j].qty > 0) {
					temp += cargo[i].qty;
				}
			}

			if(temp > max) {
				maxlabel = label;
				max = temp;
			}
		}
	}

	return maxlabel;
}

//remove spoiled merci
void removeSpoiled(struct merce *available, int naveid) {
	struct timeval currenttime;
	gettimeofday(&currenttime, NULL);
	for(int i = 0; i < 20; i++) {
		if(available[i].type > 0 && available[i].qty > 0) {
			if(available[i].spoildate.tv_sec < currenttime.tv_sec) {
				//printf("REMOVED %d TONS OF %d FROM SHIP %d DUE TO SPOILAGE\n", available[i].qty, available[i].type, naveid);
				available[i].type = 0;
				available[i].qty = 0;
			} else if(available[i].spoildate.tv_sec == currenttime.tv_sec) {
				if(available[i].spoildate.tv_usec <= currenttime.tv_usec) {
				//printf("REMOVED %d TONS OF %d FROM SHIP %d DUE TO SPOILAGE\n", available[i].qty, available[i].type, naveid);
					available[i].type = 0;
					available[i].qty = 0;
				}
			}
		}
	}
}

void sleepForStorm() {
	if(storm > 0) {
		storm = 0;

		printf("STORM! SLEEPING FOR %d HOURS\n", stormduration);
		long sleeplong;
		struct timespec sleep;
		sleeplong = (long) (stormduration * 41666666);
		sleep.tv_nsec = stormduration % 1000000000;
		sleep.tv_sec = (int) (sleeplong - sleep.tv_nsec) / 1000000000;
	}
}