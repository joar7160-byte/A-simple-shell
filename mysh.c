#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_LINE 4096

int main(int argc, char *argv[]){
    char line[MAX_LINE];
    int running = 1;

    while(running){
        printf("mysh$ ");     //temp prompt 
        fflush(stdout);      
        char ch;              //holds each character 
        int ind = 0;   
        ssize_t bytes;      // # of bytes returned by read


        while ((bytes = read(STDIN_FILENO, &ch, 1)) > 0){ //reads one char at a time
            if (ch == '\n')
            {
                break; 
            }
            
            //Check if ind is within the Max_Line and put index in our max line array
            if (ind < MAX_LINE - 1){
                line[ind] = ch; 
                ind++;
            }
        }


        //Stop reading or input ended
        if(bytes < 0 || (bytes == 0 && ind == 0)){
            break;
        }
        
        line[ind] = '\0';    //end of string

        if(strcmp(line, "exit") == 0){ //exit command so stop running shell
            running = 0;   
        }

        else if(strcmp(line, "pwd") == 0){ //if command is pwd
            char cdp[4096];      //used to store curr directory path

            //get curr directory and print each 
            if(getcwd(cdp, sizeof(cdp)) != NULL){ 
                printf("%s\n", cdp);
            }
            else{
                perror("pwd");
            }
        }
    }
    return EXIT_SUCCESS;
}