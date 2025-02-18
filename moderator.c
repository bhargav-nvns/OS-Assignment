#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <unistd.h>

#define MAX_FILTERED_WORDS 50
#define MAX_WORD_LENGTH 20
#define MAX_GROUPS 30
#define MAX_USERS 50

typedef struct {
    long mtype;
    int timestamp;
    int user;
    char mtext[256];
    int modifyingGroup;
} Message;

// Global arrays to track violations and banned users
int violations[MAX_GROUPS][MAX_USERS] = { {0} };
int removed[MAX_GROUPS][MAX_USERS] = { {0} };

// Helper function to convert a string to lowercase
void to_lowercase(char *str) {
    for (; *str; str++) {
        *str = tolower(*str);
    }
}

// Function to count violations in a message based on filtered words
int count_violations(char *message, char filtered_words[MAX_FILTERED_WORDS][MAX_WORD_LENGTH], int word_count) {
    int violation_count = 0;
    for (int i = 0; i < word_count; i++) {
        if (strstr(message, filtered_words[i]) != NULL) {
            violation_count++;
        }
    }
    return violation_count;
}

// Function to check if the message queue is empty
int is_queue_empty(int msgid) {
    struct msqid_ds buf;

    // Get the status of the message queue
    if (msgctl(msgid, IPC_STAT, &buf) == -1) {
        perror("Error retrieving message queue status");
        return -1; // Return -1 in case of an error
    }

    // Check if the queue is empty
    return buf.msg_qnum == 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Provide testcase number\n");
        exit(1);
    }

    // Initialize total number of messages
    int total_no_of_messages = 0;

    // Read filtered words from filtered_words.txt
    char filteredpath[1000];
    sprintf(filteredpath, "testcase_%s/filtered_words.txt", argv[1]);

    FILE *fp_filtered = fopen(filteredpath, "r");
    if (fp_filtered == NULL) {
        perror("Error opening filtered_words.txt");
        exit(1);
    }

    char filtered_words[MAX_FILTERED_WORDS][MAX_WORD_LENGTH];
    int word_count = 0;

    while (word_count < MAX_FILTERED_WORDS && fscanf(fp_filtered, "%20s", filtered_words[word_count]) == 1) {
        to_lowercase(filtered_words[word_count]);
        word_count++;
    }
    fclose(fp_filtered);

    // Read input file for keys and thresholds
    char filepath[1000];
    sprintf(filepath, "testcase_%s/input.txt", argv[1]);

    FILE *fp_input = fopen(filepath, "r");
    if (fp_input == NULL) {
        perror("Error opening input.txt");
        exit(1);
    }

    int no_groups, gv_key, av_key, moderator_key, violation_threshold;

    if (fscanf(fp_input, "%d %d %d %d %d", &no_groups, &gv_key, &av_key, &moderator_key, &violation_threshold) != 5) {
        perror("Error reading input.txt");
        fclose(fp_input);
        exit(1);
    }
    
    fclose(fp_input);

    // Connect or create the message queue for moderator
    int msgid = msgget(moderator_key, IPC_CREAT | 0666);
    if (msgid == -1) {
        perror("Error creating message queue");
        exit(1);
    }

    while (1) {
        Message msg;

        // Receive messages from groups.c
        if (msgrcv(msgid, &msg, sizeof(msg) - sizeof(msg.mtype), 0, 0) == -1) {
            if (errno == EIDRM || errno == ENOMSG) {
                break; // Exit when the queue is deleted or no more messages
            }
            perror("Error receiving message");
            continue;
        }

        if (msg.mtype == 10) {
            // Update total_no_of_messages when mtype=10
            total_no_of_messages += msg.user;
            printf("Total messages updated to: %d for group: %d\n", total_no_of_messages, msg.modifyingGroup);
            continue;
        } else if (msg.mtype >= MAX_GROUPS + 1 && msg.mtype <= MAX_GROUPS + no_groups) {
            // Handle messages from users in specific groups
            total_no_of_messages--;
            printf("Total messages remaining: %d\n", total_no_of_messages);
            int group = msg.modifyingGroup;
            int user = msg.user;

            if (removed[group][user] == 1) {
                // User is already banned
                msg.timestamp = 0;
                msgsnd(msgid, &msg, sizeof(msg) - sizeof(msg.mtype), 0);
                continue;
            }

            // Count violations in the message text
            int violation_count = count_violations(msg.mtext, filtered_words, word_count);
            violations[group][user] += violation_count;

            if (violations[group][user] >= violation_threshold) {
                // User gets banned due to this message
                printf("User %d from group %d has been removed due to %d violations.\n", user, group, violations[group][user]);
                removed[group][user] = 1;

                msg.timestamp = -msg.timestamp; // Indicate that this message should still be sent back
                msgsnd(msgid, &msg, sizeof(msg) - sizeof(msg.mtype), 0);
            } else {
                // User is not banned yet
                msgsnd(msgid, &msg, sizeof(msg) - sizeof(msg.mtype), 0);
            }
            msg.mtype = 15;
        }

        // Check both conditions before deleting the queue
        if (total_no_of_messages <= 0 && is_queue_empty(msgid)) {
            printf("All messages processed and queue is empty. Deleting message queue and exiting.\n");
            msgctl(msgid, IPC_RMID, NULL);
            break;
        }
    }

    return 0;
}

