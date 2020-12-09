#include "kernel/types.h"
#include "user/user.h"

const int duration_pos = 1;
typedef enum {wrong_char, success_parse, toomany_char} cmd_parse;
cmd_parse parse_cmd(int argc, char** argv);

int 
main(int argc, char** argv){
    //printf("%d, %s, %s \n",argc, argv[0], argv[1]);
    if(argc == 1){
        printf("Please enter the parameters!");
        exit();
    }
    else{
        cmd_parse parse_result;
        parse_result = parse_cmd(argc, argv);
        if(parse_result == toomany_char){
            printf("Too many args! \n");
            exit();
        }
        else if(parse_result == wrong_char){
            printf("Cannot input alphabet, number only \n");
            exit();
        }
        else{
            int duration = atoi(argv[duration_pos]);
            //printf("Sleeping %f", duration / 10.0);
            sleep(duration);
            exit();
        }
        
    }
}

cmd_parse
parse_cmd(int argc, char** argv){
    if(argc > 2){
        return toomany_char;
    }
    else {
        int i = 0;
        while (argv[duration_pos][i] != '\0')
        {
            /* code */
            if(!('0' <= argv[duration_pos][i] && argv[duration_pos][i] <= '9')){
                return wrong_char;
            }
            i++;
        }
        
    }
    return success_parse;
}