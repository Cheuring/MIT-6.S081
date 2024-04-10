#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void newProc(int p[2]){
    int prime;
    close(p[1]);
    if(read(p[0],&prime,4)!=4){
        fprintf(2,"failed to read\n");
        exit(1);
    }
    printf("prime %d\n",prime);
    int n;
    if(read(p[0],&n,4)){
        int newp[2];
        pipe(newp);
        if(fork()==0){
            newProc(newp);
        }else{
            close(newp[0]);
            if(n%prime) write(newp[1],&n,4);
            while(read(p[0],&n,4)){
                if(n%prime) write(newp[1],&n,4);;
            }
            close(p[0]);
            close(newp[1]);
            wait((int*)0);
        }
    }
    exit(0);
}

int main(int argc,char *argv[]){
    int p[2];
    pipe(p);
    if(fork()==0){
        newProc(p);
    }else{
        close(p[0]);
        for(int i=2;i<=35;++i){
            if(write(p[1],&i,4)!=4){
                fprintf(2,"failed to write\n");
                exit(1);
            }
        }
        close(p[1]);
        wait((int*)0);
        exit(0);
    }
    return 0;
}