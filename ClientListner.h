/*
 * ClientListner.h
 *
 *  Created on: Nov 20, 2019
 */

#ifndef CLIENTLISTNER_H_
#define CLIENTLISTNER_H_
#include <map>
#include "json.hpp"
#include "UserInfo.h"
#include "conf.h"
using json = nlohmann::json;
using namespace std;

struct ClientInfo {
	int ip; //host order
	unsigned short port;
	int sockfd;
	bool is_server;
	string name;
	std::string address();
	std::string toString();
};

class MessageClient;

class ClientListner {
	friend class MessageClient;
public:
	ClientListner(MessageClient*);
	virtual ~ClientListner();

	int initialize();

	bool has_stopped();

	bool going_stop();

	void going_stop(bool b);

	int connect_peer(int ip, unsigned short port, bool is_server,
			const string& name);

	int startlisten();

	int handle_new_peer();
	int show_friends(const string& msg);
	int show_users(const string& msg);

	int update_friends(const string& msg);
	int update_friends_json(json& j);

	int handle_peer_select(fd_set& rset, int msgcnt);

	int handle_peer_msg(int sockfd, const std::string& msg);

	int handle_on_user_state(int sockfd, json& msg);

	int handle_chat_message(int sockfd, json& msg);
	int handle_on_show_invitation(int sockfd, json& msg);
	int handle_on_invite(int sockfd, json& msg);
	int handle_on_invite_accept(int sockfd, json& msg);

	size_t read_one_packet(int sockfd, std::string& buf);

	void erasePeer(int sockfd);

	int com_register(const string& name, const string &password);
	int com_logout();
	int com_list_friend();

	int com_show_all_users();

	int com_chat(const string& name, const string &message);
	int com_invite(const string& name, const string &message);
	int com_invite_accept(const string& name, const string &message);

	int com_login(const string& name, const string &password);

	void activate(const string& username, bool a) {
		activate(username, a, 0, 0);
	}

	void activate(const string& username, bool a, int ip, const int port) {
		if (friends.count(username) > 0) {
			auto& x = friends[username];
			x.activate = a;
			if (!a) {
				x.ip = 0;
				x.port = 0;
				x.sockfd = 0;
			} else {
				x.ip = ip;
				x.port = port;
				x.sockfd = 0;
			}
		}
	}

	bool is_friend(const string& username) {
		return friends.count(username) > 0;
	}

	const UserInfo& get_friend(const string& username) {
		return friends[username];
	}

	int _make_chat_connection(const string& name, int ip, int port);

	int find_client_fd(const string& username) {
		for (auto& c : clients) {
			if (c.second.name == username) {
				return c.first;
			}
		}
		return -1;
	}

	int clean_all_clients();

	size_t maxPeerSize();
protected:
	int listenfd;
	int hostip;
	bool bStop;
	bool hasStoped;

	int maxfd;

	MessageClient* client;

	int serverSockfd;
	fd_set fdset;

	std::map<int, ClientInfo> clients;

	std::map<string, UserInfo> friends;

	std::string current_user;
	bool logged_in;

};

#endif /* CLIENTLISTNER_H_ */
