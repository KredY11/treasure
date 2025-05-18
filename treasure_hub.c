#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/select.h>

// Add the Treasure struct definition
#define MAX_CLUE 100
#define MAX_USER 50

typedef struct {
    char id[20];
    char user[MAX_USER];
    float latitude;
    float longitude;
    char clue[MAX_CLUE];
    int value;
} Treasure;

pid_t monitor_pid = -1;
int monitor_running = 0;
int monitor_output_fd = -1;

void sigchld_handler(int sig) {
    int status;
    waitpid(monitor_pid, &status, 0);
    monitor_running = 0;
    if (monitor_output_fd != -1) {
        close(monitor_output_fd);
        monitor_output_fd = -1;
    }
    printf("[Monitor process terminated]\n");
}

void setup_sigchld_handler() {
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGCHLD, &sa, NULL);
}

void start_monitor() {
    if (monitor_running) {
        printf("Monitor is already running.\n");
        return;
    }

    // Create pipe for monitor output
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        return;
    }

    // Create FIFO if it doesn't exist
    if (access("/tmp/monitor_pipe", F_OK) == -1) {
        if (mkfifo("/tmp/monitor_pipe", 0666) == -1) {
            perror("mkfifo");
            return;
        }
    }

    setup_sigchld_handler();

    monitor_pid = fork();
    if (monitor_pid == 0) {
        // Child: Monitor process
        close(pipe_fd[0]);
        char pipe_fd_str[10];
        snprintf(pipe_fd_str, sizeof(pipe_fd_str), "%d", pipe_fd[1]);
        execlp("./monitor", "monitor", pipe_fd_str, NULL);
        perror("execlp failed");
        exit(1);
    } else if (monitor_pid > 0) {
        // Parent
        close(pipe_fd[1]);
        monitor_output_fd = pipe_fd[0];
        monitor_running = 1;
        printf("[Monitor process started]\n");

        char command[128];
        printf("Enter monitor commands at the 'monitor>' prompt (e.g., list_hunts, list_treasures, view_treasure, stop_monitor, exit)\n");
        while (monitor_running) {
            printf("monitor> ");
            fflush(stdout);
            if (!fgets(command, sizeof(command), stdin)) break;
            command[strcspn(command, "\n")] = 0;

            if (strcmp(command, "list_hunts") == 0 ||
                strcmp(command, "list_treasures") == 0 ||
                strncmp(command, "view_treasure", 13) == 0) {

                kill(monitor_pid, SIGUSR2);
                int fd = open("/tmp/monitor_pipe", O_WRONLY);
                if (fd >= 0) {
                    write(fd, command, strlen(command));
                    write(fd, "\n", 1);
                    close(fd);

                    // Read monitor output with a timeout
                    char buffer[1024];
                    ssize_t n;
                    fd_set read_fds;
                    struct timeval timeout;

                    while (1) {
                        FD_ZERO(&read_fds);
                        FD_SET(monitor_output_fd, &read_fds);
                        timeout.tv_sec = 1;
                        timeout.tv_usec = 0;

                        int ready = select(monitor_output_fd + 1, &read_fds, NULL, NULL, &timeout);
                        if (ready < 0) {
                            perror("[Debug] select error");
                            break;
                        } else if (ready == 0) {
                            break;
                        }

                        n = read(monitor_output_fd, buffer, sizeof(buffer) - 1);
                        if (n <= 0) {
                            if (n < 0) perror("[Debug] Error reading pipe");
                            break;
                        }
                        buffer[n] = '\0';
                        printf("%s", buffer);
                        fflush(stdout);
                    }
                } else {
                    printf("[Error writing to pipe]\n");
                }

            } else if (strcmp(command, "stop_monitor") == 0) {
                kill(monitor_pid, SIGUSR2);
                int fd = open("/tmp/monitor_pipe", O_WRONLY);
                if (fd >= 0) {
                    write(fd, command, strlen(command));
                    write(fd, "\n", 1);
                    close(fd);
                }
                sleep(1);
                break;
            } else if (strcmp(command, "exit") == 0) {
                if (monitor_running) {
                    // Stop the monitor before exiting
                    kill(monitor_pid, SIGUSR2);
                    int fd = open("/tmp/monitor_pipe", O_WRONLY);
                    if (fd >= 0) {
                        write(fd, "stop_monitor", strlen("stop_monitor"));
                        write(fd, "\n", 1);
                        close(fd);
                    }
                    sleep(1); // Wait for monitor to terminate
                }
                break;
            } else {
                printf("Unknown command. Try: list_hunts, list_treasures, view_treasure, stop_monitor, exit\n");
            }
        }
    } else {
        perror("fork failed");
    }
}

void calculate_score() {
    DIR* dir = opendir("hunts");
    if (!dir) {
        printf("Failed to open hunts directory\n");
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            int treasure_pipe[2], score_pipe[2];
            if (pipe(treasure_pipe) == -1 || pipe(score_pipe) == -1) {
                perror("pipe");
                continue;
            }

            pid_t calc_pid = fork();
            if (calc_pid == 0) {
                // Child: Run score_calculator
                close(treasure_pipe[1]);
                close(score_pipe[0]);
                dup2(treasure_pipe[0], STDIN_FILENO);
                dup2(score_pipe[1], STDOUT_FILENO);
                close(treasure_pipe[0]);
                close(score_pipe[1]);
                execlp("./score_calculator", "score_calculator", NULL);
                perror("execlp score_calculator failed");
                exit(1);
            } else if (calc_pid > 0) {
                // Parent: Send treasures to score_calculator
                close(treasure_pipe[0]);
                close(score_pipe[1]);

                char filepath[512];
                snprintf(filepath, sizeof(filepath), "hunts/%s/data.bin", entry->d_name);
                int fd = open(filepath, O_RDONLY);
                if (fd >= 0) {
                    Treasure t;
                    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
                        write(treasure_pipe[1], &t, sizeof(Treasure));
                    }
                    close(fd);
                }
                close(treasure_pipe[1]);

                // Read scores from score_calculator with timeout
                char buffer[1024];
                ssize_t n;
                fd_set read_fds;
                struct timeval timeout;
                printf("Scores for hunt %s:\n", entry->d_name);
                while (1) {
                    FD_ZERO(&read_fds);
                    FD_SET(score_pipe[0], &read_fds);
                    timeout.tv_sec = 1;
                    timeout.tv_usec = 0;

                    int ready = select(score_pipe[0] + 1, &read_fds, NULL, NULL, &timeout);
                    if (ready < 0) {
                        perror("[Debug] select error in calculate_score");
                        break;
                    } else if (ready == 0) {
                        break;
                    }

                    n = read(score_pipe[0], buffer, sizeof(buffer) - 1);
                    if (n <= 0) {
                        if (n < 0) perror("[Debug] Error reading score pipe");
                        break;
                    }
                    buffer[n] = '\0';
                    printf("%s", buffer);
                    fflush(stdout);
                }
                close(score_pipe[0]);
                waitpid(calc_pid, NULL, 0);
            } else {
                perror("fork failed");
            }
        }
    }
    closedir(dir);
}

int main() {
    char input[128];
    while (1) {
        printf("treasure_hub> ");
        fflush(stdout);
        if (!fgets(input, sizeof(input), stdin)) break;
        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, "start_monitor") == 0) {
            start_monitor();
        } else if (strcmp(input, "calculate_score") == 0) {
            calculate_score();
        } else if (strcmp(input, "exit") == 0) {
            if (monitor_running)
                printf("Error: monitor still running\n");
            else
                break;
        } else {
            printf("Available command: start_monitor, calculate_score, exit\n");
        }
    }
    return 0;
}