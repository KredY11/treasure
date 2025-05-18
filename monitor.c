#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

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

volatile sig_atomic_t command_ready = 0;
volatile sig_atomic_t stop_monitor = 0;

void sigusr2_handler(int sig) {
    command_ready = 1;
}

void sigusr1_handler(int sig) {
    stop_monitor = 1;
}

void setup_signal_handlers() {
    struct sigaction sa_usr2 = {0}, sa_usr1 = {0};

    sa_usr2.sa_handler = sigusr2_handler;
    sigemptyset(&sa_usr2.sa_mask);
    sa_usr2.sa_flags = 0;
    sigaction(SIGUSR2, &sa_usr2, NULL);

    sa_usr1.sa_handler = sigusr1_handler;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = 0;
    sigaction(SIGUSR1, &sa_usr1, NULL);
}

void send_to_pipe(int fd, const char* data) {
    write(fd, data, strlen(data));
}

void list_hunts(int output_fd) {
    DIR* dir = opendir("hunts");
    if (!dir) {
        send_to_pipe(output_fd, "Failed to open hunts directory\n");
        return;
    }

    struct dirent* entry;
    send_to_pipe(output_fd, "[Monitor] Available hunts:\n");
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char buf[512];
            snprintf(buf, sizeof(buf), "- %s\n", entry->d_name);
            send_to_pipe(output_fd, buf);
        }
    }
    closedir(dir);
}

void list_treasures(int output_fd) {
    DIR* dir = opendir("hunts");
    if (!dir) {
        send_to_pipe(output_fd, "Failed to open hunts directory\n");
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char filepath[512];
            snprintf(filepath, sizeof(filepath), "hunts/%s/data.bin", entry->d_name);
            FILE* f = fopen(filepath, "rb");
            if (f) {
                Treasure t;
                char buf[512];
                snprintf(buf, sizeof(buf), "[Monitor] Treasures in hunt: %s\n", entry->d_name);
                send_to_pipe(output_fd, buf);
                while (fread(&t, sizeof(Treasure), 1, f) == 1) {
                    snprintf(buf, sizeof(buf), "  ID: %s, User: %s, GPS: %.2f %.2f, Clue: %s, Value: %d\n",
                           t.id, t.user, t.latitude, t.longitude, t.clue, t.value);
                    send_to_pipe(output_fd, buf);
                }
                fclose(f);
            }
        }
    }
    closedir(dir);
}

void view_treasure(char* cmd, int output_fd) {
    char* tid = strchr(cmd, ' ');
    if (!tid) {
        send_to_pipe(output_fd, "[Monitor] Invalid view_treasure command.\n");
        return;
    }
    tid++;

    DIR* dir = opendir("hunts");
    if (!dir) {
        send_to_pipe(output_fd, "Failed to open hunts directory\n");
        return;
    }

    struct dirent* entry;
    int found = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char filepath[512];
            snprintf(filepath, sizeof(filepath), "hunts/%s/data.bin", entry->d_name);
            FILE* f = fopen(filepath, "rb");
            if (f) {
                Treasure t;
                char buf[512];
                while (fread(&t, sizeof(Treasure), 1, f) == 1) {
                    if (strcmp(t.id, tid) == 0) {
                        snprintf(buf, sizeof(buf), "[Monitor] Found treasure in hunt: %s\nID: %s, User: %s, GPS: %.2f %.2f, Clue: %s, Value: %d\n",
                               entry->d_name, t.id, t.user, t.latitude, t.longitude, t.clue, t.value);
                        send_to_pipe(output_fd, buf);
                        found = 1;
                        break;
                    }
                }
                fclose(f);
                if (found) break;
            }
        }
    }
    if (!found) {
        char buf[100];
        snprintf(buf, sizeof(buf), "[Monitor] Treasure ID '%s' not found.\n", tid);
        send_to_pipe(output_fd, buf);
    }
    closedir(dir);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <output_pipe_fd>\n", argv[0]);
        exit(1);
    }

    int output_fd = atoi(argv[1]); // Get the write end of the pipe from command-line argument

    printf("[Monitor started and listening...]\n");
    fflush(stdout);

    setup_signal_handlers();

    while (!stop_monitor) {
        pause();

        if (command_ready) {
            command_ready = 0;

            int fd = open("/tmp/monitor_pipe", O_RDONLY);
            if (fd < 0) {
                perror("[Monitor] open command pipe");
                continue;
            }

            char command[512];
            int n = read(fd, command, sizeof(command) - 1);
            close(fd);
            if (n <= 0) {
                printf("[Monitor] Failed to read command\n");
                continue;
            }
            command[n] = '\0';

            if (strcmp(command, "list_hunts") == 0) {
                list_hunts(output_fd);
            } else if (strcmp(command, "list_treasures") == 0) {
                list_treasures(output_fd);
            } else if (strncmp(command, "view_treasure", 13) == 0) {
                view_treasure(command, output_fd);
            } else if (strcmp(command, "stop_monitor") == 0) {
                send_to_pipe(output_fd, "[Monitor] Stopping on command.\n");
                stop_monitor = 1;
            } else {
                char buf[540];
                snprintf(buf, sizeof(buf), "[Monitor] Unknown command: %s\n", command);
                send_to_pipe(output_fd, buf);
            }
        }
    }

    close(output_fd);
    printf("[Monitor process terminated]\n");
    return 0;
}