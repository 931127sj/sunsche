#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <signal.h>
#include <errno.h>

#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/time.h>

#define MY_MSGQ_KEY 2611

enum proc_state{
    READY,
    WAIT,
};

typedef struct{
    int pid;
    enum proc_state state;
    int cpu_burst;
    int io_burst;
}pcb; 

typedef struct node_{
    struct node_ *prev;
    struct node_ *next;
    pcb *proc;
}node;

typedef struct {
    node *front;
    node *rear;
    int count;
}QUEUE;

struct msgq_data {
	int mtype;
	int pid;
	int io_time;
};

void time_tick(int);
int schedule(void);
void do_child(void);
void child_process(int signo);
void do_io(void);

QUEUE* createQueue();
void enqueue(QUEUE* queue, pcb* proc_id);
node* dequeue(QUEUE* queue);
void printQueue(QUEUE* queue);

void decreaseIO(QUEUE* queue);
void to_aq_from_bq(QUEUE* a, QUEUE* b, int pid);
node* find_proc(QUEUE* queue, int pid);
