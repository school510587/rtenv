#include "stm32f10x.h"
#include "RTOSConfig.h"

#include "kernel.h"
#include "syscall.h"

#include <stddef.h>
#include <ctype.h> //test ctype

#define MAX_CMDNAME 19
#define MAX_ARGC 19
#define MAX_CMDHELP 1023
#define HISTORY_COUNT 20
#define CMDBUF_SIZE 40
#define MAX_ENVCOUNT 30
#define MAX_ENVNAME 15
#define MAX_ENVVALUE 127

/*Global Variables*/
char next_line[] = {'\n', '\r'};
char cmd[HISTORY_COUNT][CMDBUF_SIZE];
int cur_his=0;
int fdout;
int fdin;

/* Command handlers. */
void export_envvar(int argc, char *argv[]);
void measure_turnaround(int argc, char *argv[]);
void show_echo(int argc, char *argv[]);
void show_cmd_info(int argc, char *argv[]);
void show_task_info(int argc, char *argv[]);
void show_man_page(int argc, char *argv[]);
void show_history(int argc, char *argv[]);

/* Enumeration for command types. */
enum {
	CMD_ECHO = 0,
	CMD_EXPORT,
	CMD_HELP,
	CMD_HISTORY,
	CMD_MAN,
	CMD_PS,
	CMD_TIME,
	CMD_COUNT
} CMD_TYPE;
/* Structure for command handler. */
typedef struct {
	char cmd[MAX_CMDNAME + 1];
	void (*func)(int, char**);
	char description[MAX_CMDHELP + 1];
} hcmd_entry;
const hcmd_entry cmd_data[CMD_COUNT] = {
	[CMD_ECHO] = {.cmd = "echo", .func = show_echo, .description = "Show words you input."},
	[CMD_EXPORT] = {.cmd = "export", .func = export_envvar, .description = "Export environment variables."},
	[CMD_HELP] = {.cmd = "help", .func = show_cmd_info, .description = "List all commands you can use."},
	[CMD_HISTORY] = {.cmd = "history", .func = show_history, .description = "Show latest commands entered."}, 
	[CMD_MAN] = {.cmd = "man", .func = show_man_page, .description = "Manual pager."},
	[CMD_PS] = {.cmd = "ps", .func = show_task_info, .description = "List all the processes."},
	[CMD_TIME] = {.cmd = "time", .func = measure_turnaround, .description = "Measure turnaround time for a command."}
};

/* Structure for environment variables. */
typedef struct {
	char name[MAX_ENVNAME + 1];
	char value[MAX_ENVVALUE + 1];
} evar_entry;
evar_entry env_var[MAX_ENVCOUNT];
int env_count = 0;

void serialout(USART_TypeDef* uart, unsigned int intr)
{
	int fd;
	char c;
	int doread = 1;
	mkfifo("/dev/tty0/out", 0);
	fd = open("/dev/tty0/out", 0);

	while (1) {
		if (doread)
			read(fd, &c, 1);
		doread = 0;
		if (USART_GetFlagStatus(uart, USART_FLAG_TXE) == SET) {
			USART_SendData(uart, c);
			USART_ITConfig(USART2, USART_IT_TXE, ENABLE);
			doread = 1;
		}
		interrupt_wait(intr);
		USART_ITConfig(USART2, USART_IT_TXE, DISABLE);
	}
}

void serialin(USART_TypeDef* uart, unsigned int intr)
{
	int fd;
	char c;
	mkfifo("/dev/tty0/in", 0);
	fd = open("/dev/tty0/in", 0);

    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

	while (1) {
		interrupt_wait(intr);
		if (USART_GetFlagStatus(uart, USART_FLAG_RXNE) == SET) {
			c = USART_ReceiveData(uart);
			write(fd, &c, 1);
		}
	}
}

void echo()
{
	int fdout;
	int fdin;
	char c;

	fdout = open("/dev/tty0/out", 0);
	fdin = open("/dev/tty0/in", 0);

	while (1) {
		read(fdin, &c, 1);
		write(fdout, &c, 1);
	}
}

void rs232_xmit_msg_task()
{
	int fdout;
	int fdin;
	char str[100];
	int curr_char;
	int len;

	fdout = open("/dev/tty0/out", 0);
	fdin = mq_open("/tmp/mqueue/out", O_CREAT);
	setpriority(0, PRIORITY_DEFAULT - 2);

	while (1) {
		/* Read from the queue.  Keep trying until a message is
		 * received.  This will block for a period of time (specified
		 * by portMAX_DELAY). */
		len = read(fdin, str, 100);

		/* Write each character of the message to the RS232 port. */
		curr_char = 0;
		while (curr_char < len) {
			write(fdout, &str[curr_char], 1);
			curr_char++;
		}
	}
}

void serial_test_task()
{
	char c;
	char hint[] =  USER_NAME "@" USER_NAME "-STM32:~$ ";
	char *p = NULL;
	int cmd_count = 0;

	fdout = mq_open("/tmp/mqueue/out", 0);
	fdin = open("/dev/tty0/in", 0);

	for (;; cur_his = (cur_his + 1) % HISTORY_COUNT) {
		p = cmd[cur_his];
		write(fdout, hint, strlen(hint));

		while (1) {
			read(fdin, &c, 1);

			if (c == '\r' || c == '\n') {
				*p = '\0';
				write(fdout, next_line, 2);
				break;
			}
			else if (c == 127 || c == '\b') {
				if (p > cmd[cur_his]) {
					p--;
					write(fdout, "\b \b", 3);
				}
			}
			else if (p - cmd[cur_his] < CMDBUF_SIZE - 1) {
				*p++ = c;
				write(fdout, &c, 1);
			}
		}
		check_keyword();	
	}
}

/* Split command into tokens. */
char *cmdtok(char *cmd)
{
	static char *cur = NULL;
	static char *end = NULL;
	if (cmd) {
		char quo = '\0';
		cur = cmd;
		for (end = cmd; *end; end++) {
			if (*end == '\'' || *end == '\"') {
				if (quo == *end)
					quo = '\0';
				else if (quo == '\0')
					quo = *end;
				*end = '\0';
			}
			else if (isspace(*end) && !quo)
				*end = '\0';
		}
	}
	else
		for (; *cur; cur++)
			;

	for (; *cur == '\0'; cur++)
		if (cur == end) return NULL;
	return cur;
}

void exec_cmd(int argc, char *argv[])
{
	int i;

	for (i = 0; i < CMD_COUNT; i++) {
		if (!strcmp(argv[0], cmd_data[i].cmd)) {
			cmd_data[i].func(argc, argv);
			break;
		}
	}
	if (i == CMD_COUNT) {
		write(fdout, argv[0], strlen(argv[0]));
		write(fdout, ": command not found", 19);
		write(fdout, next_line, 2);
	}
}

void check_keyword()
{
	char *argv[MAX_ARGC + 1] = {NULL};
	char cmdstr[CMDBUF_SIZE];
	char buffer[CMDBUF_SIZE * MAX_ENVVALUE / 2 + 1];
	char *p = buffer;
	int argc = 1;
	int i;

	find_events();
	strcpy(cmdstr, cmd[cur_his]);
	argv[0] = cmdtok(cmdstr);
	if (!argv[0])
		return;

	while (1) {
		argv[argc] = cmdtok(NULL);
		if (!argv[argc])
			break;
		argc++;
		if (argc >= MAX_ARGC)
			break;
	}

	for(i = 0; i < argc; i++) {
		int l = fill_arg(p, argv[i]);
		argv[i] = p;
		p += l + 1;
	}

	exec_cmd(argc, argv);
}

void find_events()
{
	char buf[CMDBUF_SIZE];
	char *p = cmd[cur_his];
	char *q;
	int i;

	for (; *p; p++) {
		if (*p == '!') {
			q = p;
			while (*q && !isspace(*q))
				q++;
			for (i = cur_his + HISTORY_COUNT - 1; i > cur_his; i--) {
				if (!strncmp(cmd[i % HISTORY_COUNT], p + 1, q - p - 1)) {
					strcpy(buf, q);
					strcpy(p, cmd[i % HISTORY_COUNT]);
					p += strlen(p);
					strcpy(p--, buf);
					break;
				}
			}
		}
	}
}

char *find_envvar(const char *name)
{
	int i;

	for (i = 0; i < env_count; i++) {
		if (!strcmp(env_var[i].name, name))
			return env_var[i].value;
	}

	return NULL;
}

/* Fill in entire value of argument. */
int fill_arg(char *const dest, const char *argv)
{
	char env_name[MAX_ENVNAME + 1];
	char *buf = dest;
	char *p = NULL;

	for (; *argv; argv++) {
		if (isalnum(*argv) || *argv == '_') {
			if (p)
				*p++ = *argv;
			else
				*buf++ = *argv;
		}
		else { /* Symbols. */
			if (p) {
				*p = '\0';
				p = find_envvar(env_name);
				if (p) {
					strcpy(buf, p);
					buf += strlen(p);
					p = NULL;
				}
			}
			if (*argv == '$')
				p = env_name;
			else
				*buf++ = *argv;
		}
	}
	if (p) {
		*p = '\0';
		p = find_envvar(env_name);
		if (p) {
			strcpy(buf, p);
			buf += strlen(p);
		}
	}
	*buf = '\0';

	return buf - dest;
}

//export
void export_envvar(int argc, char *argv[])
{
	char *found;
	char *value;
	int i;

	for (i = 1; i < argc; i++) {
		value = argv[i];
		while (*value && *value != '=')
			value++;
		if (*value)
			*value++ = '\0';
		found = find_envvar(argv[i]);
		if (found)
			strcpy(found, value);
		else if (env_count < MAX_ENVCOUNT) {
			strcpy(env_var[env_count].name, argv[i]);
			strcpy(env_var[env_count].value, value);
			env_count++;
		}
	}
}

//time
void measure_turnaround(int argc, char *argv[])
{
	int i = get_tick_count();
	if (argc > 1)
		exec_cmd(argc - 1, argv + 1);

	printf("Turnaround: %d ticks\r\n", get_tick_count() - i);
}

//ps
void show_task_info(int argc, char* argv[])
{
	char buffer[200];
	char ps_message[]="PID STATUS PRIORITY";
	int task_i;
	int task;

	write(fdout, ps_message, strlen(ps_message));
	write(fdout, next_line, 2);
	process_snapshot(buffer);
	write(fdout, buffer, strlen(buffer));
}

//this function helps to show int

void itoa(int n, char *dst, int base)
{
	char buf[33] = {0};
	char *p = &buf[32];

	if (n == 0)
		*--p = '0';
	else {
		char *q;
		unsigned int num = (base == 10 && num < 0) ? -n : n;

		for (; num; num/=base)
			*--p = "0123456789ABCDEF" [num % base];
		if (base == 10 && n < 0)
			*--p = '-';
	}

	strcpy(dst, p);
}

//help

void show_cmd_info(int argc, char* argv[])
{
	const char help_desp[] = "This system has commands as follow\n\r\0";
	int i;

	write(fdout, help_desp, strlen(help_desp));
	for (i = 0; i < CMD_COUNT; i++) {
		write(fdout, cmd_data[i].cmd, strlen(cmd_data[i].cmd));
		write(fdout, ": ", 2);
		write(fdout, cmd_data[i].description, strlen(cmd_data[i].description));
		write(fdout, next_line, 2);
	}
}

//echo
void show_echo(int argc, char* argv[])
{
	const int _n = 1; /* Flag for "-n" option. */
	int flag = 0;
	int i;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-n"))
			flag |= _n;
		else
			break;
	}

	for (; i < argc; i++) {
		write(fdout, argv[i], strlen(argv[i]));
		if (i < argc - 1)
			write(fdout, " ", 1);
	}

	if (~flag & _n)
		write(fdout, next_line, 2);
}

//man
void show_man_page(int argc, char *argv[])
{
	int i;

	if (argc < 2)
		return;

	for (i = 0; i < CMD_COUNT && strcmp(cmd_data[i].cmd, argv[1]); i++)
		;

	if (i >= CMD_COUNT)
		return;

	write(fdout, "NAME: ", 6);
	write(fdout, cmd_data[i].cmd, strlen(cmd_data[i].cmd));
	write(fdout, next_line, 2);
	write(fdout, "DESCRIPTION: ", 13);
	write(fdout, cmd_data[i].description, strlen(cmd_data[i].description));
	write(fdout, next_line, 2);
}

void show_history(int argc, char *argv[])
{
	int i;

	for (i = cur_his + 1; i <= cur_his + HISTORY_COUNT; i++) {
		if (cmd[i % HISTORY_COUNT][0]) {
			write(fdout, cmd[i % HISTORY_COUNT], strlen(cmd[i % HISTORY_COUNT]));
			write(fdout, next_line, 2);
		}
	}
}

int write_blank(int blank_num)
{
	int blank_count = 0;

	while (blank_count <= blank_num) {
		write(fdout, " ", 1);
		blank_count++;
	}
}

void first()
{
	setpriority(0, 0);

	if (!fork()) setpriority(0, 0), pathserver();
	if (!fork()) setpriority(0, 0), serialout(USART2, USART2_IRQn);
	if (!fork()) setpriority(0, 0), serialin(USART2, USART2_IRQn);
	if (!fork()) rs232_xmit_msg_task();
	if (!fork()) setpriority(0, PRIORITY_DEFAULT - 10), serial_test_task();

	setpriority(0, PRIORITY_LIMIT);

	while(1);
}

#define RB_PUSH(rb, size, v) do { \
		(rb).data[(rb).end] = (v); \
		(rb).end++; \
		if ((rb).end >= size) (rb).end = 0; \
	} while (0)

#define RB_POP(rb, size, v) do { \
		(v) = (rb).data[(rb).start]; \
		(rb).start++; \
		if ((rb).start >= size) (rb).start = 0; \
	} while (0)

#define RB_PEEK(rb, size, v, i) do { \
		int _counter = (i); \
		int _src_index = (rb).start; \
		int _dst_index = 0; \
		while (_counter--) { \
			((char*)&(v))[_dst_index++] = (rb).data[_src_index++]; \
			if (_src_index >= size) _src_index = 0; \
		} \
	} while (0)

#define RB_LEN(rb, size) (((rb).end - (rb).start) + \
	(((rb).end < (rb).start) ? size : 0))

#define PIPE_PUSH(pipe, v) RB_PUSH((pipe), PIPE_BUF, (v))
#define PIPE_POP(pipe, v)  RB_POP((pipe), PIPE_BUF, (v))
#define PIPE_PEEK(pipe, v, i)  RB_PEEK((pipe), PIPE_BUF, (v), (i))
#define PIPE_LEN(pipe)     (RB_LEN((pipe), PIPE_BUF))

unsigned int *init_task(unsigned int *stack, void (*start)())
{
	stack += STACK_SIZE - 9; /* End of stack, minus what we're about to push */
	stack[8] = (unsigned int)start;
	return stack;
}

int
task_push (struct task_control_block **list, struct task_control_block *item)
{
	if (list && item) {
		/* Remove itself from original list */
		if (item->prev)
			*(item->prev) = item->next;
		if (item->next)
			item->next->prev = item->prev;
		/* Insert into new list */
		while (*list) list = &((*list)->next);
		*list = item;
		item->prev = list;
		item->next = NULL;
		return 0;
	}
	return -1;
}

struct task_control_block*
task_pop (struct task_control_block **list)
{
	if (list) {
		struct task_control_block *item = *list;
		if (item) {
			*list = item->next;
			if (item->next)
				item->next->prev = list;
			item->prev = NULL;
			item->next = NULL;
			return item;
		}
	}
	return NULL;
}

void list_tasks(char *buf, const struct task_control_block *tasks, int task_count)
{
	int i;

	for (i = 0; i < task_count; i++) {
		sprintf(buf, "%d   %d     %d\r\n", tasks[i].pid, tasks[i].status, tasks[i].priority);
		buf += strlen(buf);
	}
}

int main()
{
	unsigned int stacks[TASK_LIMIT][STACK_SIZE];
	struct task_control_block tasks[TASK_LIMIT];
	struct pipe_ringbuffer pipes[PIPE_LIMIT];
	struct task_control_block *ready_list[PRIORITY_LIMIT + 1];  /* [0 ... 39] */
	struct task_control_block *wait_list = NULL;
	size_t task_count = 0;
	size_t current_task = 0;
	size_t i;
	struct task_control_block *task;
	int timeup;
	unsigned int tick_count = 0;

	SysTick_Config(configCPU_CLOCK_HZ / configTICK_RATE_HZ);

	init_rs232();
	__enable_irq();

	tasks[task_count].stack = (void*)init_task(stacks[task_count], &first);
	tasks[task_count].pid = 0;
	tasks[task_count].priority = PRIORITY_DEFAULT;
	task_count++;

	init_io(pipes);

	/* Initialize ready lists */
	for (i = 0; i <= PRIORITY_LIMIT; i++)
		ready_list[i] = NULL;

	while (1) {
		tasks[current_task].stack = activate(tasks[current_task].stack);
		tasks[current_task].status = TASK_READY;
		timeup = 0;

		switch (tasks[current_task].stack->r7) {
		case 0x1: /* fork */
			if (task_count == TASK_LIMIT) {
				/* Cannot create a new task, return error */
				tasks[current_task].stack->r0 = -1;
			}
			else {
				/* Compute how much of the stack is used */
				size_t used = stacks[current_task] + STACK_SIZE
					      - (unsigned int*)tasks[current_task].stack;
				/* New stack is END - used */
				tasks[task_count].stack = (void*)(stacks[task_count] + STACK_SIZE - used);
				/* Copy only the used part of the stack */
				memcpy(tasks[task_count].stack, tasks[current_task].stack,
				       used * sizeof(unsigned int));
				/* Set PID */
				tasks[task_count].pid = task_count;
				/* Set priority, inherited from forked task */
				tasks[task_count].priority = tasks[current_task].priority;
				/* Set return values in each process */
				tasks[current_task].stack->r0 = task_count;
				tasks[task_count].stack->r0 = 0;
				tasks[task_count].prev = NULL;
				tasks[task_count].next = NULL;
				task_push(&ready_list[tasks[task_count].priority], &tasks[task_count]);
				/* There is now one more task */
				task_count++;
			}
			break;
		case 0x2: /* getpid */
			tasks[current_task].stack->r0 = current_task;
			break;
		case 0x3: /* write */
			_write(&tasks[current_task], tasks, task_count, pipes);
			break;
		case 0x4: /* read */
			_read(&tasks[current_task], tasks, task_count, pipes);
			break;
		case 0x5: /* interrupt_wait */
			/* Enable interrupt */
			NVIC_EnableIRQ(tasks[current_task].stack->r0);
			/* Block task waiting for interrupt to happen */
			tasks[current_task].status = TASK_WAIT_INTR;
			break;
		case 0x6: /* getpriority */
			{
				int who = tasks[current_task].stack->r0;
				if (who > 0 && who < (int)task_count)
					tasks[current_task].stack->r0 = tasks[who].priority;
				else if (who == 0)
					tasks[current_task].stack->r0 = tasks[current_task].priority;
				else
					tasks[current_task].stack->r0 = -1;
			} break;
		case 0x7: /* setpriority */
			{
				int who = tasks[current_task].stack->r0;
				int value = tasks[current_task].stack->r1;
				value = (value < 0) ? 0 : ((value > PRIORITY_LIMIT) ? PRIORITY_LIMIT : value);
				if (who > 0 && who < (int)task_count)
					tasks[who].priority = value;
				else if (who == 0)
					tasks[current_task].priority = value;
				else {
					tasks[current_task].stack->r0 = -1;
					break;
				}
				tasks[current_task].stack->r0 = 0;
			} break;
		case 0x8: /* mknod */
			if (tasks[current_task].stack->r0 < PIPE_LIMIT)
				tasks[current_task].stack->r0 =
					_mknod(&pipes[tasks[current_task].stack->r0],
						   tasks[current_task].stack->r2);
			else
				tasks[current_task].stack->r0 = -1;
			break;
		case 0x9: /* sleep */
			if (tasks[current_task].stack->r0 != 0) {
				tasks[current_task].stack->r0 += tick_count;
				tasks[current_task].status = TASK_WAIT_TIME;
			}
			break;
		case 0xA: /* process_snapshot */
			list_tasks((char*)tasks[current_task].stack->r0, tasks, task_count);
			break;
		case 0xB: /* get_tick_count */
			tasks[current_task].stack->r0 = tick_count;
			break;
		default: /* Catch all interrupts */
			if ((int)tasks[current_task].stack->r7 < 0) {
				unsigned int intr = -tasks[current_task].stack->r7 - 16;

				if (intr == SysTick_IRQn) {
					/* Never disable timer. We need it for pre-emption */
					timeup = 1;
					tick_count++;
				}
				else {
					/* Disable interrupt, interrupt_wait re-enables */
					NVIC_DisableIRQ(intr);
				}
				/* Unblock any waiting tasks */
				for (i = 0; i < task_count; i++)
					if ((tasks[i].status == TASK_WAIT_INTR && tasks[i].stack->r0 == intr) ||
					    (tasks[i].status == TASK_WAIT_TIME && tasks[i].stack->r0 == tick_count))
						tasks[i].status = TASK_READY;
			}
		}

		/* Put waken tasks in ready list */
		for (task = wait_list; task != NULL;) {
			struct task_control_block *next = task->next;
			if (task->status == TASK_READY)
				task_push(&ready_list[task->priority], task);
			task = next;
		}
		/* Select next TASK_READY task */
		for (i = 0; i < (size_t)tasks[current_task].priority && ready_list[i] == NULL; i++);
		if (tasks[current_task].status == TASK_READY) {
			if (!timeup && i == (size_t)tasks[current_task].priority)
				/* Current task has highest priority and remains execution time */
				continue;
			else
				task_push(&ready_list[tasks[current_task].priority], &tasks[current_task]);
		}
		else {
			task_push(&wait_list, &tasks[current_task]);
		}
		while (ready_list[i] == NULL)
			i++;
		current_task = task_pop(&ready_list[i])->pid;
	}

	return 0;
}
