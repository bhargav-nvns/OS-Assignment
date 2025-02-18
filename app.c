#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>

#define MAX_GRP 30        // Maximum number of groups
#define MAX_LEN 256       // Maximum length for file paths

typedef struct {
    long mtype;
    int timestamp;
    int user;
    char mtext[256];
    int modifyingGroup;
} Message;
int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <testcase_number>\n", argv[0]);
        exit(1);
    }
    Message rec_msg;     
    FILE *inpf;
    int ngrp;
    int k_app;
    int mqid;
    int fg = 0;
    char grp_fp[MAX_GRP][MAX_LEN];   
    pid_t grppid[MAX_GRP];          
    char input_filepath[1000];    

    snprintf(input_filepath, sizeof(input_filepath), "testcase_%s/input.txt", argv[1]);
    inpf = fopen(input_filepath, "r");
    if (inpf == NULL) {
        perror("Error opening input.txt");
        exit(1);
    }

    if (fscanf(inpf, "%d", &ngrp) != 1) {
        fprintf(stderr, "Error reading number of groups from input.txt\n");
        fclose(inpf);
        exit(1);
    }

    fscanf(inpf, "%*d");            
    if (fscanf(inpf, "%d", &k_app) != 1) {
        fprintf(stderr, "Error reading k_app\n");
        fclose(inpf);
        exit(1);
    }
    fscanf(inpf, "%*d");            
    fscanf(inpf, "%*d");            

    for (int i = 0; i < ngrp; i++) { 
        if (fscanf(inpf, "%s", grp_fp[i]) != 1) {
            fprintf(stderr, "Error reading group file paths\n");
            fclose(inpf);
            exit(1);
        }
    }
    fclose(inpf);       
    mqid = msgget(k_app, 0666);           
    if(mqid != -1){
        msgctl(mqid, IPC_RMID, NULL);
    }
    mqid = msgget(k_app, IPC_CREAT | 0666);
    if (mqid == -1) {
        perror("msgget failed for k_app");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < ngrp; i++) {  
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        }
        if (pid == 0) {              
            execl("./group", "./group", argv[1], grp_fp[i], (char*)NULL);
            perror("execl failed for group process");
            exit(1);
        } else {                       
            grppid[i] = pid;
        }
    }
    while (fg < ngrp) {             
        if (msgrcv(mqid, &rec_msg, sizeof(rec_msg) - sizeof(long), 3, 0) == -1) {
            continue;
        }else{
            printf("All users terminated. Exiting group process %d.\n", rec_msg.modifyingGroup);
            fg++; 
        } 
    }
    int status;
    for (int i = 0; i < ngrp; i++) {  
        if (waitpid(grppid[i], &status, 0) == -1) {
            perror("waitpid failed");
        }
    }
    return 0;
}
