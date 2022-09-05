/*
 * MessageClient.h
 *
 *  Created on: Nov 20, 2019
 */

#ifndef MESSAGECLIENT_H_
#define MESSAGECLIENT_H_
#include <string>
#include <map>
#include <vector>

using namespace std;

class ClientListner;

class MessageClient {
	friend class ClientListner;
public:
	MessageClient(string configuration_file);
	MessageClient(string configuration_file, int clientPort);
	virtual ~MessageClient();
	int run();
	int readConf();
	std::string getvar(const std::string& varname);

	int startListner();

	static bool isBuiltin(const std::string& name);

	int runBuiltin(const std::vector<std::string>& args);

	void set_prompt(const std::string& p) {
		this->prompt = p;
	}
	;

	/**
	 * Buildin command
	 */
	int com_help(std::vector<std::string> arg);

	/**
	 * Buildin command
	 */
	int com_quit(std::vector<std::string> arg);

	/**
	 * Buildin command
	 */
	int com_register(std::vector<std::string> arg);

	/**
	 * Buildin command
	 */
	int com_invite(std::vector<std::string> arg);

	/**
	 * Buildin command
	 */
	int com_invite_accept(std::vector<std::string> arg);

	/**
	 * Buildin command
	 */
	int com_chat(std::vector<std::string> arg);

	/**
	 * Buildin command
	 */
	int com_login(std::vector<std::string> arg);
	/**
	 * Buildin command
	 */
	int com_list_friend(std::vector<std::string> arg);
	/**
	 * Buildin command
	 */
	int com_show_all_users(std::vector<std::string> arg);

	/**
	 * Buildin command
	 */
	int com_logout(std::vector<std::string> arg);

	/**
	 * Buildin command
	 */
	int com_external(std::vector<std::string> ignore);

	int execute_line(const string& l);
	int client_rl_crlf();
	std::vector<string> parse(const std::string& line);
	void going_stop(bool b);
protected:
	string configuration_file;

	char* line;
	int serverPort;
	int done;
	ClientListner* listener;
	int clientPort;

	string serverHost;

	std::map<std::string, std::string> variables;

	std::vector<pthread_t> threads;

	string prompt;

};

#endif /* MESSAGECLIENT_H_ */
