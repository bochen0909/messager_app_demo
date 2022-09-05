/*
 * MessagerServer.h
 *
 *  Created on: Nov 18, 2019
 */

#ifndef MESSAGERSERVER_H_
#define MESSAGERSERVER_H_

#include <iostream>
#include <map>
#include <vector>
#include "UserInfo.h"
using namespace std;

class ServerListener;

class MessagerServer {
	friend class ServerListener;

public:
	MessagerServer(string user_info_file, string configuration_file);

	virtual ~MessagerServer();

	int run();

	int parse_configuration_file();
	int parse_userinfo_file();
	std::string get_config(const std::string& varname);
	int write_userinfo_file();
	int startListner();

	bool exists(const string& username) {
		return userinfo.count(username) > 0;
	}

	UserInfo& getUser(const string& username) {
		return userinfo.at(username);
	}

	vector<UserInfo> get_friends(const string& username) {
		vector<UserInfo> friends;
		auto& user = userinfo.at(username);
		for (auto &x : user.contacts) {
			friends.push_back(userinfo.at(x));
		}
		return friends;
	}

	void activate(const string& username, int ip, int port, int sockfd) {
		auto& info = userinfo[username];
		info.activate = true;
		info.ip = ip;
		info.port = port;
		info.sockfd = sockfd;
	}

	void deactivate(const string& username) {
		if (exists(username)) {
			auto& info = userinfo[username];
			info.activate = false;
			info.ip = 0;
			info.port = 0;
		}

	}
	vector<string> detected_unactivated_user(int sockfd) {
		vector<string> names;
		for (auto &x : userinfo) {
			if (x.second.sockfd == sockfd) {
				deactivate(x.second.username);
				names.push_back(x.second.username);
			}
		}
		return names;
	}

	void add_friends(const string& username, const string& friendname) {
		if (exists(username)) {
			auto& info = userinfo[username];
			info.contacts.push_back(friendname);
		} else {
			fprintf(stderr, "%s does not found", username.c_str());
		}
	}

	bool check(const string& username, const string& password) {
		if (exists(username)) {
			auto info = userinfo[username];
			if (info.password == password) {
				return true;
			}
		}
		return false;
	}

	bool is_online(const string& username) {
		if (exists(username)) {
			auto& info = userinfo[username];
			return info.activate;
		}
		return false;
	}
	bool is_friend(const string& username, const string& friendname) {
		if (exists(username)) {
			auto& info = userinfo[username];
			auto& v = info.contacts;
			if (std::find(v.begin(), v.end(), friendname) != v.end()) {
				return true;
			} else {
				return false;
			}
		}
		return false;

	}
	bool add(const string& username, const string& password) {
		UserInfo info;
		info.username = username;
		info.password = password;
		userinfo[username] = (info);
		write_userinfo_file();
		return true;
	}

protected:
	string user_info_file;
	string configuration_file;
	int serverPort;
	map<string, string> config;
	map<string, UserInfo> userinfo; //username:userinfo
	ServerListener* listener;

	std::vector<pthread_t> threads;
private:
	static MessagerServer* m_instance; //singleton
};

#endif /* MESSAGERSERVER_H_ */
