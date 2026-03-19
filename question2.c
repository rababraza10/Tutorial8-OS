#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#define MEMORY 1024
#define MAX_NAME 256
#define MAX_LINE 512

typedef struct {
    char name[MAX_NAME];
    int priority;
    int pid;
    int address;
    int memory;
    int runtime;
    bool suspended;
} proc;

typedef struct node {
    proc data;
    struct node *next;
} node;

typedef struct {
    node *front;
    node *rear;
    int size;
} queue;


void init_queue(queue *q) {
    q->front = NULL;
    q->rear = NULL;
    q->size = 0;
}

bool is_empty(queue *q) {
    return q->front == NULL;
}

void push(queue *q, proc p) {
    node *new_node = (node *)malloc(sizeof(node));
    if (new_node == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }

    new_node->data = p;
    new_node->next = NULL;

    if (q->rear == NULL) {
        q->front = new_node;
        q->rear = new_node;
    } else {
        q->rear->next = new_node;
        q->rear = new_node;
    }

    q->size++;
}

proc pop(queue *q) {
    if (is_empty(q)) {
        fprintf(stderr, "Error: attempted to pop from empty queue.\n");
        exit(EXIT_FAILURE);
    }

    node *temp = q->front;
    proc p = temp->data;

    q->front = q->front->next;
    if (q->front == NULL) {
        q->rear = NULL;
    }

    free(temp);
    q->size--;
    return p;
}


int avail_mem[MEMORY];

void init_memory(void) {
    for (int i = 0; i < MEMORY; i++) {
        avail_mem[i] = 0;
    }
}

int allocate_memory(int size_needed) {
    int count = 0;
    int start = -1;

    for (int i = 0; i < MEMORY; i++) {
        if (avail_mem[i] == 0) {
            if (count == 0) {
                start = i;
            }
            count++;
            if (count == size_needed) {
                for (int j = start; j < start + size_needed; j++) {
                    avail_mem[j] = 1;
                }
                return start;
            }
        } else {
            count = 0;
            start = -1;
        }
    }

    return -1;
}

void free_memory_block(int start, int size) {
    if (start < 0) return;

    for (int i = start; i < start + size && i < MEMORY; i++) {
        avail_mem[i] = 0;
    }
}



void print_proc_info(const proc *p) {
    printf("Name: %s | Priority: %d | PID: %d | Address: %d | Memory: %d | Runtime: %d | Suspended: %s\n",
           p->name,
           p->priority,
           p->pid,
           p->address,
           p->memory,
           p->runtime,
           p->suspended ? "true" : "false");
}

void wait_for_stop(pid_t pid) {
    int status;
    if (waitpid(pid, &status, WUNTRACED) == -1) {
        perror("waitpid failed");
        exit(EXIT_FAILURE);
    }
}

void wait_for_termination(pid_t pid) {
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid failed");
        exit(EXIT_FAILURE);
    }
}

pid_t start_process(void) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        execl("./process", "process", "200", NULL);
        perror("exec failed");
        exit(EXIT_FAILURE);
    }

    return pid;
}


void load_processes(const char *filename, queue *priority_q, queue *secondary_q) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("Could not open processes file");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE];

    while (fgets(line, sizeof(line), fp) != NULL) {
        proc p;
        char name[MAX_NAME];

        if (sscanf(line, " %255[^,], %d, %d, %d", name, &p.priority, &p.memory, &p.runtime) == 4) {
            strncpy(p.name, name, MAX_NAME - 1);
            p.name[MAX_NAME - 1] = '\0';
            p.pid = 0;
            p.address = 0;
            p.suspended = false;

            if (p.priority == 0) {
                push(priority_q, p);
            } else {
                push(secondary_q, p);
            }
        }
    }

    fclose(fp);
}


void run_priority_queue(queue *priority_q) {
    while (!is_empty(priority_q)) {
        proc p = pop(priority_q);

        int addr = allocate_memory(p.memory);
        if (addr == -1) {
            fprintf(stderr, "Not enough memory for priority process %s\n", p.name);
            exit(EXIT_FAILURE);
        }

        p.address = addr;
        p.pid = start_process();

        print_proc_info(&p);

        sleep(p.runtime);

        kill(p.pid, SIGINT);
        wait_for_termination(p.pid);

        free_memory_block(p.address, p.memory);
    }
}

void run_secondary_queue(queue *secondary_q) {
    while (!is_empty(secondary_q)) {
        int current_count = secondary_q->size;

        for (int i = 0; i < current_count; i++) {
            proc p = pop(secondary_q);

            if (p.pid == 0) {
                int addr = allocate_memory(p.memory);
                if (addr == -1) {

                    push(secondary_q, p);
                    continue;
                }
                p.address = addr;
            }

            if (p.runtime <= 1) {
                if (p.pid == 0) {
                    p.pid = start_process();
                } else if (p.suspended) {
                    kill(p.pid, SIGCONT);
                }

                print_proc_info(&p);

                sleep(1);
                kill(p.pid, SIGINT);
                wait_for_termination(p.pid);
                free_memory_block(p.address, p.memory);
                continue;
            }

            if (p.pid == 0) {
                p.pid = start_process();
            } else if (p.suspended) {
                kill(p.pid, SIGCONT);
            }

            print_proc_info(&p);

            sleep(1);
            kill(p.pid, SIGTSTP);
            wait_for_stop(p.pid);

            p.runtime -= 1;
            p.suspended = true;

            push(secondary_q, p);
        }
    }
}



int main(void) {
    queue priority_q;
    queue secondary_q;

    init_queue(&priority_q);
    init_queue(&secondary_q);
    init_memory();

    load_processes("processes_q2.txt", &priority_q, &secondary_q);

    printf("=== Running Priority Queue ===\n");
    run_priority_queue(&priority_q);

    printf("=== Running Secondary Queue ===\n");
    run_secondary_queue(&secondary_q);

    printf("All processes completed.\n");
    return 0;
}

