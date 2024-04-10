#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc ,char*argv[]){
    int p[2];
    char buf[1];
    pipe(p);
    if(fork()==0){
        if(read(p[0],buf,sizeof(buf))>0){
            printf("%d: received ping\n",getpid());
        }else{
            fprintf(2,"failed to read\n");
            exit(1);
        }
        close(p[0]);
        write(p[1],buf,1);
        close(p[1]);
        exit(0);
    }else{
        write(p[1],buf,1);
        close(p[1]);
        if(read(p[0],buf,sizeof(buf))>0){
            printf("%d: received pong\n",getpid());
        }else{
            fprintf(2,"failed to read\n");
            exit(1);
        }
        close(p[0]);
        wait((int*)0);
        exit(0);
    }
}