#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
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

void create_hunt_directory(char *hunt_id) {
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


void add_treasure(char *hunt_id) {
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

void list_treasures(char *hunt_id) {
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

    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "list %s", hunt_id);
    log_operation(hunt_id, log_msg);
}

void view_treasure(char *hunt_id, char *treasure_id) {
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

    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "view %s %s", hunt_id, treasure_id);
    log_operation(hunt_id, log_msg);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <operation> <hunt_id> [treasure_id]\n", argv[0]);
        printf("Operations: add, list, view, remove_treasure, remove_hunt\n");
        exit(EXIT_FAILURE);
    }

    char *operation = argv[1];
    char *hunt_id = argv[2];

    if (strcmp(operation, "add") == 0) {
        add_treasure(hunt_id);
    } else if (strcmp(operation, "list") == 0) {
        list_treasures(hunt_id);
    } else if (strcmp(operation, "view") == 0) {
        if (argc != 4) {
            printf("Usage: %s view <hunt_id> <treasure_id>\n", argv[0]);
            exit(EXIT_FAILURE);
        }
        view_treasure(hunt_id, argv[3]);
    } else if (strcmp(operation, "remove_treasure") == 0) {
        if (argc != 4) {
            printf("Usage: %s remove_treasure <hunt_id> <treasure_id>\n", argv[0]);
            exit(EXIT_FAILURE);
        }
        remove_treasure(hunt_id, argv[3]);
    } else if (strcmp(operation, "remove_hunt") == 0) {
        remove_hunt(hunt_id);
    } else {
        printf("Unknown operation: %s\n", operation);
        exit(EXIT_FAILURE);
    }
    return 0;
}