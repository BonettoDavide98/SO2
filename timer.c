#include <stdio.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include "merce.h"

int main (int argc, char * argv[]) {
    int days = atoi(argv[1]);
    int n_navi = atoi(argv[2]);
    int n_porti = atoi(argv[3]);
    int master_msgq = atoi(argv[4]);
    int master_sem_id = atoi(argv[5]);
    struct mesg_buffer message;
	struct sembuf sops;

	sops.sem_num = 0;
	sops.sem_flg = 0;
	sops.sem_op = -1;
	semop(master_sem_id, &sops, 1);

    message.mesg_type = 1;
    for(int i = 0; i < days; i++) {
        sleep(1);
        strcpy(message.mesg_text, "d");
        msgsnd(master_msgq, &message, (sizeof(long) + sizeof(char) * 100), 0);
    }
    strcpy(message.mesg_text, "t");
    msgsnd(master_msgq, &message, (sizeof(long) + sizeof(char) * 100), 0);
}