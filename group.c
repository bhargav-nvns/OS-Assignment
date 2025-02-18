#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <sys/wait.h>

typedef struct {
    long mtype;
    int timestamp;
    int user;
    char mtext[256];
    int modifyingGroup;
} Message;

int compare_messages(const void *a, const void *b) {
    Message *msgA = (Message *)a;
    Message *msgB = (Message *)b;
    return (msgA->timestamp - msgB->timestamp);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <file_path>\n", argv[0]);
        exit(1);
    }
    char fp[200];
    snprintf(fp, sizeof(fp), "testcase_%s/", argv[1]);
    char input_fp[200];
    snprintf(input_fp, sizeof(input_fp), "testcase_%s/input.txt", argv[1]);
    char filepath[200];
    snprintf(filepath, sizeof(filepath), "testcase_%s/%s", argv[1], argv[2]);
    FILE *file = fopen(filepath, "r");
    if(file == NULL){
        perror("Error opening file");
        exit(1);
    }
    FILE *input = fopen(input_fp, "r");
    if(input == NULL){
        perror("Error opening file");
        exit(1);
    }
    int grp_no;
    int usr_no;
    sscanf(argv[2], "groups/group_%d.txt", &grp_no);
    if (file == NULL || input == NULL) {
        perror("Error opening file");
        exit(1);
    }
    key_t key_app, key_mod, key_val;
    fscanf(input, "%*d");
    fscanf(input, "%d", &key_val);
    fscanf(input, "%d", &key_app);
    fscanf(input, "%d", &key_mod);

    int msg_id_val = msgget(key_val, IPC_CREAT | 0666);
    int msg_id_app = msgget(key_app, IPC_CREAT | 0666);
    int msg_id_mod = msgget(key_mod, IPC_CREAT | 0666);
    if (msg_id_val == -1 || msg_id_app == -1 || msg_id_mod == -1) {
        printf("Error creating message queue\n");
        perror("msgget");
        exit(1);
    }else{
        printf("key_val: %d, key_app: %d, key_mod: %d\n", key_val, key_app, key_mod);
    }
    Message grp_creation;
    grp_creation.mtype = 1;
    grp_creation.modifyingGroup = grp_no;
    if (msgsnd(msg_id_val, &grp_creation, sizeof(Message) - sizeof(long), 0) == -1) {
        perror("msgsnd");
        exit(1);
    }
    int user_num;
    fscanf(file, "%d", &user_num);  // Get number of users first
    char user_file_path[user_num][200];
    for (int i = 0; i < user_num; i++) {
        if (fscanf(file, "%s", user_file_path[i]) != 1) {  
            perror("Error reading user file path");
            exit(1);
        }
    }
    printf("Number of users: %d\n", user_num);
    fclose(input);
    fclose(file);
    int total_messages =0;
    Message all_messages[5000];
    int messages_per_user[user_num];
    for (int i = 0; i < user_num; i++) {
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("pipe failed");
            exit(1);
        }
        sscanf(user_file_path[i], "users/user_%d_%d.txt", &grp_no, &usr_no);
        Message usr_creation;
        usr_creation.mtype = 2;
        usr_creation.modifyingGroup = grp_no;
        usr_creation.user = usr_no;
        if (msgsnd(msg_id_val, &usr_creation, sizeof(Message) - sizeof(long), 0) == -1) {
            perror("msgsnd");
            exit(1);
        }
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            close(pipefd[0]); // Close read end 
            char user_fp[200];
            if (strlen(fp) + strlen(user_file_path[i]) >= sizeof(user_fp)) {
                fprintf(stderr, "Path too long: %s%s\n", fp, user_file_path[i]);
                exit(1);
            }
            snprintf(user_fp, sizeof(user_fp), "%s%s", fp, user_file_path[i]);
            FILE *user_file = fopen(user_fp, "r");
            if (user_file == NULL) {
                perror("Error opening user file");
                exit(1);
            }
            int num_lines = 0;
            char line[200];
            while (fgets(line, sizeof(line), user_file) != NULL) {
                num_lines++;
            }
            fclose(user_file);
            int tot_lines = num_lines;
            int timestamps[tot_lines];
            char messages[tot_lines][256];
            user_file = fopen(user_fp, "r");
            if (user_file == NULL) {
                perror("Error reopening user file");
                exit(EXIT_FAILURE);
            }
            num_lines = 0;
            while (fgets(line, sizeof(line), user_file) != NULL) {
                sscanf(line, "%d %s", &timestamps[num_lines], messages[num_lines]);
                num_lines++;
            }
            fclose(user_file);
            messages_per_user[i] = num_lines;
            int group_num, user_no;
            sscanf(user_file_path[i], "users/user_%d_%d.txt", &group_num, &user_no);
            write(pipefd[1], &num_lines, sizeof(num_lines));
            for (int j = 0; j < num_lines; j++) {
                Message user_to_grp;
                user_to_grp.mtype = 30 + group_num;
                user_to_grp.timestamp = timestamps[j];
                user_to_grp.user = user_no;
                strcpy(user_to_grp.mtext, messages[j]);
                user_to_grp.modifyingGroup = group_num;
                write(pipefd[1], &user_to_grp, sizeof(Message));
            }
            close(pipefd[1]); // Close write end
            exit(0);
        } else {
            close(pipefd[1]); // Close write end
            int num_messages;
            read(pipefd[0], &num_messages, sizeof(num_messages));
            for (int j = 0; j < num_messages; j++) {
                read(pipefd[0], &all_messages[total_messages], sizeof(Message));
                total_messages++;
            }
            close(pipefd[0]); // Close read end
        }
    }
    for(int i=0;i<total_messages;i =i+10){
        printf("mtype: %ld, user: %d, group: %d, timestamp: %d, message: %s\n", all_messages[i].mtype, all_messages[i].user, all_messages[i].modifyingGroup, all_messages[i].timestamp, all_messages[i].mtext);
    }
    printf("Total messages: %d in group no %d\n", total_messages, grp_no);
    Message num_msg;
    num_msg.mtype = 10;
    num_msg.timestamp = 0;
    num_msg.user = total_messages;
    num_msg.modifyingGroup = grp_no;
    if (msgsnd(msg_id_mod, &num_msg, sizeof(Message) - sizeof(long), 0) == -1) {
        perror("msgsnd");
        exit(1);
    }
    int msg_sent_to_mod = 0;
    for (int j = 0; j < total_messages; j++) {
        printf("Debug: Sending message with mtype: %ld, user: %d, group: %d, timestamp: %d, message: %s\n", all_messages[j].mtype, all_messages[j].user, all_messages[j].modifyingGroup, all_messages[j].timestamp, all_messages[j].mtext);
        if (msgsnd(msg_id_mod, &all_messages[j], sizeof(Message) - sizeof(long), 0) == -1) {
            perror("msgsnd");
            exit(1);
        }else{
            msg_sent_to_mod++;
            printf("message sent to mod form group %d user %d\n",all_messages[j].modifyingGroup, all_messages[j].user);
        }
    }
    printf("no of messages sent %d from grp no %d\n", msg_sent_to_mod, grp_no);
    Message to_validation[total_messages];
    Message temp;
    Message recived_msg[total_messages];
    int num_recived = 0;
    int valid_messages = 0;
    int user_active = user_num;
    while(num_recived < total_messages) {
        if (msgrcv(msg_id_mod, &temp, sizeof(Message) - sizeof(long), 100+grp_no, 0) == -1) {
            printf("error in line 197\n");
            perror("msgrcv");
            exit(1);
        }else{
            temp.mtype = temp.mtype-70;
            recived_msg[num_recived] = temp;
            num_recived++;
        }
    }
    printf("no of messages recived %d from grp no %d\n", num_recived, grp_no);
    qsort(recived_msg, num_recived, sizeof(Message), compare_messages);
    int msg_rcv_per_usr[user_num];
    for (int i = 0; i < user_num; i++) {
        msg_rcv_per_usr[i] = 0;
    }
    int no_usr_removed = 0;
    for(int i=0;i<total_messages;i++){
        if(recived_msg[i].timestamp < 0){
            user_active--;
            recived_msg[i].timestamp = 0 - recived_msg[i].timestamp;
            to_validation[valid_messages] = recived_msg[i];
            valid_messages++;
            no_usr_removed++;
            if(user_active <2){
                break; // delete group
            }
        }else if(recived_msg[i].timestamp == 0){
            continue;
        }else{
            to_validation[valid_messages] = recived_msg[i];
            valid_messages++;
            int u_no = recived_msg[i].user;
            msg_rcv_per_usr[u_no]++;
            if(msg_rcv_per_usr[u_no] == messages_per_user[u_no]){
                user_active--;
                if(user_active <2){
                    break; // delete group
                }
            }
        }
    }
    for (int i = 0; i < valid_messages; i++) {
        if (msgsnd(msg_id_val, &to_validation[i], sizeof(Message) - sizeof(long), 0) == -1) {
            perror("msgsnd");
            exit(1);
        }
    }
    if(user_active < 2){
        Message delete_grp;
        delete_grp.mtype = 3;
        delete_grp.timestamp = 0;
        delete_grp.user = no_usr_removed;
        delete_grp.modifyingGroup = grp_no;
        if (msgsnd(msg_id_val, &delete_grp, sizeof(Message) - sizeof(long), 0) == -1) {
            perror("msgsnd");
            exit(1);
        }
        if (msgsnd(msg_id_app, &delete_grp, sizeof(Message) - sizeof(long), 0) == -1) {
            perror("msgsnd");
            exit(1);
        }
    }
    return 0;
}