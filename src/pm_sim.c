#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include "pm_simulator.h"

#define MAX_THREADS  64
#define MAX_LINE_LEN 256

extern int available_pid;

typedef struct {
    int  thread_id;
    char filename[256];
} ThreadArgs;

static FILE *g_snapshot_file = NULL;

#ifdef _WIN32
#include <windows.h>
static void enable_ansi_colors(void) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;
    dwMode |= 0x0004; // ENABLE_VIRTUAL_TERMINAL_PROCESSING
    SetConsoleMode(hOut, dwMode);
}
#else
static void enable_ansi_colors(void) {}
#endif

static const char *get_thread_color(int tid) {
    switch (tid % 4) {
        case 0: return ANSI_CYAN;
        case 1: return ANSI_MAGENTA;
        case 2: return ANSI_YELLOW;
        default: return ANSI_BLUE;
    }
}

static void sleep_ms(int ms) {
    struct timespec req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&req, NULL);
}

static void write_snapshot(const char *label) {
    fprintf(g_snapshot_file, "%s\n", label);
    fprintf(g_snapshot_file, "PID\tPPID\tSTATE\t\tEXIT_STATUS\n");
    fprintf(g_snapshot_file, "----------------------------------------------\n");

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == STATE_UNUSED ||
            process_table[i].state == STATE_TERMINATED)
            continue;

        fprintf(g_snapshot_file, "%d\t%d\t",
                process_table[i].pid, process_table[i].ppid);

        if      (process_table[i].state == STATE_RUNNING)  fprintf(g_snapshot_file, "RUNNING\t\t");
        else if (process_table[i].state == STATE_WAITING)  fprintf(g_snapshot_file, "WAITING\t\t");
        else if (process_table[i].state == STATE_ZOMBIE)   fprintf(g_snapshot_file, "ZOMBIE\t\t");
        else                                                fprintf(g_snapshot_file, "UNKNOWN\t\t");

        if (process_table[i].state == STATE_ZOMBIE)
            fprintf(g_snapshot_file, "%d\n", process_table[i].exit_status);
        else
            fprintf(g_snapshot_file, "-\n");
    }

    fprintf(g_snapshot_file, "\n");
    fflush(g_snapshot_file);
}

static void *monitor_thread_func(void *arg) {
    (void)arg;

    pthread_mutex_lock(&global_monitor_mutex);
    while (1) {
        while (!global_monitor_has_work && !g_monitor_stop) {
            pthread_cond_wait(&global_monitor_work_cond, &global_monitor_mutex);
        }
        if (!global_monitor_has_work && g_monitor_stop) {
            break;
        }

        char snapshot_label[128];
        strncpy(snapshot_label, global_monitor_label, sizeof(snapshot_label) - 1);
        snapshot_label[sizeof(snapshot_label) - 1] = '\0';

        pthread_mutex_lock(&table_mutex);
        write_snapshot(snapshot_label);
        pthread_mutex_unlock(&table_mutex);

        global_monitor_has_work = 0;
        pthread_cond_broadcast(&global_monitor_done_cond);
    }
    pthread_mutex_unlock(&global_monitor_mutex);
    return NULL;
}

static void *worker_thread_func(void *arg) {
    ThreadArgs *targs = (ThreadArgs *)arg;
    int  tid = targs->thread_id;
    char filename[256];
    strncpy(filename, targs->filename, 255);
    filename[255] = '\0';

    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Thread %d: Could not open file %s\n", tid, filename);
        return NULL;
    }

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        char cmd[64];
        int  a1, a2;

        if (sscanf(line, "%63s", cmd) != 1) continue;

        const char *tcolor = get_thread_color(tid);

        if (strcmp(cmd, "fork") == 0) {
            if (sscanf(line, "%*s %d", &a1) == 1) {
                printf("%s[Thread %d]%s calls pm_fork %d\n", tcolor, tid, ANSI_RESET, a1);
                char label[128];
                snprintf(label, sizeof(label), "Thread %d calls pm_fork %d", tid, a1);
                pm_set_monitor_label(label);
                (void)pm_fork(a1);
            }

        } else if (strcmp(cmd, "exit") == 0) {
            if (sscanf(line, "%*s %d %d", &a1, &a2) == 2) {
                printf("%s[Thread %d]%s calls pm_exit %d %d\n", tcolor, tid, ANSI_RESET, a1, a2);
                char label[128];
                snprintf(label, sizeof(label), "Thread %d calls pm_exit %d %d", tid, a1, a2);
                pm_set_monitor_label(label);
                pm_exit(a1, a2);
            }

        } else if (strcmp(cmd, "wait") == 0) {
            if (sscanf(line, "%*s %d %d", &a1, &a2) == 2) {
                printf("%s[Thread %d]%s calls pm_wait %d %d\n", tcolor, tid, ANSI_RESET, a1, a2);
                char label[128];
                snprintf(label, sizeof(label), "Thread %d calls pm_wait %d %d", tid, a1, a2);
                pm_set_monitor_label(label);
                (void)pm_wait(a1, a2);
            }

        } else if (strcmp(cmd, "kill") == 0) {
            if (sscanf(line, "%*s %d", &a1) == 1) {
                printf("%s[Thread %d]%s calls pm_kill %d\n", tcolor, tid, ANSI_RESET, a1);
                char label[128];
                snprintf(label, sizeof(label), "Thread %d calls pm_kill %d", tid, a1);
                pm_set_monitor_label(label);
                pm_kill(a1);
            }

        } else if (strcmp(cmd, "sleep") == 0) {
            if (sscanf(line, "%*s %d", &a1) == 1) {
                printf("%s[Thread %d]%s sleeping %d ms\n", tcolor, tid, ANSI_RESET, a1);
                sleep_ms(a1);
            }

        } else if (strcmp(cmd, "ps") == 0) {
            printf("%s[Thread %d]%s calls pm_ps\n", tcolor, tid, ANSI_RESET);
            pm_ps();

        } else {
            printf("%s[Thread %d]%s: Unknown command '%s'\n", tcolor, tid, ANSI_RESET, cmd);
        }
    }

    fclose(f);
    return NULL;
}

int main(int argc, char *argv[]) {
    enable_ansi_colors();

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <script1.txt> [script2.txt] ...\n", argv[0]);
        return 1;
    }

    g_snapshot_file = fopen("snapshots.txt", "w");
    if (!g_snapshot_file) {
        perror("Could not open snapshots.txt");
        return 1;
    }

    time_t now = time(NULL);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(g_snapshot_file, "==============================================\n");
    fprintf(g_snapshot_file, "  SIMULATION RUN SNAPSHOT LOG (%s)\n", time_buf);
    fprintf(g_snapshot_file, "==============================================\n\n");

    pm_init();

    pthread_t monitor_tid;
    pthread_create(&monitor_tid, NULL, monitor_thread_func, NULL);

    pm_set_monitor_label("Initial Process Table");
    notify_monitor_update();

    int        num_workers = argc - 1;
    pthread_t  worker_tids[MAX_THREADS];
    ThreadArgs worker_args[MAX_THREADS];

    for (int i = 0; i < num_workers && i < MAX_THREADS; i++) {
        worker_args[i].thread_id = i;
        strncpy(worker_args[i].filename, argv[i + 1], 255);
        worker_args[i].filename[255] = '\0';
        pthread_create(&worker_tids[i], NULL, worker_thread_func, &worker_args[i]);
    }

    for (int i = 0; i < num_workers && i < MAX_THREADS; i++) {
        pthread_join(worker_tids[i], NULL);
    }

    pthread_mutex_lock(&global_monitor_mutex);
    g_monitor_stop = 1;
    pthread_cond_broadcast(&global_monitor_work_cond);
    pthread_cond_broadcast(&global_monitor_done_cond);
    pthread_mutex_unlock(&global_monitor_mutex);
    pthread_join(monitor_tid, NULL);

    fclose(g_snapshot_file);

    printf("\nAll worker threads finished.\n\n");
    printf(ANSI_BOLD "Final Process Table State:\n" ANSI_RESET);
    pm_ps();

    pm_print_audit_report();

    return 0;
}
