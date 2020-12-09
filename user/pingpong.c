#include "kernel/types.h"
#include "user/user.h"

int 
main(int argc, char** argv ){
    int pid;
    int parent_fd[2];
    int child_fd[2];
    char buf[20];
    //为父子进程建立管道
    pipe(child_fd); 
    pipe(parent_fd);

    // Child Progress
    if((pid = fork()) == 0){
        close(parent_fd[1]);
        read(parent_fd[0],buf, 4);
        printf("%d: received %s\n",getpid(), buf);
        close(child_fd[0]);
        write(child_fd[1], "pong", sizeof(buf));
        exit();
    }
    // Parent Progress
    else{
        close(parent_fd[0]);
        write(parent_fd[1], "ping",4);
        close(child_fd[1]);
        read(child_fd[0], buf, sizeof(buf));
        printf("%d: received %s\n", getpid(), buf);
        exit();
    }
    
}