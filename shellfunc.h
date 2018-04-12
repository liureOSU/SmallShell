
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#define MAX_LEN 2048
#define MAX_ARGS 512
#define DEBUG 0
int shellPID;

// struct UserInputs contains all user data needed to run shell commands.
typedef struct {
	char* usrTokInp[MAX_ARGS];
	char inputFile[MAX_LEN];
	char outputFile[MAX_LEN];
	int length;
	bool inRedirect, outRedirect, backgroundFlag;
} UserInputs;

int userPrompt(UserInputs*);
void initializeArray(UserInputs*);
void initializeInputs(UserInputs*);
int shellScript(UserInputs*);
int childProcessCommand(UserInputs);
void catchSIGTSTP(int);
void setHandlers(struct sigaction*, struct sigaction*);
void debugScript(UserInputs);
void checkChildren();