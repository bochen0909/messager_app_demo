/*
 * Listener.h
 */

#ifndef LISTENER_H_
#define LISTENER_H_
#include <map>
#include <set>
#include "json.hpp"
#include "conf.h"
using json = nlohmann::json;
using namespace std;

//forward declaration
class MessagerServer;

/**
 * One peer host
 */
struct ClientInfo {
	int ip; //host order
	unsigned short port;
	int sockfd;
	std::string address();
	std::string toString();
};

typedef struct _InviteInfo {
	std::string username;
	std::string friendname;
	std::string message;

	bool operator<(const _InviteInfo& t) const {
		if (this->username < t.username) {
			return true;
		} else if (this->username == t.username) {
			return this->friendname < t.friendname;
		} else {
			return false;
		}
	}

} InviteInfo;

class ServerListener {
	friend class MessagerServer;
public:
	ServerListener(MessagerServer* srv);
	virtual ~ServerListener();
	int initialize();
	int startlisten();
	bool going_stop();
	void going_stop(bool b);
	bool has_stopped();

	bool exists(const string& username);

protected:
	/**
	 * handle new connection from peer
	 * return: the number of request handled
	 */
	int handle_new_peer();
	int handle_peer_select(fd_set& rset, int msgcnt);

	int handle_peer_msg(int sockfd, const std::string& msg);
	int handle_register(int sockfd, json& msg);
	int handle_login(int sockfd, json& msg);
	int handle_logout(int sockfd, json& msg);
	int handle_list_friends(int sockfd, json& msg);
	int handle_show_all_users(int sockfd, json& msg);
	int handle_invite(int sockfd, json& msg);
	int handle_invite_accept(int sockfd, json& msg);

	size_t read_one_packet(int sockfd, std::string& buf);

	size_t maxPeerSize();
	void erasePeer(int sockfd);
	bool has_peer(int ip, unsigned short port);
	int get_peer_fd(int ip, unsigned short port);

	json friends_to_json(string name);
	int notify_users(const string& username, bool activate, int ip, int port);
	int notify_invitation(const string& username, int sockfd);

	int notify_invite(const string& username, const string& friendname,
			const string& message);
	int notify_invite_accept(const string& username, const string& friendname,
			const string& message);
	int notify_new_friend(const string& username, const string& friendname);
	void add_invite(const string& username, const string& friendname,
			const string& message) {
		InviteInfo i;
		i.username = username;
		i.friendname = friendname;
		i.message = message;
		invites.insert(i);
	}

	void remove_invite(const string& username, const string& friendname) {
		InviteInfo i;
		i.username = username;
		i.friendname = friendname;
		invites.erase(i);

	}
	bool has_invite(const string& username, const string& friendname) {
		InviteInfo i;
		i.username = username;
		i.friendname = friendname;
		return invites.count(i) > 0;

	}

	vector<string> get_invitation(const string& friendname) {
		vector<string> ret;
		for (auto& x : invites) {
			if (x.friendname == friendname) {
				ret.push_back(x.username);
			}
		}
		return ret;
	}

	int _after_login(const string& username, int sockfd, int ip, int port);

protected:

	MessagerServer* messageServer;

	int listenfd;

	fd_set fdset;

	bool bStop;
	bool hasStoped;

	int maxfd;

	std::map<int, ClientInfo> clients;
	std::set<InviteInfo> invites;
	int hostip;
};

#endif /* LISTENER_H_ */

