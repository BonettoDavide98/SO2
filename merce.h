#include <sys/time.h>

struct merce {
    int type;
    int qty;
    struct timeval spoildate;
};

struct position {
    double x;
    double y;
};
	
struct mesg_buffer {
   	long mesg_type;
   	char mesg_text[100];
};

struct parameters {
    int SO_NAVI;
    int SO_PORTI;
    int SO_MERCI;
    int SO_SIZE;
    int SO_MIN_VITA;
    int SO_MAX_VITA;
    int SO_LATO;
    int SO_SPEED;
    int SO_CAPACITY;
    int SO_BANCHINE;
    int SO_FILL;
    int SO_LOADSPEED;
    int SO_DAYS;
    int SO_STORM_DURATION;
    int SO_SWELL_DURATION;
    int SO_MAELSTORM;
};

struct report {
    int seawithcargo;
    int seawithoutcargo;
    int docked;
    struct portstatus * ports;
};

struct portstatus {
    int mercepresent;
    int mercesent;
    int mercereceived;
    int banksocc;
    int bankstot;
};