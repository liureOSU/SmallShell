
#include "shellfunc.h"

// Global variable definitions.
int exitStatus = -5;
int childPID[100];
int childIndex = -1;
bool blockAllBGProcesses = false;

/************************************************************************
#   checkChildren()
#   Use to check for completion of background child processes. Displays
#	exit status to stdout on completion of bg process.
#
#   Input:
#          null
#   Output:
#          displays bg process PID and exit status (signal or completion)
*************************************************************************/
void checkChildren()
{
	if (childIndex < 0) {
		return;
	}
	else {
		int i;
		int childExitStatus;
		for (i = 0; i < childIndex + 1; i++) {

			int returnV = waitpid(childPID[i], &childExitStatus, WNOHANG);
			// Check if PID has finished.
			if (returnV > 0) {
				// Display exited background child PID if applicable.
				if (WIFEXITED(childExitStatus) && childPID[i] > 0) {
					printf("Background process: %d has exited with status %d.\n", returnV, childExitStatus);
					fflush(stdout);
					// Remove finished terminated PID afterwards.
					childPID[i] = -2;
				}
				// Display signal terminated background child PID if applicable.
				else if (WIFSIGNALED(childExitStatus) && childPID[i] > 0) {

					printf("Background process: %d terminated with signal %d.\n", returnV, childExitStatus);
					fflush(stdout);
					// Remove finished terminated PID afterwards.
					childPID[i] = -2;
				}
			}
		}
	}
}

/************************************************************************
#   shellScript()
#   Contains operations for static functions 'exit', 'cd' and 'status'.
#   Also contains handlers for empty line and user comments #.  Commands
#	other than these are given to childProcessCommand() to execute in
#	bash.  Handles multiple arguments in input line.  
#
#	If EXIT is used while background processes are still running, 
#	kill off those background processes first.
#
#   Input:
#          UserInputs* inputs - contains arguments used to run shell.
#   Output:
#          EXIT, CD, STATUS, or comment/empty line command.
*************************************************************************/
int shellScript(UserInputs* inputs)
{
	// If comment or empty, then do nothing.
	if (!strcmp(inputs->usrTokInp[0], "\0") || !strcmp(inputs->usrTokInp[0], "\n") || !strncmp(inputs->usrTokInp[0], "#", 1)) {
		//printf("Here empty...\n");
		return 0;
	}
	// EXIT command.
	else if (!strcmp(inputs->usrTokInp[0], "exit")) {
		
		// Kill off any remaining background processes.
		int i;
		for (i = 0; i < childIndex + 1; i++) {
			kill(childPID[i], 15);
		}

		// Exit after all bg children complete.
		exit(0);
	}
	// CD command.
	else if (!strcmp(inputs->usrTokInp[0], "cd")) {
		// If only one argument cd to HOME environment variable.
		if (inputs->length == 0) {
			char* newDir = getenv("HOME");
			int cd = chdir(newDir);
			if (cd == -1) {
				printf("Problem changing into HOME directory.\n");
				fflush(stdout);
			}
		}
		// Else, find cwd, append arg[1] to it and cd to new dir.
		else {
			char newDir[1024];
			getcwd(newDir, sizeof(newDir));
			strcat(newDir, "/");
			strcat(newDir, inputs->usrTokInp[1]);
			strcat(newDir, "/");
			// Change to newly created directory in cwd.
			int cd = chdir(newDir);
			if (cd == -1) {
				printf("Problem changing into new directory.\n");
				fflush(stdout);
			}
		}
		return 0;
	}
	// STATUS command.
	else if (!strcmp(inputs->usrTokInp[0], "status")) {
		printf("exit status: %d\n", exitStatus);
		return 0;
	}
	// OTHER commands.
	else {
		childProcessCommand(*inputs);
		return 0;
	}
}

/************************************************************************
#   childProcessCommand()
#   Creates a child process to handle commands other than CD, EXIT or
#   STATUS by using execvp() to bash.  Handles input and output
#	redirection through the < and > commands.  Also handles foreground
#	and background child processes.
#
#   Input:
#          UserInputs inputs - contains arguments used to run shell.
#   Output:
#          fg or bg child process handles execvp call to outside command.
#		   Parent process waits for termination of fg process.
*************************************************************************/
int childProcessCommand(UserInputs inputs)
{
	// Create child process.
	pid_t spawnPID = -5;
	spawnPID = fork();
	int childExitMethod = -5;
	int outputTarget, inputTarget;

	// Set argument after last user argument to NULL for exec calls.
	inputs.usrTokInp[inputs.length + 1] = NULL;

	switch (spawnPID) {
	case -1: // child process fork error
		perror("Child NOT spawned.\n");
		fflush(stdout);
		break;

	case 0: // child process sucessfully created

			// Start output redirection if > option toggled.
		if (inputs.outRedirect) {
			outputTarget = open(inputs.outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (outputTarget == -1) {
				printf("ERROR: Cannot open %s for output redirection.\n", inputs.outputFile);
				fflush(stdout);
			}
			int redir = dup2(outputTarget, 1);
			if (redir == -1) { perror("ERROR: Output redirection failed.\n"); exit(1); }
		}

		// Start input redirection if < option toggled.
		if (inputs.inRedirect) {
			inputTarget = open(inputs.inputFile, O_RDONLY);
			if (inputTarget == -1) {
				printf("ERROR: Cannot open %s for input redirection.\n", inputs.inputFile);
				fflush(stdout);
			}
			int redir = dup2(inputTarget, 0);
			if (redir == -1) {
				perror("ERROR: Input redirection failed.\n");
				fflush(stdout);
				exit(1);
			}
		}

		// If background option is specified, redirect input and output if those flags are false.
		if (inputs.backgroundFlag && blockAllBGProcesses == false) {
			if (!inputs.outRedirect) {
				outputTarget = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);
				dup2(outputTarget, 1);
			}
			if (!inputs.inRedirect) {
				inputTarget = open("/dev/null", O_RDONLY);
				dup2(inputTarget, 0);
			}
		}

		// Close < and > files.
		if (inputs.outRedirect) { if (close(outputTarget) == -1) { exit(1); } }
		if (inputs.inRedirect) { if (close(inputTarget) == -1) { exit(1); } }

		// Set fg signal handling if bg flag not set.
		if (inputs.backgroundFlag == 0 || blockAllBGProcesses) {
			struct sigaction fgAction = { { 0 } };
			fgAction.sa_handler = SIG_DFL; // terminate fg processes on SIGINT catch.
			sigfillset(&fgAction.sa_mask);
			fgAction.sa_flags = SA_RESTART;
			sigaction(SIGINT, &fgAction, NULL);
		}

		// Execute new program.
		if (execvp(inputs.usrTokInp[0], inputs.usrTokInp) == -1) {
			printf("%s: no such file or directory\n", inputs.usrTokInp[0]);
			fflush(stdout);
			exit(1);
		}

		break;

	default: // parent process
		// If FOREGROUND PROCESS
		if (inputs.backgroundFlag == false || blockAllBGProcesses) {
			waitpid(spawnPID, &childExitMethod, 0); // wait until completion then print exit status.
			if (WIFEXITED(childExitMethod)) {
				exitStatus = WEXITSTATUS(childExitMethod);
			}
			else if (WIFSIGNALED(childExitMethod) != 0) { // or print termination signal.
				exitStatus = WTERMSIG(childExitMethod);
				printf("\nForeground process: %d terminated with signal %d.\n", spawnPID, exitStatus);
			}
			else {
				exit(1);
			}
		}
		// If BACKGROUND PROCESS
		else {
			// Print background start message.
			printf("Background process: %d begun.\n", spawnPID);
			// Add to array of bg process PIDs for later handling.
			childIndex++;
			childPID[childIndex] = spawnPID;
		}
		break;
	}
	return 0;
}

/************************************************************************
#   initializeInputs()
#   Sets UserInputs* inputs attributes to default values.
#
#   Input:
#          UserInputs* inputs - contains arguments needed to run shell.
#   Output:
#          Cleared struct UserInputs ready for next user command.
*************************************************************************/
void initializeInputs(UserInputs* inputs)
{
	inputs->inRedirect = inputs->outRedirect = inputs->backgroundFlag = false;
	inputs->length = -1;

	int i;
	for (i = 0; i < MAX_ARGS; i++) {
		memset(inputs->usrTokInp[i], '\0', MAX_LEN);
	}
	memset(inputs->outputFile, '\0', MAX_LEN);
	memset(inputs->inputFile, '\0', MAX_LEN);
}

/************************************************************************
#   userPrompt()
#   Fills struct UserInputs* inputs with user defined commands. Tokenizes
#   multi-argument input and parses them into array inputs->userTokInp[].
#	Handles input (<) output (>) and background process (&) commands by
#	setting those flags in UserInputs.  Parses $$ substring into processID.
#
#   Input:
#          UserInputs* inputs - contains arguments needed to run shell.
#   Output:
#          inputs contains user prompts used to run iteration of shell.
*************************************************************************/
int userPrompt(UserInputs* inputs)
{
	char* tok;
	char usrInp[MAX_LEN];

	// Check on background children process completion.
	checkChildren();

	// Print colon to output.
	printf(": ");
	fflush(stdout);


	// Get input with maximum length of 2048.
	fgets(usrInp, MAX_LEN, stdin);
	// Return 1 if input was null.
	if (usrInp == NULL) {
		return 1;
	}

	// Tokenize string and place into userInputs.
	tok = strtok(usrInp, " \n");
	while (tok != NULL) {
		// Check if PID shortcut $$ is used.  If so, replace with PID.
		if (strstr(tok, "$$")) {
			int pd = getpid();
			char *ch = strstr(tok, "$$");
			char pidstr[50];
			sprintf(pidstr, "%d", pd);
			static char buffer[1000];

			strncpy(buffer, tok, ch - tok);
			buffer[ch - tok] = '\0';
			sprintf(buffer + (ch - tok), "%s%s", pidstr, ch + strlen("$$"));

			++inputs->length;
			strcpy(inputs->usrTokInp[inputs->length], buffer);

		}

		// Check if input redirection is used.
		else if (!strncmp(tok, "<\0", 2)) {
			// Check that there is not already an input redirection.
			if (inputs->inRedirect) {
				printf("< already used once.  Invalid input.\n");
				fflush(stdout);
				exit(1);
			}
			// Otherwise place into arguments array and note index of redirect file.
			else {
				inputs->inRedirect = true;
				tok = strtok(NULL, " \n");
				if (tok == NULL) {
					printf("ERROR: No input file in prompt.");
					fflush(stdout);
					exit(1);
				}
				strcpy(inputs->inputFile, tok);
			}
		}
		// Check if output redirection is used.
		else if (!strncmp(tok, ">\0", 2)) {
			// Check that there is not already an output redirection.
			if (inputs->outRedirect) {
				printf("> already used once.  Invalid input.\n");
				fflush(stdout);
				exit(1);
			}
			// Otherwise place into arguments array and note index of redirect file.
			else {
				inputs->outRedirect = true;
				tok = strtok(NULL, " \n");
				if (tok == NULL) {
					printf("ERROR: No output file in prompt.");
					fflush(stdout);
					exit(1);
				}
				strcpy(inputs->outputFile, tok);
			}
		}
		// Otherwise normal input is placed into arguments array.
		else {
			++inputs->length;
			strcpy(inputs->usrTokInp[inputs->length], tok);
		}
		tok = strtok(NULL, " \n");
	}
	// If the last input is an ampersand, erase it and set backgd flag.
	if (!strncmp(inputs->usrTokInp[inputs->length], "&\0", 2)) {

		memset(inputs->usrTokInp[inputs->length], '\0', MAX_LEN);
		inputs->backgroundFlag = true;
		--inputs->length;
	}
	return 0;
}

/************************************************************************
#   initializeArray()
#	Used to initialize shell arrays when program is initially run.
#
#   Input:
#          UserInputs* inputs - contains arguments needed to run shell.
#   Output:
#          Initialized inputs->userTokInp[] and childPID arrays.
*************************************************************************/
void initializeArray(UserInputs *inputs)
{
	int i; // initialize inputs->usrTokInp[] array.
	for (i = 0; i < MAX_ARGS; i++) {
		inputs->usrTokInp[i] = malloc((MAX_LEN) * sizeof(char));
	}

	for (i = 0; i < 101; i++) {
		childPID[i] = -2; // initialize childPID array.
	}
}

/************************************************************************
#   debugScript()
#	Debug outputs to stdout.
#
#   Input:
#          UserInputs* inputs - contains arguments needed to run shell.
#   Output:
#          Debug outputs to stdout. 
*************************************************************************/
void debugScript(UserInputs inputs)
{
	printf("in: %d, out: %d, back: %d, inFile: %s, outFile: %s\n",
		inputs.inRedirect, inputs.outRedirect, inputs.backgroundFlag,
		inputs.inputFile, inputs.outputFile);
	fflush(stdout);
}

/************************************************************************
#   catchSIGTSTP
#	Handler for all SIGTSTP signals.  Toggles foreground-only mode.
#
#   Input:
#          int signo
#   Output:
#          toggles foreground-only mode (ignore & command if on)
*************************************************************************/
void catchSIGTSTP(int signo)
{
	// Debug output to stdout.
	char* message = "Caught SIGTSTP.\n";
	if (DEBUG) { write(STDOUT_FILENO, message, 16); }


	// Switch background process blocker on and off.
	if (blockAllBGProcesses) {
		printf("\nExiting foreground-only mode.\n");
		printf(": ");
		fflush(stdout);
		blockAllBGProcesses = false;
	}
	else {
		printf("\nEntering foreground-only mode (& is now ignored).\n");
		printf(": ");
		fflush(stdout);
		blockAllBGProcesses = true;
	}
}

/************************************************************************
#   setHandlers()
#	Reset SIGINT and SIGTSTP behavior at the start of each shell loop.
#
#   Input:
#          handleSIGINT and handleSIGTSTP sigaction structs.
#   Output:
#          Resets SIGINT and SIGTSTP handler behavior.
*************************************************************************/
void setHandlers(struct sigaction* handleSIGINT, struct sigaction* handleSIGTSTP)
{
	handleSIGINT->sa_handler = SIG_IGN;
	sigfillset(&handleSIGINT->sa_mask);
	handleSIGINT->sa_flags = SA_RESTART;

	handleSIGTSTP->sa_handler = catchSIGTSTP;
	sigfillset(&handleSIGTSTP->sa_mask);
	handleSIGTSTP->sa_flags = SA_RESTART;

	sigaction(SIGINT, handleSIGINT, NULL);
	sigaction(SIGTSTP, handleSIGTSTP, NULL);
}