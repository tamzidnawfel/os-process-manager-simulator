#ifndef PM_SIMULATOR_H
#define PM_SIMULATOR_H

#include <pthread.h>
#include <stdbool.h>
#define MAX_PROCESSES 64

typedef enum{
    STATE_UNUSED,
    STATE_RUNNING,
    STATE_WAITING,
    STATE_ZOMBIE,
    STATE_TERMINATED
}   ProcessState;

typedef struct{
    int pid;
    int ppid;
    int exit_status;
    ProcessState state;
    int child [MAX_PROCESSES];
    int num_children;

    pthread_cond_t wait_cond;
} PCB;

extern PCB process_table[MAX_PROCESSES];
extern pthread_mutex_t table_mutex;
extern pthread_mutex_t global_monitor_mutex;
extern pthread_cond_t global_monitor_work_cond;
extern pthread_cond_t global_monitor_done_cond;
extern int global_monitor_has_work;
extern int g_monitor_stop;
extern char global_monitor_label[128];

#define ANSI_RESET   "\033[0m"
#define ANSI_BOLD    "\033[1m"
#define ANSI_RED     "\033[1;31m"
#define ANSI_GREEN   "\033[1;32m"
#define ANSI_YELLOW  "\033[1;33m"
#define ANSI_BLUE    "\033[1;34m"
#define ANSI_MAGENTA "\033[1;35m"
#define ANSI_CYAN    "\033[1;36m"
#define ANSI_GRAY    "\033[0;90m"

void pm_init(void);
int pm_fork(int parent_id);
void pm_exit(int pid, int status);
int pm_wait(int parent_pid, int child_pid);
void pm_kill(int pid);
void pm_ps(void);
void pm_print_audit_report(void);
void pm_set_monitor_label(const char *label);
void notify_monitor_update(void);

#endif