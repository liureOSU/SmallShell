
#include "shellfunc.h"

int main()
{
	// Get PID of main shell process.
	shellPID = getpid();
	// Create sigaction structs and fill.
	struct sigaction handleSIGINT = { {0} }, handleSIGTSTP = { {0} };
	// Create inputs struct with all relevant info needed to run script iteration.
	UserInputs inputs;
	initializeArray(&inputs);
	// Loop shell until user exits.
	while (1) {
		setHandlers(&handleSIGINT, &handleSIGTSTP);
		initializeInputs(&inputs);
		userPrompt(&inputs);
		shellScript(&inputs);
		if (DEBUG) { debugScript(inputs); }
	}
	return 0;
}