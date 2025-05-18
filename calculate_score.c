#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

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

int main() {
    int input_fd = 0; // Read from stdin (pipe)
    Treasure t;
    int scores[MAX_USER][2]; // [user_index][0] = score, [user_index][1] = user_id length
    char users[MAX_USER][MAX_USER];
    int user_count = 0;

    // Initialize scores
    for (int i = 0; i < MAX_USER; i++) {
        scores[i][0] = 0;
        scores[i][1] = 0;
    }

    // Read treasures from pipe
    while (read(input_fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        int user_found = 0;
        for (int i = 0; i < user_count; i++) {
            if (strcmp(users[i], t.user) == 0) {
                scores[i][0] += t.value;
                user_found = 1;
                break;
            }
        }
        if (!user_found && user_count < MAX_USER) {
            strcpy(users[user_count], t.user);
            scores[user_count][0] = t.value;
            scores[user_count][1] = strlen(t.user);
            user_count++;
        }
    }

    // Send scores back through stdout (pipe)
    for (int i = 0; i < user_count; i++) {
        char output[100];
        snprintf(output, sizeof(output), "%s:%d\n", users[i], scores[i][0]);
        write(1, output, strlen(output));
    }

    return 0;
}