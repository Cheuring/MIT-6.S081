#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/stat.h"
#include "user/user.h"

int getline(char* new_argv[MAXARG],int curr_argc){
    char buf[1024];
    int n=0;
    while(read(0,buf+n,1)){
        if(n==1023){
            fprintf(2,"argument is too long\n");
            exit(1);
        }
        if((buf[n]==' '||buf[n]=='\n')&& n>0){
            buf[n]='\0';
            new_argv[curr_argc++]=malloc(strlen(buf)+1);
            strcpy(new_argv[curr_argc-1],buf);
            if(curr_argc==MAXARG) break;
            n=-1;
        }
        ++n;
    }
    if(n>0){
        buf[n]='\0';
        new_argv[curr_argc++]=malloc(strlen(buf)+1);
        strcpy(new_argv[curr_argc-1],buf);
    }
    return curr_argc;
}

int main(int argc, char *argv[]){
    char *command = "echo";
    char* new_argv[MAXARG];
    int n=0;
    if(argc>1){
        command = malloc(strlen(argv[1])+1);
        strcpy(command,argv[1]);
        n=1;
    }
    new_argv[0]=malloc(strlen(command)+1);
    strcpy(new_argv[0],command);
    for(int i=2 ;i<argc ;++i){
        new_argv[i-1]=malloc(strlen(argv[i])+1);
        strcpy(new_argv[i-1],argv[i]);
    }
    int curr_argc=getline(new_argv,argc-n);
    new_argv[curr_argc]= 0;
    if(fork()==0){
        exec(command,new_argv);
        fprintf(2,"exec failed\n");
        exit(1);
    }else{
        wait(0);
        exit(0);
    }
    return 0;
}