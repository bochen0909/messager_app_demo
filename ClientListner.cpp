/*
 * ClientListner.cpp
 *
 *  Created on: Nov 20, 2019
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
#include "conf.h"
#include "ClientListner.h"
#include "MessageClient.h"
#include "HttpMessage.h"
#include "UserInfo.h"
#define	SA	struct sockaddr

std::string ClientInfo::toString() {
	char tmp[MAXLINE];
	sprintf(tmp, "(0x%x) %s:%d, sockfd=%d", ip, Util::itoaddress(ip).c_str(),
			port, sockfd);
	return tmp;
}
std::string ClientInfo::address() {
	return Util::itoaddress(ip);
}

ClientListner::ClientListner(MessageClient* client) :
		listenfd(0), hostip(0), bStop(false), hasStoped(true), maxfd(0), serverSockfd(
				0), logged_in(false) {
	this->client = client;
}

ClientListner::~ClientListner() {
	if (listenfd > 0)
		close(listenfd);
}

int ClientListner::initialize() {
	int port = client->clientPort;

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
			Util::log_info("Bind client socket failed at port %d, errno=%d\n",
					port, errno);
			return -1;
		}

		if (listen(listenfd, LISTENQ) < 0) {
			Util::log_info("Listen client socket failed at port %d, errno=%d\n",
					port, errno);
			return -1;
		}

		this->listenfd = listenfd;
		Util::log_info("Bind client socket at port %d\n", port);
	}

	this->hostip = Util::getLocalIP();
	Util::log_info("client listening on %s:%d\n",
			Util::itoaddress(hostip).c_str(), port);

	this->maxfd = listenfd;
	FD_ZERO(&fdset);
	FD_SET(this->listenfd, &fdset);

	return 0;
}

bool ClientListner::has_stopped() {
	return hasStoped;
}

bool ClientListner::going_stop() {
	return bStop;
}

void ClientListner::going_stop(bool b) {
	bStop = b;
}

int ClientListner::connect_peer(int ip, unsigned short port, bool is_server,
		const string& name) {
	int sockfd;
	struct sockaddr_in servaddr;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		Util::log_error("socket() failed\n");
		return -1;
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = htonl(ip);

	if (connect(sockfd, (SA *) &servaddr, sizeof(servaddr)) < 0) {
		Util::log_error("Connect to %s:%d failed\n",
				Util::itoaddress(ip).c_str(), port);
		close(sockfd);
		return -1;
	} else
		Util::log_debug("DEBUG: Connected to %s:%d\n",
				Util::itoaddress(ip).c_str(), port);

	const char* buf = "Messenger CONNECT/0.1\r\n";
	if (Util::writen(sockfd, buf) < 0) {
		Util::log_error("Send CONNECT msg failed.\n");
		close(sockfd);
		return -1;
	} else {
		Util::log_debug("DEBUG: Send CONNECT msg to %s:%d\n",
				Util::itoaddress(ip).c_str(), port);
	}
	std::string reply;
	if (Util::readed(sockfd, reply) <= 0) {
		Util::log_debug("DEBUG: Recv CONNECT reply msg failed.\n");
		close(sockfd);
		return -1;
	} else
		Util::log_debug("DEBUG: Get reply from  %s:%d:%s\n",
				Util::itoaddress(ip).c_str(), port, reply.c_str());

	if (reply != "Messenger OK\n\n" && reply != "Messenger OK\r\n") {
		Util::log_debug("DEBUG: Recv unknown CONNECT reply msg %s.\n",
				reply.c_str());
		close(sockfd);
		return -1;
	}

	if (is_server) {
		this->serverSockfd = sockfd;
	}
	ClientInfo pinfo;
	pinfo.sockfd = sockfd;
	pinfo.ip = ip;
	pinfo.port = port;
	pinfo.ip = ntohl(servaddr.sin_addr.s_addr);
	pinfo.port = ntohs(servaddr.sin_port);
	pinfo.is_server = is_server;
	pinfo.name = name;
	clients[sockfd] = pinfo;
	maxfd = std::max(maxfd, sockfd);
	FD_SET(sockfd, &this->fdset);
	Util::log_debug("DEBUG:  Connect to %s:%d successfully.\n",
			Util::itoaddress(ip).c_str(), port);
	return 0;

}

int ClientListner::startlisten() {
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
			//this->client->client_rl_crlf();

		} else { //timeout
			//Util::log_debug("no message\n");
		}

	}

	if (this->logged_in) {
		this->com_logout();
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

int ClientListner::handle_peer_select(fd_set& rset, int msgcnt) {
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
				Util::log_info("\n");
				this->handle_peer_msg(sockfd, buf);
			}
		}
	}
	return nready;
}

void ClientListner::erasePeer(int sockfd) {
	ClientInfo &pi = this->clients[sockfd];
	Util::log_debug("Remove peer:%s\n", pi.toString().c_str());
	FD_CLR(sockfd, &this->fdset);
	clients.erase(sockfd);
	close(sockfd);

	if (this->serverSockfd > 0 and sockfd == this->serverSockfd) {
		Util::log_info("Server is lost! going to quit...\n");
		//this->going_stop(true);
		this->client->going_stop(true);
	}
}

size_t ClientListner::read_one_packet(int sockfd, std::string& buf) {
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

int ClientListner::handle_peer_msg(int sockfd, const std::string& msgtext) {

	Util::log_debug("DEBUG: recv %s\n", Util::escape_cpp(msgtext).c_str());

	HttpMessage msg;
	msg.parseFromString(msgtext);

	json j = json::parse(msg.msg);

	if (j["command"] == "on_user_state") {
		this->handle_on_user_state(sockfd, j);
	} else if (j["command"] == "on_invite") {
		this->handle_on_invite(sockfd, j);
	} else if (j["command"] == "on_invite_accept") {
		this->handle_on_invite_accept(sockfd, j);
	} else if (j["command"] == "on_show_invitation") {
		this->handle_on_show_invitation(sockfd, j);
	} else if (j["command"] == "m") {
		this->handle_chat_message(sockfd, j);
	} else {
		Util::log_error("Unknown Message command:%s\n",
				string(j["commnad"]).c_str());
	}

	return 0;
}

int ClientListner::handle_on_show_invitation(int sockfd, json& msg) {

	auto& invites = msg["invites"];
	for (const string& username : invites) {
		Util::log_info("Server>> %s invited you as friend.\n",
				username.c_str());
	}
	return 0;
}

int ClientListner::handle_chat_message(int sockfd, json& msg) {

	const string& message = msg["message"];
	const string& username = msg["username"];
	Util::log_info("%s>> %s\n", username.c_str(), message.c_str());
	return 0;
}

int ClientListner::handle_on_invite(int sockfd, json& msg) {
	string username = msg["username"];
	string message = msg["message"];
	if (message.empty()) {
		Util::log_info("Server>> %s invited you as a  friend.\n",
				username.c_str());
	} else {
		Util::log_info("Server>> %s invited you as a friend and said `%s`.\n",
				username.c_str(), message.c_str());
	}
	return 0;
}

int ClientListner::handle_on_invite_accept(int sockfd, json& msg) {
	string invitename = msg["invitename"];
	string message = msg["message"];
	json& j = msg["friendinfo"];
	this->update_friends_json(j);
	if (message.empty()) {

		Util::log_info("Server>> %s accepted your invitation.\n",
				invitename.c_str());
	} else {
		Util::log_info("Server>> %s accepted your invitation and said `%s`.\n",
				invitename.c_str(), message.c_str());
	}
	return 0;

}

int ClientListner::handle_on_user_state(int sockfd, json& msg) {

	string username = msg["username"];
	bool activate = msg["activate"];
	int ip = msg["ip"];
	int port = msg["port"];

	this->activate(username, activate, ip, port);

	Util::log_info("Server>> %s %s\n", username.c_str(),
			activate ? "online" : "offline");
	return 0;
}

size_t ClientListner::maxPeerSize() {
	unsigned int maxsize = (FD_SETSIZE - 10) / 100 * 100;
	return maxsize;
}

int ClientListner::handle_new_peer() {
	struct sockaddr_in cliaddr;
	socklen_t clilen = sizeof(cliaddr);
	int connfd = accept(listenfd, (SA *) &cliaddr, &clilen);

	char str[INET6_ADDRSTRLEN];
	Util::log_debug("DEBUG: new client: %s, port %d\n",
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

int ClientListner::show_friends(const string& msg) {
	json j = json::parse(msg);
	if (j.is_null() || j.empty()) {
		Util::log_info("no friends.\n");
	} else {
		Util::log_info("%d friends:\n", j.size());
		// special iterator member functions for objects
		for (json::iterator it = j.begin(); it != j.end(); ++it) {
			auto &j2 = it.value();
			Util::log_info("%-20s %-10s %-20s %-10d\n",
					string(j2["username"]).c_str(),
					j2["activate"] ? "online" : "offline",
					Util::itoaddress(j2["ip"]).c_str(), (int) j2["port"]);
		}
	}
	return 0;
}

int ClientListner::show_users(const string& msg) {
	json j = json::parse(msg);
	if (j.is_null() || j.empty()) {
		Util::log_info("no users.\n");
	} else {
		Util::log_info("%d users:\n", j.size());
		// special iterator member functions for objects
		for (json::iterator it = j.begin(); it != j.end(); ++it) {
			auto &j2 = it.value();
			Util::log_info("%-20s %-10s %-20s %-10d\n",
					string(j2["username"]).c_str(),
					j2["activate"] ? "online" : "offline",
					Util::itoaddress(j2["ip"]).c_str(), (int) j2["port"]);
		}
	}
	return 0;
}

int ClientListner::update_friends(const string& msg) {
	json j = json::parse(msg);
	this->update_friends_json(j);
	return 0;
}

int ClientListner::update_friends_json(json& j) {
	if (j.is_null() || j.empty()) {

	} else {
		for (json::iterator it = j.begin(); it != j.end(); ++it) {
			auto &j2 = it.value();
			auto &username = j2["username"];
			UserInfo user = UserInfo::parse(j2);
			this->friends[username] = user;
		}
	}
	return 0;
}

int ClientListner::com_show_all_users() {

	if (this->current_user.empty() || !this->logged_in) {
		Util::log_error("Error! not login yet.\n");
		return -1;
	}

	HttpMessage resp;
	json j;
	j["command"] = "query";

	resp.set_message(j.dump());
	int sockfd = this->serverSockfd;
	string textmsg = resp.toString();
	Util::log_debug("DEBUG: sending %s\n", Util::escape_cpp(textmsg).c_str());
	ssize_t nwriten = Util::writen(sockfd, textmsg);
	if (nwriten < 0) {
		Util::log_error("reply message failed\n");
		return -1;
	} else {
		Util::log_debug("DEBUG: Finished lf request.\n");
	}

	std::string buf;
	size_t n = read_one_packet(sockfd, buf);
	if (n < 0) {
		Util::log_error("Read reply failed for server: %s\n", buf.c_str());
		return -1;
	} else if (n == 0) {
		Util::log_error("server closed: %s\n", buf.c_str());
		//erasePeer(sockfd);
		return -1;
	} else {
		Util::log_debug("DEBUG: recv reply %s\n",
				Util::escape_cpp(buf).c_str());

		HttpMessage msg;
		msg.parseFromString(buf);
		if (string(msg.retcode) == "200") {
			this->show_users(msg.msg);
		} else {
			Util::log_info("Server>> Failed: %s\n", msg.retdesc);
		}
		return 0;
	}

	return 0;

}

int ClientListner::com_list_friend() {

	if (this->current_user.empty() || !this->logged_in) {
		Util::log_error("Error! not login yet.\n");
		return -1;
	}

	HttpMessage resp;
	json j;
	j["command"] = "lf";
	j["username"] = current_user;

	resp.set_message(j.dump());
	int sockfd = this->serverSockfd;
	string textmsg = resp.toString();
	Util::log_debug("DEBUG: sending %s\n", Util::escape_cpp(textmsg).c_str());
	ssize_t nwriten = Util::writen(sockfd, textmsg);
	if (nwriten < 0) {
		Util::log_error("reply message failed\n");
		return -1;
	} else {
		Util::log_debug("DEBUG: Finished lf request.\n");
	}

	std::string buf;
	size_t n = read_one_packet(sockfd, buf);
	if (n < 0) {
		Util::log_error("Read reply failed for server: %s\n", buf.c_str());
		return -1;
	} else if (n == 0) {
		Util::log_error("server closed: %s\n", buf.c_str());
		//erasePeer(sockfd);
		return -1;
	} else {
		Util::log_debug("DEBUG: recv reply %s\n",
				Util::escape_cpp(buf).c_str());

		HttpMessage msg;
		msg.parseFromString(buf);
		if (string(msg.retcode) == "200") {
			this->show_friends(msg.msg);
			//this->update_friends(msg.msg);

		} else {
			Util::log_info("Server>> Failed: %s\n", msg.retdesc);
		}
		return 0;
	}

	return 0;

}
int ClientListner::_make_chat_connection(const string& name, int ip, int port) {
	int clientfd = this->find_client_fd(name);
	if (clientfd > 0) {
		return clientfd;
	}

	bool is_server = false;

	this->connect_peer(ip, port, is_server, name);
	clientfd = this->find_client_fd(name);
	if (clientfd > 0) {
		return clientfd;
	} else {
		return -1;
	}

}
int ClientListner::com_chat(const string& name, const string &message) {
	if (this->current_user.empty() || !this->logged_in) {
		Util::log_error("Error! not login yet.\n");
		return -1;
	}

	if (!this->is_friend(name)) {
		Util::log_error("%s is not your friend.\n", name.c_str());
		return -1;
	}

	auto& info = this->get_friend(name);

	if (!info.activate) {
		Util::log_error("%s is not online.\n", name.c_str());
		return -1;
	}

	int sockfd = this->_make_chat_connection(name, info.ip, info.port);

	if (sockfd < 0) {
		return -1;
	}

	HttpMessage resp;
	json j;
	j["command"] = "m";
	j["username"] = current_user;
	j["message"] = message;

	resp.set_message(j.dump());
	string textmsg = resp.toString();
	Util::log_debug("DEBUG: sending %s\n", Util::escape_cpp(textmsg).c_str());
	ssize_t nwriten = Util::writen(sockfd, textmsg);
	if (nwriten < 0) {
		Util::log_error("sending message failed\n");
		return -1;
	} else {
		Util::log_debug("DEBUG: Finished sending chat message.\n");
	}
	return 0;

}
int ClientListner::com_invite(const string& name, const string &message) {
	if (this->current_user.empty() || !this->logged_in) {
		Util::log_error("Error! not login yet.\n");
		return -1;
	}

	if (current_user == name) {
		Util::log_error("cannot invite yourself.\n");
		return -1;
	}
	HttpMessage resp;
	json j;
	j["command"] = "i";
	j["username"] = current_user;
	j["invitename"] = name;
	j["message"] = message;

	resp.set_message(j.dump());
	int sockfd = this->serverSockfd;
	string textmsg = resp.toString();
	Util::log_debug("DEBUG: sending %s\n", Util::escape_cpp(textmsg).c_str());
	ssize_t nwriten = Util::writen(sockfd, textmsg);
	if (nwriten < 0) {
		Util::log_error("sending message failed\n");
		return -1;
	} else {
		Util::log_debug("DEBUG: Finished invite request.\n");
	}

	std::string buf;
	size_t n = read_one_packet(sockfd, buf);
	if (n < 0) {
		Util::log_error("Read reply failed for server: %s\n", buf.c_str());
		return -1;
	} else if (n == 0) {
		Util::log_error("server closed: %s\n", buf.c_str());
		//erasePeer(sockfd);
		return -1;
	} else {
		Util::log_debug("DEBUG: recv reply %s\n",
				Util::escape_cpp(buf).c_str());

		HttpMessage msg;
		msg.parseFromString(buf);
		Util::log_info("Server>> %s\n", msg.retdesc);
		return 0;
	}

	return 0;
}
int ClientListner::com_invite_accept(const string& name,
		const string &message) {
	if (this->current_user.empty() || !this->logged_in) {
		Util::log_error("Error! not login yet.\n");
		return -1;
	}
	HttpMessage resp;
	json j;
	j["command"] = "ia";
	j["username"] = current_user;
	j["invitename"] = name;
	j["message"] = message;

	resp.set_message(j.dump());
	int sockfd = this->serverSockfd;
	string textmsg = resp.toString();
	Util::log_debug("DEBUG: sending %s\n", Util::escape_cpp(textmsg).c_str());
	ssize_t nwriten = Util::writen(sockfd, textmsg);
	if (nwriten < 0) {
		Util::log_error("sending message failed\n");
		return -1;
	} else {
		Util::log_debug("DEBUG: Finished invite accept request.\n");
	}

	std::string buf;
	size_t n = read_one_packet(sockfd, buf);
	if (n < 0) {
		Util::log_error("Read reply failed for server: %s\n", buf.c_str());
		return -1;
	} else if (n == 0) {
		Util::log_error("server closed: %s\n", buf.c_str());
		//erasePeer(sockfd);
		return -1;
	} else {
		Util::log_debug("DEBUG: recv reply %s\n",
				Util::escape_cpp(buf).c_str());

		HttpMessage msg;
		msg.parseFromString(buf);
		Util::log_info("Server>> %s\n", msg.retdesc);

		json j = json::parse(msg.msg);
		this->update_friends_json(j["friendinfo"]);
		return 0;
	}

	return 0;
}
int ClientListner::clean_all_clients() {
	std::vector<int> fds;
	for (auto& c : clients) {
		if (!c.second.is_server) {
			fds.push_back(c.first);
		}
	}
	for (int fd : fds) {
		this->erasePeer(fd);
	}
	return 0;
}
int ClientListner::com_logout() {
	if (this->current_user.empty() || !this->logged_in) {
		Util::log_error("Error! not login yet.\n");
		return -1;
	}
	HttpMessage resp;
	json j;
	j["command"] = "logout";
	j["username"] = current_user;

	resp.set_message(j.dump());
	int sockfd = this->serverSockfd;
	string textmsg = resp.toString();
	Util::log_debug("DEBUG: sending %s\n", Util::escape_cpp(textmsg).c_str());
	ssize_t nwriten = Util::writen(sockfd, textmsg);
	if (nwriten < 0) {
		Util::log_error("reply message failed\n");
		return -1;
	} else {
		Util::log_debug("DEBUG: Finished logout request.\n");
	}
	this->current_user = "";
	this->logged_in = false;
	this->clean_all_clients();
	this->client->set_prompt("Messenger: ");
	return 0;

}

int ClientListner::com_register(const string& name, const string &password) {
	if (this->logged_in) {
		Util::log_error("User %s is logged in. logout first.\n",
				current_user.c_str());
		return -1;
	}

	HttpMessage resp;
	json j;
	j["command"] = "r";
	j["username"] = name;
	j["password"] = password;
	j["ip"] = (Util::getLocalIP());
	j["port"] = (client->clientPort);

	resp.set_message(j.dump());
	int sockfd = this->serverSockfd;
	string textmsg = resp.toString();
	Util::log_debug("DEBUG: sending %s\n", Util::escape_cpp(textmsg).c_str());
	ssize_t nwriten = Util::writen(sockfd, textmsg);
	if (nwriten < 0) {
		Util::log_error("reply message failed\n");
		return -1;
	} else {
		Util::log_debug("DEBUG: Finished register request.\n");
	}

	std::string buf;
	size_t n = read_one_packet(sockfd, buf);
	if (n < 0) {
		Util::log_error("Read reply failed for server: %s\n", buf.c_str());
		return -1;
	} else if (n == 0) {
		Util::log_error("server closed: %s\n", buf.c_str());
		//erasePeer(sockfd);
		return -1;
	} else {
		Util::log_debug("DEBUG: recv reply %s\n",
				Util::escape_cpp(buf).c_str());

		HttpMessage msg;
		msg.parseFromString(buf);
		if (string(msg.retcode) == "200") {
			Util::log_info("Server>> register succeeds\n");
			this->current_user = name;
			this->logged_in = true;
			this->show_friends(msg.msg);
			this->update_friends(msg.msg);
			this->client->set_prompt(name + ": ");

		} else {
			Util::log_info("Server>> Failed: %s\n", msg.retdesc);
		}
		return 0;
	}

	return 0;

}

int ClientListner::com_login(const string& name, const string &password) {
	if (this->logged_in) {
		Util::log_error("User %s is currently logged in. logout first.\n",
				current_user.c_str());
		return -1;
	}

	HttpMessage resp;
	json j;
	j["command"] = "l";
	j["username"] = name;
	j["password"] = password;
	j["ip"] = (Util::getLocalIP());
	j["port"] = (client->clientPort);

	resp.set_message(j.dump());
	int sockfd = this->serverSockfd;
	string textmsg = resp.toString();
	Util::log_debug("DEBUG: sending %s\n", Util::escape_cpp(textmsg).c_str());
	ssize_t nwriten = Util::writen(sockfd, textmsg);
	if (nwriten < 0) {
		Util::log_error("reply message failed\n");
		return -1;
	} else {
		Util::log_debug("DEBUG: Finished login request.\n");
	}

	std::string buf;
	size_t n = read_one_packet(sockfd, buf);
	if (n < 0) {
		Util::log_error("Read reply failed for server: %s\n", buf.c_str());
		return -1;
	} else if (n == 0) {
		Util::log_error("server closed: %s\n", buf.c_str());
		//erasePeer(sockfd);
		return -1;
	} else {
		Util::log_debug("DEBUG: recv reply %s\n",
				Util::escape_cpp(buf).c_str());

		HttpMessage msg;
		msg.parseFromString(buf);
		if (string(msg.retcode) == "200") {
			Util::log_info("Server>> login succeeds\n");
			this->current_user = name;
			this->logged_in = true;
			this->show_friends(msg.msg);
			this->update_friends(msg.msg);
			this->client->set_prompt(name + ": ");
		} else {
			Util::log_info("Server>> Failed: %s\n", msg.retdesc);
		}
		return 0;
	}

	return 0;

}

