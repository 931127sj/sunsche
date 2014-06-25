#include "init.h"

int global_tick = 0;
int time_quantum = 3;
pcb *proc[10];

int *msgq_id;

QUEUE* srunq;
QUEUE* swaitq;

int main(int argc, char* argv[]){
	int pid;

    struct sigaction old_sighandler, new_sighandler;
    struct itimerval itimer, old_timer;

    int ret;
	struct msgq_data msg;

    int i, r1, r2;

    srunq  = createQueue();
    swaitq = createQueue();

    for(i = 0; i < 10; i++){
        if((pid = fork()) > 0){
            // parent
            printf("parent (%d)\n", getpid());

            srand(time(NULL));
            do{
                r1 = rand() % (10 + i);
            }while(r1 == 0);
            do{
                r2 = rand() % (10 + i);
            }while(r2 == 0);

            // create child proc
            proc[i] = (pcb*)malloc(sizeof(pcb));
            memset(proc[i], 0, sizeof(pcb));
            proc[i]->pid = pid;
            proc[i]->state = READY;
            proc[i]->cpu_burst = r1;
            proc[i]->io_burst = r2;
 
            printf("child[%d] (%d) init cpu <%d>, init io <%d>\n", i, pid, proc[i]->cpu_burst, proc[i]->io_burst);

			enqueue(srunq, proc[i]);

			// ret = msgsnd(2611, &msg, sizeof(msg.io_time), 0);
            // if(ret < 0) printf("msgsnd error! (%d)\n", errno);
        }else if(pid < 0){
            // fork() fail
            printf("fork failed \n");
            return 0;
        }else if(pid == 0){
            // child
            do_child();
            // not reach here
            return 0;
        }
    } // end for
	printf("====srunq====\n");
    printQueue(srunq);

	// register timer signal
    memset(&new_sighandler, 0, sizeof(new_sighandler));
    new_sighandler.sa_handler = &time_tick;
    sigaction(SIGALRM, &new_sighandler, &old_sighandler);

    // setitimer
    itimer.it_interval.tv_sec = 1;
    itimer.it_interval.tv_usec = 0;
    itimer.it_value.tv_sec = 1;
    itimer.it_value.tv_usec = 0;
    global_tick = 0;
    setitimer(ITIMER_REAL, &itimer, &old_timer);

    msgq_id = (int*)malloc(sizeof(int));
    ret = msgget(2611, IPC_CREAT | 0644);
    if(ret == -1){
        printf("msgq creation error ! (p) ");
        printf("errno: %d\n", errno);

        *msgq_id = -1;
    }else{
        *msgq_id = ret;
        printf("msgq (key:0x%x, id:0x%x) created.\n", 2611, ret);
    }

	while(1){
        if(msgq_id > 0){
        ret = msgrcv(*msgq_id, &msg, sizeof(msg), 0, 0);
           if(ret > 0){
                int io_pid;
                io_pid = msg.pid;
                for(i = 0; i < 10; i++){
                    // change state
                    if(proc[i]->pid == io_pid){
                        proc[i]->state = WAIT;
                        proc[i]->io_burst = msg.io_time;
                        time_quantum = 0;
                        printf("now (%d) proc (%d) request io for (%d) ticks,and sleep \n", global_tick, proc[i]->pid, proc[i]->io_burst);
                    }
                    break;
                }
				// remove proc[i] from runqueue
				to_aq_from_bq(swaitq, srunq, proc[i]->pid);
            }else if(ret == 0){
                printf("not recieve message!\n");
            }else{
                printf("msgrcv error! (%d)\n", errno);
            }
        }
    }

	return;	
}

void time_tick(int signo){
    int run_pid = schedule();
	node *now_node;
	node *temp;
	pcb *nproc;

    global_tick++;
    printf("global_tick %d\n", global_tick);

    if(global_tick > 60){
        int i = 0;
        for(i = 0; i < 10; i++)
            kill(proc[i]->pid, SIGKILL);

        kill(getpid(), SIGKILL);
        return;
    }

	time_quantum--;
	decreaseIO(swaitq);
	now_node = find_proc(srunq, run_pid);
	nproc = now_node->proc;

	if((time_quantum == 3) && (nproc->cpu_burst == 0)){
		temp = dequeue(srunq);
		enqueue(srunq, temp->proc);

		nproc = temp->next->proc;
	}

	nproc->cpu_burst--;

	if((time_quantum > 0) && (nproc->cpu_burst == 0)){
		temp = dequeue(srunq);
		enqueue(swaitq, temp->proc);
		time_quantum = 3;
	} 

	if(time_quantum == 0){
		temp = dequeue(srunq);
		enqueue(srunq, temp->proc);
	}

    printf("now run proc id is (%d) remaining cpu <%d>\n", run_pid, nproc->cpu_burst);
    // printf("now real proc id is (%d)\n", getpid());

    /* notify run_pid process of having next time quantum */
    kill (run_pid, SIGALRM);
}

int schedule(void){
	// find proc from runq, according to the rr policy
	node* temp;
	temp = srunq->front;
	if(time_quantum == 0) time_quantum = 3;

    return temp->proc->pid;
}

void do_child(void){
	int ret, i;
    struct sigaction old_sighandler, new_sighandler;

    // register timer signal
    memset (&new_sighandler, 0, sizeof (new_sighandler));
    new_sighandler.sa_handler = &child_process;
    sigaction(SIGALRM, &new_sighandler, &old_sighandler);

    // open msgq
    msgq_id = (int *)malloc(sizeof(int));
    *msgq_id = -1;
    while (1) {
    }

}

void child_process(int signo){
	node *now_node;
	pcb  *now_proc;

	now_node = find_proc(srunq, getpid());
	now_proc = now_node->proc;

	now_proc->cpu_burst--;

    printf("child (%d) remaining_cpu_burst: %d \n", getpid(), now_proc->cpu_burst);
    if (time_quantum < 0) {
       do_io();

       // reset cpu_burst
	   if(now_proc->io_burst == 0) to_aq_from_bq(srunq, swaitq, getpid());

       // remaining_cpu_burst = 1;
   }

	return;
}

void do_io(void){
	struct msgq_data msg;
	int ret;
	key_t key;

	if (*msgq_id == -1) {
		ret = msgget(MY_MSGQ_KEY, IPC_CREAT | 0644);
		if (ret == -1) {
			printf("msgq creation error !  (c) ");
			printf("errno: %d\n", errno);
		}
		*msgq_id = ret;
	}

	msg.mtype = 0;
	msg.pid = getpid();
	msg.io_time = 1;
	msgsnd(*msgq_id, &msg, sizeof(msg), 0);
}

QUEUE* createQueue(void){
	QUEUE* queue;

	queue = (QUEUE *)malloc(sizeof(QUEUE));
    if (queue)
    {
        queue->front = NULL;
        queue->rear = NULL;
        queue->count = 0;
    }
    return queue;
}

void enqueue(QUEUE* queue, pcb* proc_id){
	node *newPtr;
 
    if (!(newPtr = (node*)malloc(sizeof(node)))) return;

    newPtr->proc = proc_id;
    newPtr->next = NULL;
	newPtr->prev = queue->rear;

    if (queue->count == 0) queue->front = newPtr;
    else queue->rear->next = newPtr;

    (queue->count)++;
    queue->rear = newPtr;
    return;
}

node* dequeue(QUEUE* queue){
	node *temp;

    if (!queue->count) return 0;

    temp = queue->front;
    if (queue->count == 1) queue->rear = queue->front = NULL;
    else queue->front = queue->front->next;

    (queue->count)--;
    return temp;
}

void printQueue(QUEUE* queue){
	node *temp;
    int i = 1;

    for(temp = queue->front; temp != NULL; temp = temp->next){
        printf("(%2d) pid : %d\n", i, temp->proc->pid);
        i++;
    }
    printf("=============\n");

    return;
}

void decreaseIO(QUEUE* queue){
	node *temp;

	for(temp = queue->front; temp != NULL; temp = temp->next)
		temp->proc->io_burst--;

	return;
}

void to_aq_from_bq(QUEUE* a, QUEUE* b, int pid){
	node* temp;

	if(pid == -1){
		temp = dequeue(b);
		enqueue(a, temp->proc);
	}else{
		temp = find_proc(b, pid);
		temp->prev->next = temp->next;
		enqueue(a, temp->proc);
	}

	return;
}

node* find_proc(QUEUE* queue, int pid){
	node *temp;

	for(temp = queue->front; temp != NULL; temp = temp->next){
		if(temp->proc->pid == pid) return temp;
	}

	return NULL;
}
