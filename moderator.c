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

void to_lowercase(char *str) {
    for (; *str; str++) {
        *str = tolower(*str);
    }
}

int count_violations(char *message, char filtered_words[MAX_FILTERED_WORDS][MAX_WORD_LENGTH], int word_count) {
    int violation_count = 0;
    char *str = message;
    to_lowercase(str);
    for (int i = 0; i < word_count; i++) {
        if (strstr(str, filtered_words[i]) != NULL) {
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
    int violations[MAX_GROUPS][MAX_USERS] = { {0} };
    int removed[MAX_GROUPS][MAX_USERS] = { {0} };
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
    printf("No of groups: %d, GV key: %d, AV key: %d, Moderator key: %d, Violation threshold: %d\n", no_groups, gv_key, av_key, moderator_key, violation_threshold);
    // Connect or create the message queue for moderator
    // int msgid = msgget(moderator_key, 0666);
    // if (msgid != -1) {
    //     // Delete the existing message queue
    //     msgctl(msgid, IPC_RMID, NULL);
    // }
    int
    msgid = msgget(moderator_key, IPC_CREAT | 0666);
    if (msgid == -1) {
        perror("Error creating message queue");
        exit(1);
    }
    struct msqid_ds buff_mod;

    Message all_messages[5000];
    int msg_rcv = 0;
    int grp_rcv =0;
    Message temp;
    total_no_of_messages = 0;
    int percentage_filled_mod;
    printf("Total messages updated to: %d\n", total_no_of_messages);
    unsigned long num_bytes_mod;
    Message temp2;
    while(msg_rcv<total_no_of_messages || grp_rcv < no_groups){
        if(msgctl(msgid,IPC_STAT,&buff_mod) == -1){
            perror("msgctl");
            exit(1);
        }
        num_bytes_mod = buff_mod.msg_qnum*sizeof(Message);
        percentage_filled_mod = (100*num_bytes_mod)/buff_mod.msg_qbytes;
        //printf("num_bytes_mod: %lu\n", num_bytes_mod);
        if(msgrcv(msgid, &temp, sizeof(Message) - sizeof(long), 0, 0) == -1){
            perror("Error receiving message");
            continue;
        }else{
            if(temp.mtype ==10){
                total_no_of_messages += temp.user;
                grp_rcv++;
                continue;
            }

            if(temp.mtype >= (MAX_GROUPS) && temp.mtype <= (MAX_GROUPS + MAX_GROUPS)){
                int group = temp.modifyingGroup;
                int user = temp.user;
                
                printf("Message received from user %d in group %d-- Rem Messages %d time: %d message %s\n", user, group, total_no_of_messages - msg_rcv, temp.timestamp, temp.mtext);
                if(removed[group][user] == 1){
                    temp.timestamp = 0;
                    all_messages[msg_rcv] = temp;
                    msg_rcv++;
                    //printf("User %d from group %d has been banned.\n", user, group);
                    continue;
                }
                temp2 = temp;
                int violation_count = count_violations(temp2.mtext, filtered_words, word_count);
                violations[group][user] += violation_count;
                if(violations[group][user] >= violation_threshold){
                    printf("User %d from group %d has been removed due to %d violations at %ld\n", user, group, violations[group][user], temp.timestamp);
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
    printf("out of while");
    int tot_msg_rcv = msg_rcv;

    while(1){
        if(tot_msg_rcv <= 0){
            printf("All messages processed and exiting.\n");
            //msgctl(msgid, IPC_RMID, NULL);
            break;
        }else{
            all_messages[msg_rcv - tot_msg_rcv].mtype = 100 + all_messages[msg_rcv - tot_msg_rcv].modifyingGroup;
            if (msgsnd(msgid, &all_messages[msg_rcv - tot_msg_rcv], sizeof(Message) - sizeof(long), 0) == -1) {
                perror("msgsnd");
                exit(1);
            }else{
                
                printf("Message sent back to group %d user %d remainding : %d time %d message: %s \n", all_messages[msg_rcv - tot_msg_rcv].modifyingGroup, all_messages[msg_rcv - tot_msg_rcv].user, tot_msg_rcv,all_messages[msg_rcv - tot_msg_rcv].timestamp, all_messages[msg_rcv - tot_msg_rcv].mtext);
                tot_msg_rcv--;
            }
        }

    }


    return 0;
}

