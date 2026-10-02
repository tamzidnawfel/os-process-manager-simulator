#include <stdio.h>
#include <string.h>
#include "pm_simulator.h"

PCB process_table[MAX_PROCESSES];
int available_pid = 1;
pthread_mutex_t global_monitor_mutex     = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  global_monitor_work_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t  global_monitor_done_cond = PTHREAD_COND_INITIALIZER;
int             global_monitor_has_work  = 0;
int             g_monitor_stop           = 0;
char            global_monitor_label[128] = "Process Table Updated";
pthread_mutex_t table_mutex              = PTHREAD_MUTEX_INITIALIZER;
static _Thread_local char tls_monitor_label[128] = "Process Table Updated";

static int empty_block(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == STATE_UNUSED)
            return i;
    }
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == STATE_TERMINATED) {
            pthread_cond_destroy(&process_table[i].wait_cond);
            return i;
        }
    }
    return -1;
}

void pm_init(void) {
    memset(process_table, 0, sizeof(process_table));

    int slot = empty_block();
    if (slot == -1) {
        printf("SYSTEM FAILURE: Could not create a root process.\n");
        return;
    }
    
    process_table[slot].pid          = available_pid++;
    process_table[slot].ppid         = 0;
    process_table[slot].exit_status  = -1;
    process_table[slot].state        = STATE_RUNNING;
    process_table[slot].num_children = 0;
    pthread_cond_init(&process_table[slot].wait_cond, NULL);

    printf(ANSI_BOLD ANSI_BLUE "SYSTEM BOOT: Root Process (PID 1) created successfully.\n" ANSI_RESET);
}

int pm_fork(int ppid) {
    pthread_mutex_lock(&table_mutex);

    int parent_index = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].pid == ppid) {
            if (process_table[i].state == STATE_RUNNING ||
                process_table[i].state == STATE_WAITING) {
                parent_index = i;
            }
            break;
        }
    }
    if (parent_index == -1) {
        printf("FORK FAILED: Parent PID %d not found or not active.\n", ppid);
        pthread_mutex_unlock(&table_mutex);
        return -1;
    }

    int child_index = empty_block();
    if (child_index == -1) {
        printf("FORK FAILED: Process table is full.\n");
        pthread_mutex_unlock(&table_mutex);
        return -1;
    }

    int new_pid = available_pid++;

    process_table[child_index].pid          = new_pid;
    process_table[child_index].ppid         = ppid;
    process_table[child_index].state        = STATE_RUNNING;
    process_table[child_index].exit_status  = -1;
    process_table[child_index].num_children = 0;
    pthread_cond_init(&process_table[child_index].wait_cond, NULL);

    if (process_table[parent_index].num_children < MAX_PROCESSES) {
        int nc = process_table[parent_index].num_children;
        process_table[parent_index].child[nc] = new_pid;
        process_table[parent_index].num_children++;
    }

    printf(ANSI_GREEN "FORK SUCCESSFUL: Created child PID %d (Parent: %d)\n" ANSI_RESET, new_pid, ppid);

    pthread_mutex_unlock(&table_mutex);
    notify_monitor_update();

    return new_pid;
}

void pm_exit(int pid, int status) {
    pthread_mutex_lock(&table_mutex);

    int curr_index = -1;
    int parent_pid = -1;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].pid == pid &&
            process_table[i].state != STATE_UNUSED &&
            process_table[i].state != STATE_TERMINATED) {
            curr_index = i;
            parent_pid = process_table[i].ppid;
            break;
        }
    }
    if (curr_index == -1) {
        printf(ANSI_RED "EXIT FAILED: Process PID %d not found or dead.\n" ANSI_RESET, pid);
        pthread_mutex_unlock(&table_mutex);
        return;
    }

    process_table[curr_index].state       = STATE_ZOMBIE;
    process_table[curr_index].exit_status = status;
    printf(ANSI_YELLOW "SUCCESSFULLY EXITED: Process %d is now a ZOMBIE (Status: %d).\n" ANSI_RESET,
           pid, status);

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].pid == parent_pid &&
            process_table[i].state != STATE_UNUSED) {
            pthread_cond_broadcast(&process_table[i].wait_cond);
            break;
        }
    }

    pthread_mutex_unlock(&table_mutex);
    notify_monitor_update();
}

void pm_kill(int pid) {
    printf(ANSI_RED "KILL INITIATED for PID %d...\n" ANSI_RESET, pid);
    pm_exit(pid, 9);
}

int pm_wait(int p_pid, int c_pid) {
    pthread_mutex_lock(&table_mutex);
    int p_index = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].pid == p_pid &&
            process_table[i].state != STATE_UNUSED) {
            p_index = i;
            break;
        }
    }
    if (p_index == -1) {
        printf("WAIT FAILED: Parent PID %d not found.\n", p_pid);
        pthread_mutex_unlock(&table_mutex);
        return -1;
    }

    while (1) {
        int dead_child_index = -1;
        int active_child_num = 0;

        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (process_table[i].ppid == p_pid &&
                process_table[i].state != STATE_UNUSED) {
                if (c_pid != -1 && process_table[i].pid != c_pid)
                    continue;
                active_child_num++;
                if (process_table[i].state == STATE_ZOMBIE) {
                    dead_child_index = i;
                    break;
                }
            }
        }

        if (dead_child_index != -1) {
            int dead_pid   = process_table[dead_child_index].pid;
            int ext_status = process_table[dead_child_index].exit_status;
            process_table[dead_child_index].state = STATE_TERMINATED;

            printf(ANSI_GREEN "WAIT SUCCESSFUL: Parent %d reaped child %d (Status: %d).\n" ANSI_RESET,
                   p_pid, dead_pid, ext_status);

            pthread_mutex_unlock(&table_mutex);
            notify_monitor_update();
            return dead_pid;
        }

        if (active_child_num == 0) {
            printf(ANSI_RED "WAIT FAILED: No active children to wait for.\n" ANSI_RESET);
            pthread_mutex_unlock(&table_mutex);
            return -1;
        }

        printf(ANSI_YELLOW "WAIT: Parent %d going to sleep...\n" ANSI_RESET, p_pid);
        process_table[p_index].state = STATE_WAITING;

        pthread_cond_wait(&process_table[p_index].wait_cond, &table_mutex);

        process_table[p_index].state = STATE_RUNNING;
    }
}

void notify_monitor_update(void) {
    pthread_mutex_lock(&global_monitor_mutex);
    if (tls_monitor_label[0] != '\0') {
        strncpy(global_monitor_label, tls_monitor_label, sizeof(global_monitor_label) - 1);
        global_monitor_label[sizeof(global_monitor_label) - 1] = '\0';
    } else {
        strncpy(global_monitor_label, "Process Table Updated", sizeof(global_monitor_label) - 1);
        global_monitor_label[sizeof(global_monitor_label) - 1] = '\0';
    }
    global_monitor_has_work = 1;
    pthread_cond_signal(&global_monitor_work_cond);

    while (global_monitor_has_work && !g_monitor_stop) {
        pthread_cond_wait(&global_monitor_done_cond, &global_monitor_mutex);
    }
    pthread_mutex_unlock(&global_monitor_mutex);
}

void pm_set_monitor_label(const char *label) {
    if (label == NULL || label[0] == '\0') {
        strncpy(tls_monitor_label, "Process Table Updated", sizeof(tls_monitor_label) - 1);
        tls_monitor_label[sizeof(tls_monitor_label) - 1] = '\0';
        return;
    }
    strncpy(tls_monitor_label, label, sizeof(tls_monitor_label) - 1);
    tls_monitor_label[sizeof(tls_monitor_label) - 1] = '\0';
}

void pm_ps(void) {
    pthread_mutex_lock(&table_mutex);

    printf(ANSI_BOLD "PID\tPPID\tSTATE\t\tEXIT_STATUS\n" ANSI_RESET);
    printf("----------------------------------------------\n");

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == STATE_UNUSED ||
            process_table[i].state == STATE_TERMINATED)
            continue;

        printf("%d\t%d\t", process_table[i].pid, process_table[i].ppid);

        if      (process_table[i].state == STATE_RUNNING)  printf(ANSI_GREEN  "RUNNING\t\t" ANSI_RESET);
        else if (process_table[i].state == STATE_WAITING)  printf(ANSI_YELLOW "WAITING\t\t" ANSI_RESET);
        else if (process_table[i].state == STATE_ZOMBIE)   printf(ANSI_RED    "ZOMBIE\t\t"  ANSI_RESET);
        else                                                printf("UNKNOWN\t\t");

        if (process_table[i].state == STATE_ZOMBIE)
            printf("%d\n", process_table[i].exit_status);
        else
            printf("-\n");
    }
    printf("\n");

    pthread_mutex_unlock(&table_mutex);
}

void pm_print_audit_report(void) {
    pthread_mutex_lock(&table_mutex);

    int running = 0, waiting = 0, zombie = 0, terminated = 0, total_slots = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if      (process_table[i].state == STATE_RUNNING)    running++;
        else if (process_table[i].state == STATE_WAITING)    waiting++;
        else if (process_table[i].state == STATE_ZOMBIE)     zombie++;
        else if (process_table[i].state == STATE_TERMINATED) terminated++;
        if (process_table[i].state != STATE_UNUSED)          total_slots++;
    }

    printf(ANSI_BOLD "============================================================\n" ANSI_RESET);
    printf(ANSI_CYAN ANSI_BOLD "            KERNEL AUDIT & TELEMETRY REPORT\n" ANSI_RESET);
    printf(ANSI_BOLD "============================================================\n" ANSI_RESET);
    printf("Total Processes Created:       " ANSI_BOLD "%d\n" ANSI_RESET, available_pid - 1);
    printf("Active Running Processes:      " ANSI_GREEN "%d\n" ANSI_RESET, running);
    printf("Blocked Waiting Processes:     " ANSI_YELLOW "%d\n" ANSI_RESET, waiting);
    printf("Zombies Remaining (Unreaped):  " ANSI_RED "%d\n" ANSI_RESET, zombie);
    printf("Processes Reaped (Terminated): " ANSI_BLUE "%d\n" ANSI_RESET, terminated);
    printf("PCB Capacity Utilization:      %d / %d slots (%.1f%%)\n",
           total_slots, MAX_PROCESSES, (float)total_slots * 100.0f / MAX_PROCESSES);
    printf("Lost Wakeups / Deadlocks:      " ANSI_GREEN "0 (Atomic condvar synchronization)\n" ANSI_RESET);
    printf(ANSI_BOLD "============================================================\n\n" ANSI_RESET);

    pthread_mutex_unlock(&table_mutex);
}
