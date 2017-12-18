#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <termio.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_LINE_LENGTH       1 << 10
#define MAX_ARG_LENGTH       1 << 10
#define MAX_ARG_SIZE           100
#define MAX_HIST_SIZE           20

#define INPUT_REDIRECTION     ">"
#define OUTPUT_REDIRECTION   "<"
#define APPEND_REDIRECTION   ">>"
#define PIPELINE                "|"
#define BACKGROUND           "&"

#define ENTER                  10
#define QUIT                   4
#define BACKSPACE             8
#define ESC                    27
#define LSB                    91

#define STATE_NONCANONICAL  0
#define STATE_CANONICAL      1

#define DELIMS                " \t\r\n"
/* alias 추가 하기 위한 선언*/
#define MAXSIZE 	1024
typedef struct list
{
	char *name;
	char *value;
	struct list *prev;
	struct list *next;
}list_t;

list_t *ahead;	//alias list head
list_t *atail; 	//alias list tail

void __alias(char**);
void init_list(void);
void __unalias(char**);
void convargvp(char**, size_t*);
//////////// 여기까지///////////

void set_input_mode(void);
void reset_input_mode(void);
int prompt(char*);

void init_history(char**);
void get_history(char**, int);
void print_history(char**, int);
void clear_history(char**);
bool parse(char*, char**, size_t*);
int lookupRedirection(char**, size_t, int*);
bool lookupBackground(char**, size_t);
bool execute(char**, size_t);

struct termios saved_tty;

char* hist[MAX_HIST_SIZE]={0};
int current_cursor = 0;

int main(int argc, char** argv)
{
  char line[MAX_LINE_LENGTH];

  init_history(hist);
  set_input_mode();
  init_list(); //list초기화
  
  while(true)
  {
    char* arguments[MAX_ARG_LENGTH];
    size_t argument_count = 0;

    if(prompt(line) == 0) continue;

    hist[current_cursor] = strdup(line);
    current_cursor = (current_cursor + 1) % MAX_HIST_SIZE;

    parse(line, arguments, &argument_count);

    if(argument_count == 0) continue;
     
    convargvp(arguments, &argument_count); //alias를 다시 역치환 

    if(strcmp(arguments[0], "exit") == 0)
      exit(EXIT_SUCCESS);
    else if((strcmp(arguments[0], "history") == 0))
      print_history(hist, current_cursor);
    else if(strcmp(arguments[0], "help") == 0)
    {
      fprintf(stdout, "오픈소스소프트웨어 기말프로젝트 \n B389030 박휘만\n B389017 김한성\n");
    }
    else if(strcmp(arguments[0], "clear") == 0)
      system(arguments[0]);
////////////// alias 명령어 추가 ///////////////////
    else if(strcmp(arguments[0], "alias") == 0)
      __alias(arguments);
////////////////////////////////////////////////////
/////////////// unalias 명령어 /////////////////////
    else if(strcmp(arguments[0], "unalias") == 0)
      __unalias(arguments);
////////////////////////////////////////////////////
////////////// Change directory 명령어 추가 ///////////////////////
    else if(strcmp(arguments[0], "cd") == 0){
      chdir(arguments[1]);
    }
///////////////////////////////////////////////////////////////////
    else 
      execute(arguments, argument_count);
  }

  reset_input_mode();
  clear_history(hist);

  return EXIT_SUCCESS;
}

/* HISTORY */
void init_history(char** hist_buf)
{
  int i;
  for(i=0; i<MAX_HIST_SIZE; ++i) hist_buf[i] = NULL;
}

void print_history(char** hist_buf, int cursor)
{
  int i = cursor;
  int n = 1;

  do
  {
    if(hist_buf[i])
      fprintf(stdout, "%2d: %s\n", n++, hist_buf[i]);

    i = (i + 1) % MAX_HIST_SIZE;
  }
  while(i != cursor);
}

void clear_history(char** hist_buf)
{
  int i;

  for(i=0; i<MAX_HIST_SIZE; ++i)
  {
    free(hist_buf[i]);
    hist_buf[i] = NULL;
  }
}

/* INPUT - (NON)CANONICAL MODE */
void set_input_mode(void)
{
  struct termios tty;

  if(!isatty(STDIN_FILENO))
  {
    perror("isatty");

    exit(EXIT_FAILURE);
  }

  tcgetattr(STDIN_FILENO, &saved_tty);
  atexit(reset_input_mode);

  tcgetattr(STDIN_FILENO, &tty);
  tty.c_lflag &= ~(ICANON | ECHO);
  tty.c_cc[VMIN] = 1;
  tty.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &tty);
}

void reset_input_mode(void)
{
  tcsetattr(STDIN_FILENO, TCSANOW, &saved_tty);
}

/* SHELL PROMPT */
int prompt(char* line)
{
  int n = 0;
  int len = 0;
  char c;

  int cur = current_cursor;

  fflush(NULL);

  fprintf(stdout, "[%s]$ ", get_current_dir_name());

  while(c = fgetc(stdin))
  {
    if(c == QUIT)
    {
      fputc('\n', stdout);
      return 0;
    }

    if(c == ENTER)
    {
      fputc('\n', stdout);
      break;      
    }

    if((c != BACKSPACE) && (c <= 26)) break;

    switch(c)
    {
      case BACKSPACE:
        if(n == 0) break;

        fputc('\b', stdout);
        fputc(' ', stdout);
        fputc('\b', stdout);

        line[--n] = (char) 0;

        break;

      case  ESC:
        if((c = fgetc(stdin)) != LSB) break;

        switch(fgetc(stdin))
        {
          case 'A':
            --cur;
            if(cur < 0 ) cur = current_cursor - 1;

	    
            fprintf(stdout, "\r%80s", " ");
            fprintf(stdout, "\r[%s]$ %s", get_current_dir_name(), hist[cur]);

            memset(line, '\0', MAX_LINE_LENGTH);
	///////////////////history segmentation fault/////////////////            
	    if(hist[cur]==NULL)
	    fprintf(stdout,"history blanked!");
	    else{
	//////////////////////////////////////////////////////////////
	    strcpy(line, hist[cur]);
            n = strlen(line);
	    }
	    

            break;

          case 'B':
            ++cur;
              if(cur >= current_cursor) cur = 0;

              fprintf(stdout, "\r%80s", " ");
              fprintf(stdout, "\r[%s]$ %s", get_current_dir_name(), hist[cur]);

              memset(line, '\0', MAX_LINE_LENGTH);
              strcpy(line, hist[cur]);
              n = strlen(line);

              break;
        }

        break;

      default:
        fputc(c, stdout);
        line[n++] = (char) c;

        break;
    }
  }

  line[n] = '\0';

  len = strlen(line);

  if(len == 0) return 0;

  for(n=0; n<len; ++n)
    if(line[n] != ' ' && line[n] != '\t') return 1;

  return 0;
}

int lookupRedirection(char** argv, size_t argc, int* flag)
{
  int i;

  for(i=0; i<(int)argc; ++i)
  {
    if(strcmp(argv[i], INPUT_REDIRECTION) == 0)
    {
      *flag = O_WRONLY | O_CREAT | O_TRUNC;

      break;
    }
    
    if(strcmp(argv[i], APPEND_REDIRECTION) == 0)
    {
      *flag = O_WRONLY | O_CREAT | O_APPEND;

      break;
    }
  }

  return i;
}

bool lookupBackground(char** argv, size_t argc)
{
  return (strcmp(argv[argc - 1], BACKGROUND) == 0);
}

bool parse(char* line, char** argv, size_t* argc)
{
  size_t n = 0;
  char* temp = strtok(line, DELIMS);

  if(temp == NULL) return false;

  while(temp != NULL)
  {
    argv[n++] = temp;

    temp = strtok(NULL, DELIMS);
  }
  argv[n] = NULL;

  *argc = n;

  return true;
}
///////////////alias 함수///////////////////
void __alias(char **argvp)
{
	list_t *t;
	int n;
	char *name, *value;
	char alias[MAXSIZE] = {0};
	
	//alias 명령어만 쳤으면 alias list 출력
	if(argvp[1] ==0)
	{
		t = ahead->next;
		while(t != atail)
		{
			printf("%s=%s\n", t->name, t->value);
			t = t->next;
		}
	}
	else
	{
		//argvp로 분리되어있는 alias 내용을 합침
		n = 1;
		while(argvp[n] != NULL)
		{
			strcat(alias, argvp[n]);
			strcat(alias, " ");
			n++;
		}


		alias[strlen(alias)-1] = '\0';
		
		//새로운 alias list entry에 name과 value 저장
		t = (list_t *)malloc(sizeof(list_t));
		name = strtok(alias, "=");
		value = strtok(NULL, "");
		t->name = (char *)malloc(strlen(name)+1);
		t->value = (char *)malloc(strlen(value)+1);
		strcpy(t->name, name);
		strcpy(t->value, value);

		//alias list의 마지막에 삽입
		t->prev = atail->prev;
		t->next = atail;
		atail->prev->next = t;
		atail->prev = t;

	}
}
//////////////////////////////////////////////
/////////////////list 초기화////////////////////
void init_list(void)
{
	ahead = (list_t *)malloc(sizeof(list_t));
	atail = (list_t *)malloc(sizeof(list_t));

	ahead->name = NULL;
	ahead->value = NULL;
	ahead->prev = ahead;
	ahead->next = atail;
	atail->name = NULL;
	atail->value = NULL;
	atail->value = NULL;
	atail->prev = ahead;
	atail->next = atail;
}
////////////////////////////////////////////
//////////////////unalias//////////////////
void __unalias(char **argvp)
{
	list_t *t, *del;
	
	t = ahead->next;
	while(t != atail)
	{
		if(strcmp(t->name, argvp[1]) == 0)
		{
			del = t;
			del->prev->next = del->next;
			del->next->prev = del->prev;
			t = t->next;
			free(del);
		}
		else
			t = t->next;
	}
}
//////////////////////////////////////////////
//////////////////alias 역치환////////////////////
void convargvp(char **argvp, size_t *argc)
{
	int n, start=0, end=0, i=0, j=0, test=0;
	char *name, *value, *tmp;
	char argtmp[MAXSIZE] = {0};
	char alias[8][MAXSIZE] = {0};
	list_t *t;
	
	for(n=0; argvp[n]!=NULL; n++)
	{
		/* unalias라면 치환하지 않음 */
		if(strcmp(argvp[0], "unalias") == 0)
			break;
		
		/* 인자가 alias 인자라면 원래대로 치환 */
		t = ahead->next;
		while(t != atail)
		{
			if(strcmp(argvp[n], t->name) == 0)
			{
				/* alias 배열에 value의 명령어 구분하여 넣어줌 */
				/* ex> 'ls -l'이였다면 ls와 -l 저장 */
				strcpy(argtmp, t->value);
				argtmp[0] = ' ';
				argtmp[strlen(argtmp)-1] = '\0';
				tmp = strtok(argtmp, " ");
				strcpy(alias[i++], tmp);
				while(tmp = strtok(NULL, " "))
				{
					strcpy(alias[i], tmp);
					i++;
				}
			
				/* argvp에 있는 인자들을 새로이 치환된 alias 인자들이 */
				/* 들어올 수 있도록 공간 확보 차원에서 뒤로 옮김 */
				while(argvp[++end]);
				for(end; end>n; end--)
				{
					argvp[end+i-1] = argvp[end];
				}

				/* 비어있는 공간에 alias 인자 삽입*/
				for(start=n, j=0; j<i; start++, j++)
				{
					tmp = (char *)malloc(strlen(alias[j])+1);
					strcpy(tmp, alias[j]);
					argvp[start] = tmp;
				}
			}
			t = t->next;
		}
	}
	*argc = n;
}
/////////////////////////////////////////////////////////////////////////////

bool execute(char** argv, size_t argc)
{
  char* params[MAX_ARG_LENGTH];

  int fd = -1;
  int flag = 0;
  int idx = lookupRedirection(argv, argc, &flag);

  bool bg = lookupBackground(argv, argc);

  pid_t pid;
  int status;

  int i;

  if((pid = fork()) == -1)
  {
    perror("fork");

    return false;
  }
  else if(pid == 0)
  {
    for(i=0; i<idx; ++i)
    {
      if(strcmp(argv[i], BACKGROUND) == 0) break;

      params[i] = argv[i];
    }

    if(flag > 0)
    {
      if((fd = open(argv[i+1], flag, 0644)) == -1)
      {
        perror("open");

        return false;
      }

      if(close(STDOUT_FILENO) == -1)
      {
        perror("close");
        
        return false;
      }   

      if(dup2(fd, STDOUT_FILENO) == -1)
      {
        perror("dup2");

        return false;
      }

      if(close(fd) == -1)
      {
        perror("close");

        return false;
      }
    }

    if(execvp(argv[0], params) == -1)
    {
      perror("execvp");

      return false;
    }

    exit(EXIT_SUCCESS);
  }
  else
  {
    if(bg == true)
    {
      return true;
    }
    else
    {
      if((pid = waitpid(pid, &status, 0)) == -1)
      {
        perror("waitpid");
      
        return false;
      }

      return true;
    }
  }
}

