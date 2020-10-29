#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

//Copy from Grep.c
char buf[1024];
int match(char*, char*);
int matchhere(char*, char*);
int matchstar(int, char*, char*);

int
match(char *re, char *text)
{
  if(re[0] == '^')
    return matchhere(re+1, text);
  do{  // must look at empty string
    if(matchhere(re, text))
      return 1;
  }while(*text++ != '\0');
  return 0;
}

// matchhere: search for re at beginning of text
int matchhere(char *re, char *text)
{
  if(re[0] == '\0')
    return 1;
  if(re[1] == '*')
    return matchstar(re[0], re+2, text);
  if(re[0] == '$' && re[1] == '\0')
    return *text == '\0';
  if(*text!='\0' && (re[0]=='.' || re[0]==*text))
    return matchhere(re+1, text+1);
  return 0;
}

// matchstar: search for c*re at beginning of text
int matchstar(int c, char *re, char *text)
{
  do{  // a * matches zero or more instances
    if(matchhere(re, text))
      return 1;
  }while(*text!='\0' && (*text++==c || c=='.'));
  return 0;
}


/*
  find.c
*/
char* fmtname(char *path);
void find(char *path, char *re);

int 
main(int argc, char** argv){
    if(argc < 2){
      printf("Parameters are not enough\n");
    }
    else{
      //在路径path下递归搜索文件 
      find(argv[1], argv[2]);
    }
    exit();
}

// 对ls中的fmtname，去掉了空白字符串
char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;
  // printf("len of p: %d\n", strlen(p));
  if(strlen(p) >= DIRSIZ)
    return p;
  memset(buf, 0, sizeof(buf));
  memmove(buf, p, strlen(p));
  //memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  return buf;
}

void 
find(char *path, char *re){
  // printf("---------------------------------------------\n");
  // printf("path:%s\n", path);
  // printf("fmtpath:%s\n",fmtname(path));
  // printf("re:%s\n", re);
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;
  
  if((fd = open(path, 0)) < 0){
      fprintf(2, "find: cannot open %s\n", path);
      return;
  }

  if(fstat(fd, &st) < 0){
      fprintf(2, "find: cannot stat %s\n", path);
      close(fd);
      return;
  }
  
  switch(st.type){
  case T_FILE:
      //printf("File re: %s, fmtpath: %s\n", re, fmtname(path));
      if(match(re, fmtname(path)))
          printf("%s\n", path);
      break;
          //printf("%s %d %d %l\n", fmtname(path), st.type, st.ino, st.size);

  case T_DIR:
      if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
          printf("find: path too long\n");
          break;
      }
      strcpy(buf, path);
      p = buf + strlen(buf);
      *p++ = '/';
      while(read(fd, &de, sizeof(de)) == sizeof(de)){
          if(de.inum == 0)
              continue;
          memmove(p, de.name, DIRSIZ);
          p[DIRSIZ] = 0;
          if(stat(buf, &st) < 0){
              printf("find: cannot stat %s\n", buf);
              continue;
          }
          // printf("%s, %d\n",fmtname(buf), strlen(fmtname(buf)));
          // printf("%s\n",buf);
          // // printf("%d\n",strcmp(".", fmtname(buf)));
          // // printf("%d\n",strcmp("..", fmtname(buf)));
          char* lstname = fmtname(buf);
          // printf("lstname: %s\n", lstname);
          if(strcmp(".", lstname) == 0 || strcmp("..", lstname) == 0){
            //printf("%s %d %d %d\n", buf, st.type, st.ino, st.size);
            continue;
          }
          else{
            //printf("deep: %s %d %d %d\n", buf, st.type, st.ino, st.size);
            find(buf, re);
          }
          //printf("%s %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);
      }
      break;
  }
  close(fd);
}