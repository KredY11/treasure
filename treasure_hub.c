#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>

#define ID_SIZE 32
#define NAME_SIZE 32
#define CLUE_SIZE 128

typedef struct {
    char treasure_id[ID_SIZE];
    char user_name[NAME_SIZE];
    float latitude;
    float longitude;
    char clue[CLUE_SIZE];
    int value;
} Treasure;

#define RECORD_SIZE sizeof(Treasure)

volatile sig_atomic_t signal_received = 0;

void handle_sigusr1(int sig) {
    signal_received = 1;
}

void handle_sigint(int sig) {
    fprintf(stderr, "treasure_hub: Terminated\n");
    unlink("treasure_hub.pid");
    exit(EXIT_FAILURE);
}

void list_hunts() {
    DIR *dir;
    struct dirent *entry;
    dir = opendir(".");
    if (!dir) {
        perror("Failed to open directory");
        return;
    }

    printf("Hunts:\n");
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "Hunt", 4) == 0) {
            char treasures_path[512];
            snprintf(treasures_path, sizeof(treasures_path), "%s/treasures.bin", entry->d_name);
            
            struct stat st;
            if (stat(treasures_path, &st) == -1) continue;

            int treasure_count = st.st_size / RECORD_SIZE;
            printf("%s: %d treasures\n", entry->d_name + 4, treasure_count);
        }
    }
    closedir(dir);
}

void list_treasures(const char *hunt_id) {
    char treasures_path[512];
    snprintf(treasures_path, sizeof(treasures_path), "Hunt%s/treasures.bin", hunt_id);

    struct stat st;
    if (stat(treasures_path, &st) == -1) {
        perror("Failed to get file stats");
        return;
    }

    printf("Hunt: %s\n", hunt_id);
    printf("File Size: %ld bytes\n", st.st_size);
    printf("Last Modified: %s", ctime(&st.st_mtime));
    printf("Treasures:\n");

    int fd = open(treasures_path, O_RDONLY);
    if (fd == -1) {
        perror("Failed to open treasures.bin");
        return;
    }

    Treasure t;
    while (read(fd, &t, RECORD_SIZE) == RECORD_SIZE) {
        printf("Treasure ID: %s, User: %s, Value: %d\n", t.treasure_id, t.user_name, t.value);
    }
    close(fd);
}

void view_treasure(const char *hunt_id, const char *treasure_id) {
    char treasures_path[512];
    snprintf(treasures_path, sizeof(treasures_path), "Hunt%s/treasures.bin", hunt_id);

    int fd = open(treasures_path, O_RDONLY);
    if (fd == -1) {
        perror("Failed to open treasures.bin");
        return;
    }

    Treasure t;
    int found = 0;
    while (read(fd, &t, RECORD_SIZE) == RECORD_SIZE) {
        if (strncmp(t.treasure_id, treasure_id, ID_SIZE) == 0) {
            printf("Treasure ID: %s\n", t.treasure_id);
            printf("User: %s\n", t.user_name);
            printf("GPS: (%f, %f)\n", t.latitude, t.longitude);
            printf("Clue: %s\n", t.clue);
            printf("Value: %d\n", t.value);
            found = 1;
            break;
        }
    }
    if (!found) {
        printf("Treasure %s not found in hunt %s.\n", treasure_id, hunt_id);
    }
    close(fd);
}

void process_command() {
    FILE *fp = fopen("command.txt", "r");
    if (!fp) {
        perror("Failed to open command.txt");
        return;
    }

    char command[256], hunt_id[128], treasure_id[128];
    fscanf(fp, "%s", command);

    if (strcmp(command, "list_hunts") == 0) {
        list_hunts();
    } else if (strcmp(command, "list_treasures") == 0) {
        fscanf(fp, "%s", hunt_id);
        list_treasures(hunt_id);
    } else if (strcmp(command, "view_treasure") == 0) {
        fscanf(fp, "%s %s", hunt_id, treasure_id);
        view_treasure(hunt_id, treasure_id);
    }

    fclose(fp);
}

int main() {
    FILE *pid_fp = fopen("treasure_hub.pid", "w");
    if (!pid_fp) {
        perror("Failed to create treasure_hub.pid");
        exit(EXIT_FAILURE);
    }
    fprintf(pid_fp, "%d\n", getpid());
    fclose(pid_fp);

    struct sigaction sa;
    sa.sa_handler = handle_sigusr1;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("Failed to set SIGUSR1 handler");
        exit(EXIT_FAILURE);
    }

    sa.sa_handler = handle_sigint;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Failed to set SIGINT handler");
        exit(EXIT_FAILURE);
    }

    printf("treasure_hub started with PID %d\n", getpid());

    while (1) {
        if (signal_received) {
            signal_received = 0;
            process_command();
        }
        pause();
    }

    return 0;
}