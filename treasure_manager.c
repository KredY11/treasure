#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>

#define MAX_CLUE 100
#define MAX_USER 50
#define RECORD_SIZE (sizeof(Treasure))

typedef struct {
    char id[20];
    char user[MAX_USER];
    float latitude;
    float longitude;
    char clue[MAX_CLUE];
    int value;
} Treasure;

void usage() {
    printf("Usage: treasure_manager <operation> <hunt_id> [args]\n");
    exit(1);
}

char* get_hunt_path(const char* hunt_id) {
    static char path[100];
    snprintf(path, sizeof(path), "hunts/%s", hunt_id);
    return path;
}

char* get_file_path(const char* hunt_id) {
    static char path[120];
    snprintf(path, sizeof(path), "hunts/%s/data.bin", hunt_id);
    return path;
}

void create_hunt(const char* hunt_id) {
    char* path = get_hunt_path(hunt_id);
    mkdir("hunts", 0777);
    mkdir(path, 0777);
    char link_name[100];
    snprintf(link_name, sizeof(link_name), "logged_hunt-%s", hunt_id);
    char log_path[120];
    snprintf(log_path, sizeof(log_path), "%s/logged_hunt", path);
    creat(log_path, 0644);
    symlink(log_path, link_name);
}

void log_action(const char* hunt_id, const char* action) {
    char path[120];
    snprintf(path, sizeof(path), "hunts/%s/logged_hunt", hunt_id);
    int fd = open(path, O_WRONLY | O_APPEND);
    dprintf(fd, "%s\n", action);
    close(fd);
}

void add_treasure(const char* hunt_id, Treasure t) {
    create_hunt(hunt_id);
    char* file_path = get_file_path(hunt_id);
    int fd = open(file_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    write(fd, &t, sizeof(Treasure));
    close(fd);
    log_action(hunt_id, "Added treasure");
}

void list_treasures(const char* hunt_id) {
    char* path = get_file_path(hunt_id);
    struct stat st;
    stat(path, &st);
    printf("Hunt ID: %s\nSize: %ld\nModified: %ld\n\n", hunt_id, st.st_size, st.st_mtime);

    int fd = open(path, O_RDONLY);
    Treasure t;
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        printf("ID: %s, User: %s, GPS: %.2f %.2f, Clue: %s, Value: %d\n",
               t.id, t.user, t.latitude, t.longitude, t.clue, t.value);
    }
    close(fd);
    log_action(hunt_id, "Listed treasures");
}

void view_treasure(const char* hunt_id, const char* tid) {
    int fd = open(get_file_path(hunt_id), O_RDONLY);
    Treasure t;
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        if (strcmp(t.id, tid) == 0) {
            printf("Treasure ID: %s\nUser: %s\nGPS: %.2f %.2f\nClue: %s\nValue: %d\n",
                   t.id, t.user, t.latitude, t.longitude, t.clue, t.value);
            break;
        }
    }
    close(fd);
    log_action(hunt_id, "Viewed treasure");
}

void remove_treasure(const char* hunt_id, const char* tid) {
    char* path = get_file_path(hunt_id);
    int fd = open(path, O_RDONLY);
    int temp_fd = open("temp.bin", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    Treasure t;

    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        if (strcmp(t.id, tid) != 0)
            write(temp_fd, &t, sizeof(Treasure));
    }
    close(fd);
    close(temp_fd);
    rename("temp.bin", path);
    log_action(hunt_id, "Removed treasure");
}

void remove_hunt(const char* hunt_id) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "rm -rf hunts/%s", hunt_id);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "rm -f logged_hunt-%s", hunt_id);
    system(cmd);
}

int main(int argc, char* argv[]) {
    if (argc < 3) usage();

    const char* cmd = argv[1];
    const char* hunt_id = argv[2];

    if (strcmp(cmd, "add") == 0 && argc == 8) {
        Treasure t;
        strncpy(t.id, argv[3], sizeof(t.id));
        strncpy(t.user, argv[4], sizeof(t.user));
        t.latitude = atof(argv[5]);
        t.longitude = atof(argv[6]);
        strncpy(t.clue, argv[7], sizeof(t.clue));
        t.value = rand() % 100; // Random value for now
        add_treasure(hunt_id, t);
    } else if (strcmp(cmd, "list") == 0) {
        list_treasures(hunt_id);
    } else if (strcmp(cmd, "view") == 0 && argc == 4) {
        view_treasure(hunt_id, argv[3]);
    } else if (strcmp(cmd, "remove_treasure") == 0 && argc == 4) {
        remove_treasure(hunt_id, argv[3]);
    } else if (strcmp(cmd, "remove_hunt") == 0) {
        remove_hunt(hunt_id);
    } else {
        usage();
    }
    return 0;
}
