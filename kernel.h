#ifndef KERNEL_H
#define KERNEL_H

#include <stddef.h>

#define STACK_SIZE 512 /* Size of task stacks in words */
#define TASK_LIMIT 8  /* Max number of tasks we can handle */
#define PIPE_LIMIT (TASK_LIMIT * 2)
#define PIPE_BUF 128 /* Size of largest atomic pipe message */
#define PATH_MAX 32 /* Longest absolute path */

#define PRIORITY_DEFAULT 20
#define PRIORITY_LIMIT (PRIORITY_DEFAULT * 2 - 1)

// Task status list
#define TASK_READY	  0
#define TASK_WAIT_READ  1
#define TASK_WAIT_WRITE 2
#define TASK_WAIT_INTR  3
#define TASK_WAIT_TIME  4

#define O_CREAT 4

/* Stack struct of user thread, see "Exception entry and return" */
struct user_thread_stack {
	unsigned int r4;
	unsigned int r5;
	unsigned int r6;
	unsigned int r7;
	unsigned int r8;
	unsigned int r9;
	unsigned int r10;
	unsigned int fp;
	unsigned int _lr;	   /* Back to system calls or return exception */
	unsigned int _r7;	   /* Backup from isr */
	unsigned int r0;
	unsigned int r1;
	unsigned int r2;
	unsigned int r3;
	unsigned int ip;
	unsigned int lr;	/* Back to user thread code */
	unsigned int pc;
	unsigned int xpsr;
	unsigned int stack[STACK_SIZE - 18];
};

/* Task Control Block */
struct task_control_block {
	struct user_thread_stack *stack;
	int pid;
	int status;
	int priority;
	struct task_control_block **prev;
	struct task_control_block  *next;
};

struct pipe_ringbuffer {
	char data[PIPE_BUF];
	int start;
	int end;

	int (*readable) (struct pipe_ringbuffer*, struct task_control_block*);
	int (*writable) (struct pipe_ringbuffer*, struct task_control_block*);
	int (*read) (struct pipe_ringbuffer*, struct task_control_block*);
	int (*write) (struct pipe_ringbuffer*, struct task_control_block*);
};

int _mknod(struct pipe_ringbuffer *pipe, int dev);
void _read(struct task_control_block *task, struct task_control_block *tasks, size_t task_count, struct pipe_ringbuffer *pipes);
void _write(struct task_control_block *task, struct task_control_block *tasks, size_t task_count, struct pipe_ringbuffer *pipes);
int mkfifo(const char *pathname, int mode);
int mq_open(const char *name, int oflag);
int open(const char *pathname, int flags);
void pathserver();

#endif
