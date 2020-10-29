#include "kernel/types.h"
#include "kernel/stat.h"
#include <sys/types.h> 
#include "user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

#define CMDSTYLE        2

char *cutoffinput(char *buf);
void substring(char s[], char sub[], int pos, int len);
void printargs(char* args[MAXARG][CMDSTYLE], int args_num);

/* 打印参数 */
void 
printargs(char* args[MAXARG][CMDSTYLE], int args_num){
    for (size_t i = 0; i < args_num; i++)
    {
        /* code */
        printf("--------------args %d:--------------\n", i + 1);
        printf("cmd: %s, arg: %s, argslen: %d \n", args[i][0], args[i][1], strlen(args[i][1]));
    }
    
}
/* 子串 */
void 
substring(char s[], char *sub, int pos, int len) {
   int c = 0;   
   while (c < len) {
      *(sub + c) = s[pos+c];
      c++;
   }
   *(sub + c) = '\0';
}

/* 截断 '\n' */
char* 
cutoffinput(char *buf){
    /* 记得要为char *新分配一片地址空间，否则编译器默认指向同一片地址 */
    if(strlen(buf) > 1 && buf[strlen(buf) - 1] == '\n'){
        char *subbuff = (char*)malloc(sizeof(char) * (strlen(buf) - 1));
        substring(buf, subbuff, 0, strlen(buf) - 1);
        return subbuff;
    }
    else
    {
        char *subbuff = (char*)malloc(sizeof(char) * strlen(buf));
        strcpy(subbuff, buf);
        return subbuff;
    }
}

int 
main(int argc, char *argv[])
{
    /* code */
    pid_t pid;
    char buf[MAXPATH];
    char *args[MAXARG][CMDSTYLE];
    char *cmd;
    /* 默认命令为echo */
    if(argc == 1){
        cmd = "echo";
    }
    else{
        cmd = argv[1];
    }
    /* 计数器 */
    int args_num = 0;
    while (1)
    {
        memset(buf, 0, sizeof(buf));
        gets(buf, MAXPATH);
        /* printf("buf:%s",buf); */
        char *arg = cutoffinput(buf);
        /* printf("xargs:gets arg: %s, arglen: %d\n", arg, strlen(arg)); */
        /* press ctrl + D */
        if(strlen(arg) == 0 || args_num >= MAXARG){
            break;
        }
        args[args_num][0] = cmd;
        args[args_num][1] = arg;
        args_num++;
    }

    /* 
        printargs(args, args_num);
        printf("Break Here\n");
     */
    /* 填充exec需要执行的命令至argv2exec */
    int argstartpos = 1;
    char *argv2exec[MAXARG];
    argv2exec[0] = cmd;
    if(argc == 3)
    {
        /* 从2开始 */
        argstartpos++;
        argv2exec[1] = argv[2];
    }
    else if (argc >= 3)
    {
        printf("xargs: too many initial args, only 2 permitted! \n");
        exit();
    }
    for (size_t i = 0; i < args_num; i++)
    {
        /* code */
        argv2exec[i + argstartpos] = args[i][1];
    }
    argv2exec[args_num + argstartpos] = 0;
    
    /* 运行cmd */
    if((pid = fork()) == 0){   
        exec(cmd, argv2exec);    
    }  
    else
    {
        /* code */
        wait();
    }
    exit();
}
