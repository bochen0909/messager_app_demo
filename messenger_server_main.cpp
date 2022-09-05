#include <stdio.h>
#include "MessagerServer.h"

/**
 * Entry for server program.
 */
int main(int argc, char** argv) {
	if (argc != 3) {
		fprintf(stderr, "usage: %s <user info file> <configuration file>\n",
				argv[0]);
		return -1;
	}
	MessagerServer server(argv[1], argv[2]);
	int exit_code = server.run();
	return exit_code;
}
