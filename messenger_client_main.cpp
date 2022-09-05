#include <stdio.h>
#include "MessageClient.h"
#include "stdlib.h"
/**
 * Entry for client program.
 */
int main(int argc, char** argv) {
	if (argc != 2 && argc != 3) {
		fprintf(stderr, "usage: %s  <configuration file> [port]\n",
				argv[0]);
		return -1;
	}
	int port =5100;
	if (argc==3){
		port =atoi(argv[2]);
	}
	MessageClient client(argv[1],port);
	int exit_code = client.run();
	return exit_code;
}
