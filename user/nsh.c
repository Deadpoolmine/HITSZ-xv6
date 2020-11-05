#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"
#include "kernel/fcntl.h"

#define MAXBUFSIZ 256 
#define MAXSTACKSIZ 100 
#define MAXARGS 10
#define ECHO "echo"
#define GREP "grep"
#define CAT "cat"   
/* Not allowed to use _MLOC*/

/********************** definition *************************/
char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";



typedef enum CmdType
{
  Execcmd,
  Redircmd,
  Pipecmd,
  NULLcmd
} cmdtype;

typedef enum RedirType
{
  Stdout2file,
  File2stdin
} redirtype;

typedef enum Boolean
{
  false,
  true
} boolean;


typedef struct cmd
{
  /* data */
  cmdtype type;
  union cmdcontent
  {
    struct pipecmd 
    {
      /* data */
      struct cmd* leftcmd;
      struct cmd* rightcmd;
    } pipecmd;

    struct redircmd
    {
      /* data */
      struct cmd* stdincmd;
      struct cmd* stdoutcmd;
      redirtype redirtype;  // <: file2stdin 或者 >: stdout2file 
      int fd; //相关的IO描述符
      char *file; //文件名
      int mode; //打开文件方式
    } redircmd;

    struct execcmd {
      char *argv[MAXARGS];
    } execcmd;

  } cmdcontent;
} cmd;


cmd cmdstack[MAXSTACKSIZ];     //用于给cmd分配空间，保存递归cmd
char tokens[MAXARGS][MAXPATH]; //用于保存execcmd的参数
char files[MAXARGS][MAXPATH];  //用于保存redircmd文件名
/**************************** headerfunction ***********************/
cmd* parsecmd(char *cmd, char *_endofcmd,int currentstackpointer);


/************************* utils ************************/
/* borrow from sh.c line 310 */
void init(); //清空cmdstack以及tokens
int gettoken(char **ps, char *es,int startpos, char **token, char **endoftoken); //利用空格切分Token
void parsetoken(char **token, char *endoftoken, char *parsedtoken);
int allocatestack();
int allocatetokens();
int allocatefiles();
/* do with cmd */
void evaluate(cmd* parsedcmd);
void preprocessCmd(char *cmd);

/*************************** code_implements *************************/

cmd nullcmd;

int 
main() { 
  char _cmd[MAXBUFSIZ];
  nullcmd.type = NULLcmd;
  while (true)
  {
      /* code */
      memset(_cmd, 0, sizeof(_cmd));
      printf("@ ");
      gets(_cmd, MAXBUFSIZ);
      preprocessCmd(_cmd);
      /* important */
      if(strlen(_cmd) == 0 || _cmd[0] == 0){
          exit(0);
      }
      //printf("_cmd: %s\n", _cmd);
      /* parse cmd */
      init();
      char *endofcmd;
      endofcmd = _cmd + strlen(_cmd);
      cmd* parsedcmd = parsecmd(_cmd, endofcmd, 0);
      /* start run */
      if(fork() == 0)
        evaluate(parsedcmd);
      wait(0);
  }
  return 0;
}

/******************* 处理cmd ******************/
void 
evaluate(cmd *parsedcmd){
  int pd[2];

  if(parsedcmd->type == NULLcmd){
    return ;
  }
  
  switch (parsedcmd->type)
  { 
  /* code */
  case Pipecmd:
    /* fprintf(2,"-------------exec pipecmd-------------: \n");
    fprintf(2,"pipe left cmd type: %d\n", parsedcmd->cmdcontent.pipecmd.leftcmd->type);
    fprintf(2,"pipe right cmd type: %d\n", parsedcmd->cmdcontent.pipecmd.rightcmd->type); */
    /* stdout | stdin */
    //parsedcmd.cmdcontent.pipecmd;
    pipe(pd);
    /* stdout */
    /* 左边命令的输出将会定位到标准输出内 */
    if(fork() == 0){
      close(1);
      dup(pd[1]);
      close(pd[0]);
      close(pd[1]);
      //fprintf(2,"arrived at left\n");
      evaluate(parsedcmd->cmdcontent.pipecmd.leftcmd);
    }
    /* stdin */
    /* 右边命令的输入将被重定向到标准输入内 */
    if(fork() == 0){
      close(0);
      dup(pd[0]);
      close(pd[0]);
      close(pd[1]);
      //fprintf(2,"arrived at right \n");
      evaluate(parsedcmd->cmdcontent.pipecmd.rightcmd);
    }
    close(pd[0]);
    close(pd[1]);
    wait(0);
    wait(0);
    /* stdin */
    break;

  case Execcmd:
    /* fprintf(2,"exec execmd: %s, %d\n",parsedcmd->cmdcontent.execcmd.argv[0], strlen(parsedcmd->cmdcontent.execcmd.argv[0])); */
    exec(parsedcmd->cmdcontent.execcmd.argv[0], parsedcmd->cmdcontent.execcmd.argv);
    break;
  
  case Redircmd:
    /* cat < d */
    /* < 0, > 1 */
    /* fprintf(2, "exec redircmd: \n");
    fprintf(2, "file name:%s\n", parsedcmd->cmdcontent.redircmd.file);
    fprintf(2, "stdin cmd:%s \n", parsedcmd->cmdcontent.redircmd.stdincmd->cmdcontent.execcmd.argv[0]);
    fprintf(2, "stdout cmd:%s\n", parsedcmd->cmdcontent.redircmd.stdoutcmd->cmdcontent.execcmd.argv[0]);;
    fprintf(2, "stdout cmd type:%d\n", parsedcmd->cmdcontent.redircmd.stdoutcmd->type);
    fprintf(2, "fd:%d\n", parsedcmd->cmdcontent.redircmd.fd);  */

    close(parsedcmd->cmdcontent.redircmd.fd);
    if(open(parsedcmd->cmdcontent.redircmd.file, parsedcmd->cmdcontent.redircmd.mode) < 0){
      fprintf(2, "open %s failed\n", parsedcmd->cmdcontent.redircmd.file);
      exit(-1);
    }
    if(parsedcmd->cmdcontent.redircmd.redirtype == File2stdin){
      evaluate(parsedcmd->cmdcontent.redircmd.stdincmd);
    }
    else if(parsedcmd->cmdcontent.redircmd.redirtype == Stdout2file)
    {
      evaluate(parsedcmd->cmdcontent.redircmd.stdoutcmd);
    }
    break;
  default:
    break;
  }
  
}

/* | > < */
/* 管道|具有最高优先级 */
cmd* 
parsecmd(char *_cmd, char *_endofcmd, int currentstackpointer){
  char *s;
  s = _endofcmd;
  //printf("--------------------parsecmd--------------------\n");
  boolean isexec = true;
  boolean ispipe = false;
  /* 先找管道 */
  for(; s >= _cmd; s--){
    if (*s == '|')
    {
      cmdstack[currentstackpointer].type = Pipecmd;
      cmdstack[currentstackpointer].cmdcontent.pipecmd.leftcmd = parsecmd(_cmd, s - 1, allocatestack());
      cmdstack[currentstackpointer].cmdcontent.pipecmd.rightcmd = parsecmd(s + 1, _endofcmd, allocatestack());
      isexec = false;
      ispipe = true;
      break;
    }
  }
  /* 再找重定向符 */
  if(!ispipe){
    s = _endofcmd;
    for (; s >= _cmd; s--)
    {
      /* code */
      if (*s == '<' || *s == '>')
      {
        cmdstack[currentstackpointer].type = Redircmd;
        /* code */
        /* stdin < file */
        if(*s == '<'){
          cmdstack[currentstackpointer].cmdcontent.redircmd.redirtype = File2stdin;
          cmdstack[currentstackpointer].cmdcontent.redircmd.fd = 0;
          cmdstack[currentstackpointer].cmdcontent.redircmd.stdincmd = parsecmd(_cmd, s - 1, allocatestack());
          cmdstack[currentstackpointer].cmdcontent.redircmd.stdoutcmd = &nullcmd;
          cmdstack[currentstackpointer].cmdcontent.redircmd.mode = O_RDONLY;
        }
        /* stdout > file */
        else {
          cmdstack[currentstackpointer].cmdcontent.redircmd.redirtype = Stdout2file;
          cmdstack[currentstackpointer].cmdcontent.redircmd.fd = 1;
          cmdstack[currentstackpointer].cmdcontent.redircmd.stdincmd = &nullcmd;
          cmdstack[currentstackpointer].cmdcontent.redircmd.stdoutcmd = parsecmd(_cmd, s - 1, allocatestack());
          cmdstack[currentstackpointer].cmdcontent.redircmd.mode = O_WRONLY|O_CREATE;
        }
        char *file, *endoffile;
        gettoken(&_cmd, _endofcmd,  s - _cmd + 1, &file, &endoffile);
        int pos = allocatefiles();
        parsetoken(&file, endoffile, files[pos]);
        cmdstack[currentstackpointer].cmdcontent.redircmd.file = files[pos]; 
        isexec = false;
        break;
      }
    }
    if(isexec){
      cmdstack[currentstackpointer].type = Execcmd;
      int totallen = _endofcmd - _cmd;
      int startpos = 0;
      int count = 0;
      while (startpos < totallen)
      {
        /* code */
        char *token, *endoftoken;
        startpos = gettoken(&_cmd, _endofcmd, startpos, &token, &endoftoken);
        if(*token != ' '){
          //printf("token addr: %p\n", token);
          int pos = allocatetokens();
          parsetoken(&token, endoftoken, tokens[pos]);
          //printf("token: %s, %d\n", tokens[pos], strlen(tokens[pos]));
          cmdstack[currentstackpointer].cmdcontent.execcmd.argv[count] = tokens[pos];
          count++;
        }
      }
      cmdstack[currentstackpointer].cmdcontent.execcmd.argv[count] = 0;
    }
  }
  return &cmdstack[currentstackpointer];
}

void 
init(){
  memset(tokens, 0, sizeof(tokens));
  memset(files, 0, sizeof(files));
  memset(cmdstack, 0, sizeof(cmdstack));
  for (int i = 0; i < MAXSTACKSIZ; i++)
  {
    /* code */
    cmdstack[i].type = NULLcmd;
  }
}
/* 分配栈 */
int
allocatestack(){
  int newpointer = 0;
  while(cmdstack[newpointer].type != NULLcmd) newpointer++;
  return newpointer;
}

int
allocatetokens(){
  int newpointer = 0;
  while(tokens[newpointer][0] != 0) newpointer++;
  return newpointer;
}

int
allocatefiles(){
  int newpointer = 0;
  while(files[newpointer][0] != 0) newpointer++;
  return newpointer;
}

/* 去掉回车符 */
void
preprocessCmd(char *cmd){
  int n = strlen(cmd);
  if(n > MAXBUFSIZ){
      printf("command too long!");
      exit(0);
  }
  else
  {
      /* code */
      if(cmd[n - 1] == '\n'){
          cmd[n - 1] = '\0';
      }
  }
}

/************************* Utils **************************/
void parsetoken(char **token, char *endoftoken, char *parsedtoken){
  //printf("gettoken: ");
  char *s = *token;
  for (; s < endoftoken; s++)
  {
    *(parsedtoken++) = *s;
    //printf("%c", *s);
  }
  *parsedtoken = '\0';
  //printf("\n");
}

int
gettoken(char **ps, char *es, int startpos, char **token, char **endoftoken)
{
  char *s;
  int pos = startpos;
  s = *ps + startpos;
  /* 清理所有s的空格 trim */
  while(s < es && strchr(whitespace, *s)){
    s++;
    pos++;
  }
  *token = s;
  while (s < es && !strchr(whitespace, *s))
  {
    /* code */
    s++;
    pos++;
  }
  *endoftoken = s;
  return pos;
}  