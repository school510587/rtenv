#include <stddef.h>

/* Kind of interrupts */
#define INTR_UART_RX 0
#define INTR_UART_TX 1

void *activate(void *stack);

int fork();
int getpid();

int write(int fd, const void *buf, size_t count);
int read(int fd, void *buf, size_t count);

void wait_interrupt(int intr);

int getpriority(int who);
int setpriority(int who, int value);

int mknod(int fd, int mode, int dev);

void sleep(unsigned int);

void process_snapshot(char *buf);

int get_tick_count();
