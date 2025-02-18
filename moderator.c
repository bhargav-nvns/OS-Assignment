#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
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
    int msgid = msgget(moderator_key, 0666);
    if (msgid != -1) {
        // Delete the existing message queue
        msgctl(msgid, IPC_RMID, NULL);
    }
    msgid = msgget(moderator_key, IPC_CREAT | 0666);
    if (msgid == -1) {
        perror("Error creating message queue");
        exit(1);
    }
    Message all_messages[5000];
    int msg_rcv = 0;
    int grp_rcv =0;
    Message temp;
    while(grp_rcv<no_groups){
        if(msgrcv(msgid, &temp, sizeof(Message) - sizeof(long), 10, 0) == -1){
            perror("Error receiving message");
            continue;
        }else{
            total_no_of_messages += temp.user;
            grp_rcv++;
        }
    }
    printf("Total messages updated to: %d\n", total_no_of_messages);
    while(msg_rcv<total_no_of_messages){
        if(msgrcv(msgid, &temp, sizeof(Message) - sizeof(long), 0, 0) == -1){
            perror("Error receiving message");
            continue;
        }else{
            if(temp.mtype >= (MAX_GROUPS + 1) && temp.mtype <= (MAX_GROUPS + no_groups)){
                int group = temp.modifyingGroup;
                int user = temp.user;
                printf("Message received from user %d in group %d\n", user, group);
                if(removed[group][user] == 1){
                    temp.timestamp = 0;
                    all_messages[msg_rcv] = temp;
                    msg_rcv++;
                    printf("User %d from group %d has been removed due to %d violations.\n", user, group, violations[group][user]);
                    continue;
                }
                int violation_count = count_violations(temp.mtext, filtered_words, word_count);
                violations[group][user] += violation_count;
                if(violations[group][user] >= violation_threshold){
                    printf("User %d from group %d has been removed due to %d violations.\n", user, group, violations[group][user]);
                    removed[group][user] = 1;
                    temp.timestamp = -temp.timestamp;
                    all_messages[msg_rcv] = temp;
                    msg_rcv++;
                }else{
                    all_messages[msg_rcv] = temp;
                    msg_rcv++;
                }
            }
        }
    }
    
    int tot_msg_rcv = msg_rcv;

    while(1){
        if(tot_msg_rcv <= 0){
            printf("All messages processed and queue is empty. Deleting message queue and exiting.\n");
            msgctl(msgid, IPC_RMID, NULL);
            break;
        }else{
            all_messages[msg_rcv - tot_msg_rcv].mtype = 100 + all_messages[msg_rcv - tot_msg_rcv].modifyingGroup;
            if (msgsnd(msgid, &all_messages[msg_rcv - tot_msg_rcv], sizeof(Message) - sizeof(long), 0) == -1) {
                perror("msgsnd");
                exit(1);
            }else{
                tot_msg_rcv--;
            }
        }

    }


    return 0;
}

