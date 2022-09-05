/*
 * ServerListener.cpp
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <stdexcept>
#include <typeinfo>
#include <string>
#include <algorithm>
#include <iostream>

#include "Util.h"
#include "ServerListener.h"
#include "MessagerServer.h"
#include "conf.h"
#include "HttpMessage.h"
#include "UserInfo.h"
using namespace std;

#define	SA	struct sockaddr

bool operator==(const InviteInfo& lhs, const InviteInfo& rhs) {
	return lhs.username < lhs.username && lhs.friendname < lhs.friendname;
}

std::string ClientInfo::toString() {
	char tmp[MAXLINE];
	sprintf(tmp, "(0x%x) %s:%d, sockfd=%d", ip, Util::itoaddress(ip).c_str(),
			port, sockfd);
	return tmp;
}
std::string ClientInfo::address() {
	return Util::itoaddress(ip);
}

ServerListener::ServerListener(MessagerServer* srv) :
		listenfd(-1), bStop(false), hasStoped(false), maxfd(-1), hostip(0) {
	this->messageServer = srv;

}

ServerListener::~ServerListener() {
	if (listenfd > 0)
		close(listenfd);
}

bool ServerListener::has_stopped() {
	return hasStoped;
}

bool ServerListener::exists(const string& username) {
	return messageServer->exists(username);
}

bool ServerListener::going_stop() {
	return bStop;
}

void ServerListener::going_stop(bool b) {
	bStop = b;
}

int ServerListener::startlisten() {
	while (!this->going_stop()) {
		fd_set rset;
		rset = this->fdset;
		/* Wait up to 2 seconds. */
		struct timeval tv;
		tv.tv_sec = 2;
		tv.tv_usec = 0;
		int nready = select(this->maxfd + 1, &rset, NULL,
		NULL, &tv);
		if (nready == -1) {
			Util::log_error("select returns error %d\n", nready);
		} else if (nready) {

			if (FD_ISSET(listenfd, &rset)) { /* new client connection */
				this->handle_new_peer();
				nready--;
			}

			if (nready <= 0) {
				continue;
			}

			this->handle_peer_select(rset, nready);

		} else { //timeout
			//Util::log_debug("no message\n");
		}
	}
	close(listenfd);

	for (std::map<int, ClientInfo>::iterator i = clients.begin();
			i != clients.end(); i++) {
		close(i->first);
	}
	hasStoped = true;
	Util::log_info("Listener stopped....\n");

	return 0;
}

int ServerListener::handle_peer_select(fd_set& rset, int msgcnt) {
	std::map<int, ClientInfo>::iterator iter;
	int nready = msgcnt;
	std::map<int, ClientInfo> tmpfds(clients);
	for (iter = tmpfds.begin(); iter != tmpfds.end(); iter++) {
		if (nready <= 0) {
			break;
		}
		int sockfd = iter->first;
		if (FD_ISSET(sockfd, &rset)) {
			nready--;
			std::string buf;
			size_t n = read_one_packet(sockfd, buf);
			if (n < 0) {
				Util::log_error("Read message failed for %s: %s\n", buf.c_str(),
						iter->second.toString().c_str());
			} else if (n == 0) {
				//peer closed
			} else {
				this->handle_peer_msg(sockfd, buf);
			}
		}
	}
	return nready;
}

size_t ServerListener::read_one_packet(int sockfd, std::string& buf) {
	size_t n;
	std::string buf1;
	n = Util::read_once(sockfd, buf1);
	if (n == 0) {
		this->erasePeer(sockfd);
		return 0;
	} else {
		buf = buf1;
		return buf.size();
	}
}

int ServerListener::handle_peer_msg(int sockfd, const std::string& msgtext) {
	Util::log_debug("DEBUG: recv %s\n", Util::escape_cpp(msgtext).c_str());

	HttpMessage msg;
	msg.parseFromString(msgtext);

	json j = json::parse(msg.msg);

	if (j["command"] == "r") {
		this->handle_register(sockfd, j);
	} else if (j["command"] == "l") {
		this->handle_login(sockfd, j);
	} else if (j["command"] == "i") {
		this->handle_invite(sockfd, j);
	} else if (j["command"] == "ia") {
		this->handle_invite_accept(sockfd, j);
	} else if (j["command"] == "lf") {
		this->handle_list_friends(sockfd, j);
	} else if (j["command"] == "query") {
		this->handle_show_all_users(sockfd, j);
	} else if (j["command"] == "logout") {
		this->handle_logout(sockfd, j);
	} else {
		Util::log_error("Unknown Message command:%s\n", string(j["commnad"]).c_str());
	}

	return 0;
}

json ServerListener::friends_to_json(string name) {

	if (name.empty()) {
		json j;
		for (const auto& kv : messageServer->userinfo) {
			j[kv.first] = kv.second.to_json();
		}
		return j;
	} else {
		json j;
		UserInfo& user = this->messageServer->getUser(name);
		for (string contact : user.contacts) {
			j[contact] = this->messageServer->getUser(contact).to_json();
		}
		return j;
	}
}

int ServerListener::notify_invitation(const string& username, int sockfd) {
	vector<string> vec = this->get_invitation(username);
	if (!vec.empty()) {
		HttpMessage resp;

		json j;
		j["command"] = "on_show_invitation";
		j["username"] = username;
		j["invites"] = vec;

		resp.set_message(j.dump());

		ssize_t nwriten = Util::writen(sockfd, resp.toString());
		if (nwriten < 0) {
			Util::log_error("notify invites for %s failed\n", username.c_str());
		} else {
			Util::log_debug("DEBUG: notify invites for %s \n",
					username.c_str());
		}
	}

	return 0;
}

int ServerListener::notify_users(const string& username, bool activate, int ip,
		int port) {
	vector<UserInfo> friends = this->messageServer->get_friends(username);
	HttpMessage resp;

	json j;
	j["command"] = "on_user_state";
	j["activate"] = activate;
	j["username"] = username;
	j["ip"] = ip;
	j["port"] = port;

	resp.set_message(j.dump());

	for (auto &fri : friends) {
		if (fri.activate && fri.sockfd > 0) {
			ssize_t nwriten = Util::writen(fri.sockfd, resp.toString());
			if (nwriten < 0) {
				Util::log_error("notify user %s failed\n",
						fri.username.c_str());
			} else {
				Util::log_debug("DEBUG: notify user %s \n",
						fri.username.c_str());
			}

		}
	}
	return 0;
}

int ServerListener::notify_new_friend(const string& username,
		const string& friendname) {
	auto& uinfo = this->messageServer->getUser(username);
	auto& finfo = this->messageServer->getUser(friendname);

	HttpMessage resp;

	json j;
	j["command"] = "on_new_friend";
	j["friendinfo"][finfo.username] = finfo.to_json();

	resp.set_message(j.dump());

	ssize_t nwriten = Util::writen(uinfo.sockfd, resp.toString());
	if (nwriten < 0) {
		Util::log_error("notify new friend %s failed\n",
				finfo.username.c_str());
	} else {
		Util::log_debug("DEBUG: notify new friend %s \n",
				finfo.username.c_str());
	}

	return 0;
}

int ServerListener::notify_invite(const string& username,
		const string& friendname, const string& message) {
	auto& finfo = this->messageServer->getUser(friendname);
	int sockfd = finfo.sockfd;

	HttpMessage resp;

	json j;
	j["command"] = "on_invite";
	j["username"] = username;
	j["message"] = message;

	resp.set_message(j.dump());

	ssize_t nwriten = Util::writen(sockfd, resp.toString());
	if (nwriten < 0) {
		Util::log_error("on_invite user %s failed\n", friendname.c_str());
	} else {
		Util::log_debug("DEBUG: on_invite user %s \n", friendname.c_str());
	}

	return 0;
}

int ServerListener::notify_invite_accept(const string& username,
		const string& friendname, const string& message) {
	auto& finfo = this->messageServer->getUser(username);
	auto& uinfo = this->messageServer->getUser(friendname);
	int sockfd = finfo.sockfd;

	HttpMessage resp;

	json j;
	j["command"] = "on_invite_accept";
	j["invitename"] = friendname;
	j["message"] = message;

	j["friendinfo"][uinfo.username] = uinfo.to_json();

	resp.set_message(j.dump());

	ssize_t nwriten = Util::writen(sockfd, resp.toString());
	if (nwriten < 0) {
		Util::log_error("on_invite_accept user %s failed\n",
				friendname.c_str());
	} else {
		Util::log_debug("DEBUG: on_invite_accept user %s \n",
				friendname.c_str());
	}

	return 0;
}

int ServerListener::handle_show_all_users(int sockfd, json& msg) {

	HttpMessage resp;

	resp.set_message(this->friends_to_json("").dump());

	ssize_t nwriten = Util::writen(sockfd, resp.toString());
	if (nwriten < 0) {
		Util::log_error("reply message failed\n");
		return -1;
	} else {
		Util::log_debug("DEBUG: Finished show all users request.\n");
		return 0;
	}
}

int ServerListener::handle_list_friends(int sockfd, json& msg) {
	string username = msg["username"];

	HttpMessage resp;

	if (!this->exists(username)) {
		strcpy(resp.retcode, "500");
		strcpy(resp.retdesc, "User does not exist");

	} else {
		strcpy(resp.retcode, "200");
		strcpy(resp.retdesc, "OK");

	}

	resp.set_message(this->friends_to_json(username).dump());

	ssize_t nwriten = Util::writen(sockfd, resp.toString());
	if (nwriten < 0) {
		Util::log_error("reply message failed\n");
		return -1;
	} else {
		Util::log_debug("DEBUG: Finished list user request.\n");
		return 0;
	}
}

int ServerListener::handle_logout(int sockfd, json& msg) {
	string username = msg["username"];
	this->messageServer->deactivate(username);
	Util::log_info("User %s logged out\n", username.c_str());
	this->notify_users(username, false, 0, 0);
	return 0;
}
int ServerListener::handle_invite(int sockfd, json& msg) {
	string username = msg["username"];
	string invitename = msg["invitename"];
	string message = msg["message"];

	HttpMessage resp;
	if (!this->exists(invitename)) {
		strcpy(resp.retcode, "500");
		strcpy(resp.retdesc,
				(string("User does not exist: ") + invitename).c_str());
	} else if (this->messageServer->is_friend(username, invitename)) {
		strcpy(resp.retcode, "500");
		strcpy(resp.retdesc,
				(invitename + string(" is already your friend.")).c_str());
	} else {
		if (this->messageServer->is_online(invitename)) {

			strcpy(resp.retcode, "200");
			sprintf(resp.retdesc, "The request has been sent.");
			this->add_invite(username, invitename, message);
			this->notify_invite(username, invitename, message);
		} else {
			strcpy(resp.retcode, "200");
			sprintf(resp.retdesc,
					"%s is not online the request has been added to queue.",
					invitename.c_str());

			this->add_invite(username, invitename, message);

		}
	}
	resp.set_message(this->friends_to_json(username).dump());

	ssize_t nwriten = Util::writen(sockfd, resp.toString());
	if (nwriten < 0) {
		Util::log_error("send message failed\n");
		return -1;
	} else {
		Util::log_debug("DEBUG: Finished invite request.\n");
		return 0;
	}
}

int ServerListener::handle_invite_accept(int sockfd, json& msg) {
	string username = msg["invitename"];
	string invitename = msg["username"];
	string message = msg["message"];

	if (!this->has_invite(username, invitename)) {
		HttpMessage resp;
		strcpy(resp.retcode, "500");
		strcpy(resp.retdesc, "no such invitation.");
		ssize_t nwriten = Util::writen(sockfd, resp.toString());
		if (nwriten < 0) {
			Util::log_error("send message failed\n");
			return -1;
		} else {
			Util::log_debug("DEBUG: Finished invite request.\n");
			return 0;
		}

	} else {

//		strcpy(resp.retcode, "200");
//		sprintf(resp.retdesc, "OK");
		this->remove_invite(username, invitename);

		this->messageServer->add_friends(username, invitename);
		this->messageServer->add_friends(invitename, username);
		this->messageServer->write_userinfo_file();

		if (this->messageServer->is_online(invitename)) {
			this->notify_invite_accept(username, invitename, message);
		}

		if (this->messageServer->is_online(invitename)) {
			this->notify_new_friend(invitename, username);
		}

		//resp.set_message(this->friends_to_json(username).dump());
	}
	return 0;
}

int ServerListener::_after_login(const string& username, int sockfd, int ip,
		int port) {
	this->notify_users(username, true, ip, port);
	this->notify_invitation(username, sockfd);
	return 0;
}

int ServerListener::handle_register(int sockfd, json& msg) {
	string username = msg["username"];
	string password = msg["password"];
	int ip = (msg["ip"]);
	int port = (msg["port"]);

	HttpMessage resp;
	bool bSucc = false;
	if (this->exists(username)) {
		strcpy(resp.retcode, "500");
		strcpy(resp.retdesc, "User exists");

	} else {
		strcpy(resp.retcode, "200");
		strcpy(resp.retdesc, "OK");
		bSucc = true;
		this->messageServer->add(username, password);
		this->messageServer->activate(username, ip, port, sockfd);

	}

	resp.set_message(this->friends_to_json(username).dump());

	ssize_t nwriten = Util::writen(sockfd, resp.toString());
	if (nwriten < 0) {
		Util::log_error("reply message failed\n");
		return -1;
	} else {
		Util::log_debug("DEBUG: Finished register request.\n");
		if (bSucc) {
			this->_after_login(username, sockfd, ip, port);
		}
		return 0;
	}
}

int ServerListener::handle_login(int sockfd, json& msg) {
	string username = msg["username"];
	string password = msg["password"];
	int ip = (msg["ip"]);
	int port = (msg["port"]);

	HttpMessage resp;
	bool bSucc = false;
	if (!this->exists(username)) {
		strcpy(resp.retcode, "500");
		strcpy(resp.retdesc, "User not exists");
	} else if (!this->messageServer->check(username, password)) {
		strcpy(resp.retcode, "500");
		strcpy(resp.retdesc, "Password not right");
	} else if (this->messageServer->is_online(username)) {
		strcpy(resp.retcode, "500");
		strcpy(resp.retdesc, "User already logged in.");
	} else {
		strcpy(resp.retcode, "200");
		strcpy(resp.retdesc, "OK");
		this->messageServer->activate(username, ip, port, sockfd);
		resp.set_message(this->friends_to_json(username).dump());
		bSucc = true;
	}

	ssize_t nwriten = Util::writen(sockfd, resp.toString());
	if (nwriten < 0) {
		Util::log_error("reply message failed\n");
		return -1;
	} else {
		Util::log_debug("DEBUG: Finished login request.\n");
		if (bSucc) {
			this->_after_login(username, sockfd, ip, port);
		}
		return 0;
	}
}

void ServerListener::erasePeer(int sockfd) {
	ClientInfo &pi = this->clients[sockfd];
	Util::log_debug("Remove peer:%s\n", pi.toString().c_str());
	FD_CLR(sockfd, &this->fdset);
	clients.erase(sockfd);
	close(sockfd);

	vector<string> names = this->messageServer->detected_unactivated_user(
			sockfd);
	for (string& name : names) {
		this->notify_users(name, false, 0, 0);
	}
}

bool ServerListener::has_peer(int ip, unsigned short port) {
	return get_peer_fd(ip, port) > 0;
}

int ServerListener::get_peer_fd(int ip, unsigned short port) {
	std::map<int, ClientInfo>::iterator iter;
	for (iter = clients.begin(); iter != clients.end(); iter++) {
		if (iter->second.ip == ip && iter->second.port == port) {
			return iter->first;
		}
	}
	return -1;
}

size_t ServerListener::maxPeerSize() {
	unsigned int maxsize = (FD_SETSIZE - 10) / 100 * 100;
	return maxsize;
}

int ServerListener::handle_new_peer() {
	struct sockaddr_in cliaddr;
	socklen_t clilen = sizeof(cliaddr);
	int connfd = accept(listenfd, (SA *) &cliaddr, &clilen);

	char str[INET6_ADDRSTRLEN];
	Util::log_debug("new client: %s, port %d\n",
			inet_ntop(AF_INET, &cliaddr.sin_addr, str, INET6_ADDRSTRLEN),
			ntohs(cliaddr.sin_port));

	if (this->clients.size() >= maxPeerSize()) {
		Util::log_error("too many clients, exceeds limits: %u\n",
				maxPeerSize());
		close(connfd);
		return -1; //ignore the request
	}

	std::string buf;
	int nread = Util::readed(connfd, buf);
	if (nread < 0) {
		Util::log_error("Error to read from peer.\n");
		close(connfd);
		return -1; //ignore the request
	}

	if (buf == "Messenger CONNECT/0.1\n\n"
			|| buf == "Messenger CONNECT/0.1\r\n") {
		ssize_t nwriten = Util::writen(connfd, "Messenger OK\r\n");
		if (nwriten > 0) {
			ClientInfo pinfo;
			pinfo.ip = ntohl(cliaddr.sin_addr.s_addr);
			pinfo.port = ntohs(cliaddr.sin_port);
			pinfo.sockfd = connfd;
			clients[connfd] = pinfo;
			FD_SET(connfd, &fdset); /* add new descriptor to set */
			maxfd = std::max(maxfd, connfd);
		} else {
			Util::log_error("reply Messenger OK to peer failed\n");
			close(connfd);
			return -1;
		}

	} else {
		Util::log_error("Unknown request from peer: %s\n", buf.c_str());
		close(connfd);
		return -1;
	}

	return 0;
}

int ServerListener::initialize() {
	int port = messageServer->serverPort;
	{
		int listenfd;
		struct sockaddr_in servaddr;
		listenfd = socket(AF_INET, SOCK_STREAM, 0);
		int enable = 1;
		setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

		bzero(&servaddr, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

		servaddr.sin_port = htons(port);

		if (bind(listenfd, (SA *) &servaddr, sizeof(servaddr)) < 0) {
			Util::log_info("Listen file socket failed at port %d, errno=%d\n",
					port, errno);
			return -1;
		}

		if (listen(listenfd, LISTENQ) < 0) {
			Util::log_info("Listen file socket failed at port %d, errno=%d\n",
					port, errno);
			return -1;
		}

		this->listenfd = listenfd;
		Util::log_info("Bind file socket at port %d\n", port);
	}

	this->hostip = Util::getLocalIP();
	Util::log_info("Using IP %s:%d or %s:%d for clients to connect\n",
			Util::getLocalHostname().c_str(), port,
			Util::itoaddress(hostip).c_str(), port);

	this->maxfd = listenfd;
	FD_ZERO(&fdset);
	FD_SET(this->listenfd, &fdset);
	return 0;
}

