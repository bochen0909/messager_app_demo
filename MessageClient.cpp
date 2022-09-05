/*
 * MessageClient.cpp
 *
 *  Created on: Nov 20, 2019
 */

#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <setjmp.h>
#include <signal.h>
#include <pthread.h>
#include <stdlib.h>
#include "Util.h"
#include "MessageClient.h"

#include "ClientListner.h"

static char* command_names[] = { (char*) "help", (char*) "r", (char*) "l",
		(char*) "m", (char*) "i", (char*) "ia", (char*) "logout", (char*) "lf",
		(char*) "query", (char*) "!", (char*) "exit", (char*) 0 };
static char* command_docs[] = {
		(char*) "Display information about built-in commands.",
		(char*) "register a new user", (char*) "login with a existing user",
		(char*) "send message to a friend", (char*) "invite a new friend",
		(char*) "accept a invitation", (char*) "logout current user.",
		(char*) "list friends", (char*) "query available users",
		(char*) "Execute external shell command.", (char*) "Exit.", (char*) 0 };

MessageClient::MessageClient(string configuration_file) :
		MessageClient(configuration_file, 5100) {

}

MessageClient::MessageClient(string configuration_file, int port) :
		line(0), serverPort(0), done(false), listener(0), clientPort(port) {
	this->configuration_file = configuration_file;
	prompt = "Messenger: ";

}

MessageClient::~MessageClient() {
	if (line != NULL)
		free(line);
	if (listener) {
		delete listener;
	}
}

char * command_generator(const char *text, int state) {
	static int list_index, len;
	char *name;

	/* If this is a new word to complete, initialize now.  This includes
	 saving the length of TEXT for efficiency, and initializing the index
	 variable to 0. */
	if (!state) {
		list_index = 0;
		len = strlen(text);
	}

	/* Return the next name which partially matches from the command list. */
	while ((name = command_names[list_index++])) {
		if (strncmp(name, text, len) == 0)
			return (Util::dupstr(name));
	}

	/* If no names matched, then return NULL. */
	return ((char *) NULL);
}

char ** myshell_completion(const char* text, int start, int end) {
	char **matches;

	matches = (char **) NULL;

	/* If this word is at the start of the line, then it is a command
	 to complete.  Otherwise it is the name of a file in the current
	 directory. */
	if (start == 0)
		matches = rl_completion_matches(text, command_generator);

	return (matches);
}

std::string MessageClient::getvar(const std::string& varname) {
	if (variables.find(varname) != variables.end()) {
		return variables[varname];
	} else {
		return "";
	}
}

int MessageClient::readConf() {
	const char* filename = this->configuration_file.c_str();
	if (!Util::file_exists(filename)) {
		Util::log_error("Error! File %s does not exists\n", filename);
		return -1;
	}
	std::vector<std::string> lines = Util::readlines(filename);
	for (size_t i = 0; i < lines.size(); i++) {
		std::string str = Util::trim(lines[i]);
		if (str.empty())
			continue;
		std::vector<std::string> vec = Util::splitByDeli2(str, ':');
		if (vec.size() != 2) {
			Util::log_error("Warning! Ignore line in %s: %s\n", filename,
					str.c_str());
			continue;
		}

		std::string name = vec[0];
		std::string value = vec[1];
		this->variables[name] = value;
		Util::log_debug("DEBUG: config: '%s' -> '%s'\n", name.c_str(),
				value.c_str());
	}

	std::string tmpstr;
	tmpstr = this->getvar("servhost");
	if (tmpstr.empty()) {
		Util::log_error("Error! servhost is not set.\n");
		return -1;
	} else {
		this->serverHost = tmpstr;
	}

	tmpstr = this->getvar("servport");
	if (tmpstr.empty()) {
		Util::log_error("Error! servport is not set.\n");
		return -1;
	} else {
		this->serverPort = atoi(tmpstr.c_str());
	}

	return 0;
}

bool MessageClient::isBuiltin(const std::string& name) {
	char* cmd;
	int i = 0;
	while ((cmd = command_names[i++]) != 0) {
		if (name == cmd) {
			return true;
		}
	}
	return false;
}

int MessageClient::runBuiltin(const std::vector<std::string>& args) {
	string command = args[0];

	/* Call the function. */
	int exit_code = 0;
	if (command == "help" || command == "?") {
		exit_code = this->com_help(args);
	} else if (command == "exit") {
		exit_code = this->com_quit(args);
	} else if (command == "m") {
		exit_code = this->com_chat(args);
	} else if (command == "r") {
		exit_code = this->com_register(args);
	} else if (command == "l") {
		exit_code = this->com_login(args);
	} else if (command == "i") {
		exit_code = this->com_invite(args);
	} else if (command == "ia") {
		exit_code = this->com_invite_accept(args);
	} else if (command == "lf") {
		exit_code = this->com_list_friend(args);
	} else if (command == "query") {
		exit_code = this->com_show_all_users(args);
	} else if (command == "logout") {
		exit_code = this->com_logout(args);
	} else if (command == "!") {
		exit_code = this->com_external(args);
	} else {
		fprintf(stderr, "Command is not builtin: %s\n", command.c_str());
		exit_code = -1;
	}
	return exit_code;
}

bool isValidUserName(std::string name) {
	for (size_t i = 0; i < name.size(); i++) {
		if (isspace(name.at(i))) {
			return false;
		}
	}
	return true;
}

int MessageClient::com_register(std::vector<std::string> args) {
	if (args.size() != 1 && args.size() != 2) {
		fprintf(stderr, "usage: r [username] \n");
		return -1;
	}

	if (this->listener->logged_in) {
		Util::log_error("logout first.\n");
		return -1;
	}

	string name;
	if (args.size() == 1) {
		char _name[128];
		printf("Enter   your username: ");
		fgets(_name, 128, stdin);
		_name[strcspn(_name, "\n")] = '\0';
		name = _name;
	} else {
		name = args[1];
	}

	if (!isValidUserName(name) || (name.size()) == 0) {
		Util::log_error("Error, username cannot contains whitespace.\n");
		return -1;
	}

	string password1 = getpass("Enter   your password: ");
	string password2 = getpass("Confirm your password: ");
	if (password1 != password2) {
		Util::log_error("Error, passwords do not match\n");
		return -1;
	} else {
		int exit_code = this->listener->com_register(name, password1);
		return exit_code;
	}
}

int MessageClient::com_list_friend(std::vector<std::string> args) {
	if (args.size() != 1) {
		fprintf(stderr, "usage: lf \n");
		return -1;
	}
	int exit_code = this->listener->com_list_friend();
	return exit_code;
}

int MessageClient::com_show_all_users(std::vector<std::string> args) {
	if (args.size() != 1) {
		fprintf(stderr, "usage: query \n");
		return -1;
	}
	int exit_code = this->listener->com_show_all_users();
	return exit_code;
}

int MessageClient::com_logout(std::vector<std::string> args) {
	if (args.size() != 1) {
		fprintf(stderr, "usage: logout \n");
		return -1;
	}

	int exit_code = this->listener->com_logout();
	return exit_code;
}

int MessageClient::com_invite(std::vector<std::string> args) {
	if (args.size() != 2 && args.size() != 3) {
		fprintf(stderr, "usage: i <name> [message] \n");
		return -1;
	}

	string username = args[1];
	string message = args.size() == 3 ? args[2] : "";

	int exit_code = this->listener->com_invite(username, message);
	return exit_code;
}

int MessageClient::com_invite_accept(std::vector<std::string> args) {
	if (args.size() != 2 && args.size() != 3) {
		fprintf(stderr, "usage: ia <name> [message] \n");
		return -1;
	}

	string username = args[1];
	string message = args.size() == 3 ? args[2] : "";

	int exit_code = this->listener->com_invite_accept(username, message);
	return exit_code;
}

int MessageClient::com_chat(std::vector<std::string> args) {
	if (args.size() != 3) {
		fprintf(stderr, "usage: m <name> <message> \n");
		return -1;
	}

	string username = args[1];
	string message = args[2];

	int exit_code = this->listener->com_chat(username, message);
	return exit_code;
}

int MessageClient::com_login(std::vector<std::string> args) {
	if (args.size() != 1 && args.size() != 2) {
		fprintf(stderr, "usage: l [username] \n");
		return -1;
	}

	if (this->listener->logged_in) {
		Util::log_error("logout first.\n");
		return -1;
	}

	string name;
	if (args.size() == 1) {
		char _name[128];
		printf("Enter   your username: ");
		fgets(_name, 128, stdin);
		_name[strcspn(_name, "\n")] = '\0';
		name = _name;
	} else {
		name = args[1];
	}
	if (!isValidUserName(name) || name.size() == 0) {
		Util::log_error("Error, username cannot contains whitespace.\n");
		return -1;
	}

	string password1 = getpass("Enter   your password: ");
	int exit_code = this->listener->com_login(name, password1);
	return exit_code;
}

int MessageClient::com_quit(std::vector<std::string> arg) {
	if (this->listener->logged_in) {
		this->listener->com_logout();
	}
	done = 1;
	return 0;
}

void MessageClient::going_stop(bool b) {
	done = 1;
}

int MessageClient::com_help(std::vector<std::string> args) {
	if (args.size() > 2) {
		fprintf(stderr, "usage: help [command]\n");
		return -1;
	}

	register int i;
	for (i = 0; command_names[i]; i++) {
		if (args.size() == 1
				|| (strcmp(args[1].c_str(), command_names[i]) == 0)) {
			printf("%s\t\t%s\n", command_names[i], command_docs[i]);
		}
	}
	return (0);
}

std::vector<string> MessageClient::parse(const std::string& line) {
	if (Util::startsWith(line, "!")) {
		std::vector<std::string> vec;
		vec.push_back("!");
		vec.push_back(line.substr(1));
		return vec;
	} else {
		std::vector<std::string> vec = Util::splitcmdline3(line);
		return vec;
	}
}

int MessageClient::com_external(std::vector<std::string> args) {
	if (args[1].empty()) {
		fprintf(stderr, "usage: !<external command>\n");
		return -1;
	}
	int exit_code = system(args[1].c_str());
	return exit_code;
}

int MessageClient::execute_line(const string& l) {
	std::vector<std::string> vec = parse(l);

	if (vec.size() == 0)
		return 0;
	string cmd = vec[0];
	if (0)
		if (!this->isBuiltin(cmd)) {
			fprintf(stderr, "%s command is not supported.\n", cmd.c_str());
			return 1;
		}
	int exit_code = this->runBuiltin(vec);
	return exit_code;
}

int MessageClient::client_rl_crlf() {
	rl_forced_update_display();
	return 0;
}

int MessageClient::run() {
	if (this->readConf() < 0) {
		return -1;
	}

	rl_readline_name = "MsgClient";
	rl_attempted_completion_function = myshell_completion;

	rl_catch_signals = 1;

	signal(SIGINT, SIG_IGN);

	if (this->startListner() < 0) {
		return -1;
	}

	/* Loop reading and executing lines until the user quits. */
	for (; done == 0;) {
		char* line = readline(this->prompt.c_str());

		if (!line)
			break;

		string s = Util::trim(line);
		if (!s.empty()) {
			add_history(s.c_str());
			execute_line(s);
		}
	}
	printf("\n");
	/* Wait for all threads to complete */
	bool allthreadstopped = false;
	listener->going_stop(true);
	for (int i = 0; i < 10; i++) {
		usleep(300 * 1000);
		if (listener->has_stopped()) {
			allthreadstopped = true;
			break;
		}
	}
	if (!allthreadstopped) {
		fprintf(stderr, "Stopping some threads timeout. Kill them.\n");
		for (size_t i = 0; i < threads.size(); i++) {
			//pthread_tryjoin_np(threads[i], NULL,timeout);
			pthread_kill(threads[i], SIGINT);
		}
	}

	for (size_t i = 0; i < threads.size(); i++) {
		//pthread_tryjoin_np(threads[i], NULL,timeout);
		pthread_join(threads[i], NULL);
	}
	return 0;
}

void* listener_thread_run(void* arg) {
	ClientListner* listener = (ClientListner*) arg;
	listener->startlisten();
	pthread_exit(NULL);
}

int MessageClient::startListner() {
	this->listener = new ClientListner(this);
	if (listener->initialize() < 0) {
		return -1;
	}
	if (listener->connect_peer(Util::host2ip(this->serverHost),
			this->serverPort, true, "MESSENGER SERVER") < 0) {
		return -1;
	}
	Util::log_info("connected to server %s:%d\n", serverHost.c_str(),
			serverPort);

	pthread_t thread;
	pthread_attr_t attr;
	/* For portability, explicitly create threads in a joinable state */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	pthread_create(&thread, &attr, listener_thread_run,
			(void *) this->listener);
	pthread_attr_destroy(&attr);
	this->threads.push_back(thread);
	Util::log_info("Finish starting listener thread tid=%ld\n", thread);
	return 0;
}
