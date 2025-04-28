#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>

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

pid_t hub_pid = 0;

void start_treasure_hub() {
    FILE *pid_fp = fopen("treasure_hub.pid", "r");
    if (pid_fp) {
        if (fscanf(pid_fp, "%d", &hub_pid) == 1) {
            if (kill(hub_pid, 0) == 0) {
                fclose(pid_fp);
                return;
            }
        }
        fclose(pid_fp);
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("Failed to fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        execl("./treasure_hub", "treasure_hub", (char *)NULL);
        perror("Failed to start treasure_hub");
        exit(EXIT_FAILURE);
    }

    hub_pid = pid;
    sleep(1);

    pid_fp = fopen("treasure_hub.pid", "r");
    if (pid_fp && fscanf(pid_fp, "%d", &hub_pid) != 1) {
        hub_pid = 0;
    }
    if (pid_fp) fclose(pid_fp);
}

void send_command(const char *command) {
    start_treasure_hub();

    if (hub_pid == 0 || kill(hub_pid, 0) != 0) {
        fprintf(stderr, "Error: treasure_hub is not running\n");
        hub_pid = 0;
        unlink("treasure_hub.pid");
        return;
    }

    FILE *fp = fopen("command.txt", "w");
    if (!fp) {
        perror("Failed to open command.txt");
        return;
    }
    fprintf(fp, "%s", command);
    fclose(fp);

    if (kill(hub_pid, SIGUSR1) == -1) {
        perror("Failed to send SIGUSR1");
        hub_pid = 0;
        unlink("treasure_hub.pid");
    }
}

void create_hunt_directory(const char *hunt_id) {
    char hunt_dir[256];
    snprintf(hunt_dir, sizeof(hunt_dir), "Hunt%s", hunt_id);

    if (mkdir(hunt_dir, 0755) == -1 && errno != EEXIST) {
        perror("Failed to create hunt directory");
        exit(EXIT_FAILURE);
    }

    char treasures_path[512];
    snprintf(treasures_path, sizeof(treasures_path), "%s/treasures.bin", hunt_dir);
    int fd = open(treasures_path, O_CREAT | O_RDWR, 0644);
    if (fd == -1) {
        perror("Failed to create treasures.bin");
        exit(EXIT_FAILURE);
    }
    close(fd);

    char log_path[512];
    snprintf(log_path, sizeof(log_path), "%s/logged_hunt", hunt_dir);
    fd = open(log_path, O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (fd == -1) {
        perror("Failed to create logged_hunt");
        exit(EXIT_FAILURE);
    }
    close(fd);

    char symlink_name[256];
    snprintf(symlink_name, sizeof(symlink_name), "logged_hunt-%s", hunt_id);
    if (symlink(log_path, symlink_name) == -1 && errno != EEXIST) {
        perror("Failed to create symbolic link");
        exit(EXIT_FAILURE);
    }
}

void log_operation(const char *hunt_id, const char *operation) {
    char log_path[512];
    snprintf(log_path, sizeof(log_path), "Hunt%s/logged_hunt", hunt_id);

    int fd = open(log_path, O_WRONLY | O_APPEND);
    if (fd == -1) {
        perror("Failed to open log file");
        return;
    }

    char log_entry[512];
    time_t now = time(NULL);
    snprintf(log_entry, sizeof(log_entry), "[%s] %s\n", ctime(&now), operation);
    write(fd, log_entry, strlen(log_entry));
    close(fd);
}

void add_treasure(const char *hunt_id) {
    create_hunt_directory(hunt_id);

    char treasures_path[512];
    snprintf(treasures_path, sizeof(treasures_path), "Hunt%s/treasures.bin", hunt_id);

    int fd = open(treasures_path, O_RDWR);
    if (fd == -1) {
        perror("Failed to open treasures.bin");
        exit(EXIT_FAILURE);
    }

    Treasure t;
    printf("Enter Treasure ID: ");
    fgets(t.treasure_id, ID_SIZE, stdin);
    t.treasure_id[strcspn(t.treasure_id, "\n")] = 0;
    printf("Enter User Name: ");
    fgets(t.user_name, NAME_SIZE, stdin);
    t.user_name[strcspn(t.user_name, "\n")] = 0;
    printf("Enter Latitude: ");
    scanf("%f", &t.latitude);
    printf("Enter Longitude: ");
    scanf("%f", &t.longitude);
    getchar();
    printf("Enter Clue: ");
    fgets(t.clue, CLUE_SIZE, stdin);
    t.clue[strcspn(t.clue, "\n")] = 0;
    printf("Enter Value: ");
    scanf("%d", &t.value);
    getchar();

    Treasure temp;
    off_t offset = 0;
    while (read(fd, &temp, RECORD_SIZE) == RECORD_SIZE) {
        if (strncmp(temp.treasure_id, t.treasure_id, ID_SIZE) == 0) {
            printf("Error: Treasure ID must be unique.\n");
            close(fd);
            return;
        }
        if (strncmp(temp.user_name, t.user_name, NAME_SIZE) == 0) {
            printf("Error: User Name must be unique.\n");
            close(fd);
            return;
        }
        offset += RECORD_SIZE;
    }

    lseek(fd, 0, SEEK_END);
    write(fd, &t, RECORD_SIZE);
    close(fd);

    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "add %s: Added treasure %s", hunt_id, t.treasure_id);
    log_operation(hunt_id, log_msg);
}

void list_treasures(const char *hunt_id) {
    char command[256];
    snprintf(command, sizeof(command), "list_treasures %s\n", hunt_id);
    send_command(command);

    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "list %s", hunt_id);
    log_operation(hunt_id, log_msg);
}

void view_treasure(const char *hunt_id, const char *treasure_id) {
    char command[256];
    snprintf(command, sizeof(command), "view_treasure %s %s\n", hunt_id, treasure_id);
    send_command(command);

    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "view %s %s", hunt_id, treasure_id);
    log_operation(hunt_id, log_msg);
}

void remove_treasure(const char *hunt_id, const char *treasure_id) {
    char treasures_path[512];
    snprintf(treasures_path, sizeof(treasures_path), "Hunt%s/treasures.bin", hunt_id);

    int fd = open(treasures_path, O_RDWR);
    if (fd == -1) {
        perror("Failed to open treasures.bin");
        return;
    }

    char temp_path[512];
    snprintf(temp_path, sizeof(temp_path), "Hunt%s/temp.bin", hunt_id);
    int temp_fd = open(temp_path, O_CREAT | O_RDWR, 0644);
    if (temp_fd == -1) {
        perror("Failed to create temp file");
        close(fd);
        return;
    }

    Treasure t;
    int found = 0;
    while (read(fd, &t, RECORD_SIZE) == RECORD_SIZE) {
        if (strncmp(t.treasure_id, treasure_id, ID_SIZE) != 0) {
            write(temp_fd, &t, RECORD_SIZE);
        } else {
            found = 1;
        }
    }
    close(fd);
    close(temp_fd);

    if (!found) {
        printf("Treasure %s not found in hunt %s.\n", treasure_id, hunt_id);
        unlink(temp_path);
        return;
    }

    unlink(treasures_path);
    rename(temp_path, treasures_path);

    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "remove_treasure %s %s", hunt_id, treasure_id);
    log_operation(hunt_id, log_msg);
}

void remove_hunt(const char *hunt_id) {
    char hunt_dir[256];
    snprintf(hunt_dir, sizeof(hunt_dir), "Hunt%s", hunt_id);

    char treasures_path[512];
    snprintf(treasures_path, sizeof(treasures_path), "%s/treasures.bin", hunt_dir);
    unlink(treasures_path);

    char log_path[512];
    snprintf(log_path, sizeof(log_path), "%s/logged_hunt", hunt_dir);
    unlink(log_path);

    rmdir(hunt_dir);

    char symlink_name[256];
    snprintf(symlink_name, sizeof(symlink_name), "logged_hunt-%s", hunt_id);
    unlink(symlink_name);

    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "remove_hunt %s", hunt_id);
    log_operation(hunt_id, log_msg);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <operation> <hunt_id> [treasure_id]\n", argv[0]);
        printf("Operations: add, list, view, remove_treasure, remove_hunt\n");
        return EXIT_FAILURE;
    }

    const char *operation = argv[1];
    const char *hunt_id = argv[2];

    if (strcmp(operation, "add") == 0) {
        add_treasure(hunt_id);
    } else if (strcmp(operation, "list") == 0) {
        list_treasures(hunt_id);
    } else if (strcmp(operation, "view") == 0) {
        if (argc != 4) {
            printf("Usage: %s view <hunt_id> <treasure_id>\n", argv[0]);
            return EXIT_FAILURE;
        }
        view_treasure(hunt_id, argv[3]);
    } else if (strcmp(operation, "remove_treasure") == 0) {
        if (argc != 4) {
            printf("Usage: %s remove_treasure <hunt_id> <treasure_id>\n", argv[0]);
            return EXIT_FAILURE;
        }
        remove_treasure(hunt_id, argv[3]);
    } else if (strcmp(operation, "remove_hunt") == 0) {
        remove_hunt(hunt_id);
    } else {
        printf("Unknown operation: %s\n", operation);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}