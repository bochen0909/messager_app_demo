/*
 * MessagerServer.cpp
 *
 *  Created on: Nov 18, 2019
 */

#include "MessagerServer.h"
#include "Util.h"
#include "stdlib.h"
#include "stdio.h"
#include <signal.h>
#include "ServerListener.h"

MessagerServer* MessagerServer::m_instance = (MessagerServer*) NULL;

MessagerServer::MessagerServer(string user_info_file, string configuration_file) :
		serverPort(5656), listener(0) {
	this->user_info_file = user_info_file;
	this->configuration_file = configuration_file;
}

MessagerServer::~MessagerServer() {
	if (listener) {
		delete listener;
	}
}

static volatile int keepRunning = 1;

void intHandler(int dummy) {
	keepRunning = 0;
}

int MessagerServer::run() {
	if (this->parse_configuration_file() < 0) {
		return -1;
	}
	if (this->parse_userinfo_file() < 0) {
		return -1;
	}

	signal(SIGINT, intHandler);

	this->startListner();

	/* Loop reading and executing lines until the user quits. */
	for (; keepRunning;) {
		usleep(200);
	}

	/* Wait for all threads to complete */
	Util::log_info("Going to shutdown....\n");
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
		fprintf(stderr, "Stoping some threads timeout. Kill them.\n");
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
	ServerListener* listener = (ServerListener*) arg;
	listener->startlisten();
	pthread_exit(NULL);
}

int MessagerServer::startListner() {
	this->listener = new ServerListener(this);
	if (listener->initialize() < 0) {
		return -1;
	}

	pthread_t thread;
	pthread_attr_t attr;
	/* For portability, explicitly create threads in a joinable state */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	pthread_create(&thread, &attr, listener_thread_run,
			(void *) this->listener);
	pthread_attr_destroy(&attr);
	this->threads.push_back(thread);
	Util::log_info("Finish starting listener thread tid=%d\n", thread);
	return 0;
}

std::string MessagerServer::get_config(const std::string& varname) {
	if (config.find(varname) != config.end()) {
		return config[varname];
	} else {
		return "";
	}
}

int MessagerServer::parse_configuration_file() {
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
			Util::log_error("Warning! Ignore line in %s[%d]: `%s`\n", filename,
					vec.size(), str.c_str());
			continue;
		}

		std::string name = Util::trim(vec[0]);
		std::string value = Util::trim(vec[1]);
		this->config[name] = value;
		Util::log_debug("DEBUG: config: '%s' -> '%s'\n", name.c_str(),
				value.c_str());
	}

	std::string tmpstr;
	tmpstr = this->get_config("port");
	if (tmpstr.empty()) {
		Util::log_error("Error! port is not set.\n");
		return -1;
	} else {
		this->serverPort = atoi(tmpstr.c_str());
	}

	return 0;

}
int MessagerServer::parse_userinfo_file() {
	const char* filename = this->user_info_file.c_str();
	if (!Util::file_exists(filename)) {
		Util::log_error("Error! File %s does not exists\n", filename);
		return -1;
	}
	std::vector<std::string> lines = Util::readlines(filename);
	for (size_t i = 0; i < lines.size(); i++) {
		std::string str = Util::trim(lines[i]);
		if (str.empty())
			continue;
		std::vector<std::string> vec = Util::splitByDeli2(str, '|');
		if (vec.size() != 3 && vec.size() != 2) {
			Util::log_error("Warning! Ignore line in %s:[%d], %s\n", filename,
					vec.size(), str.c_str());
			continue;
		}

		std::string username = Util::trim(vec[0]);
		std::string password = (vec[1]);

		UserInfo info;
		info.username = username;
		info.password = password;

		Util::log_debug("DEBUG: the %dth user\n", i);
		Util::log_debug("DEBUG: username: ''%s'\n", username.c_str());
		Util::log_debug("DEBUG: password: ''%s'\n", username.c_str());

		if (vec.size() == 3) {

			std::string contactlist = (vec[2]);
			std::vector<std::string> vec2 = Util::splitByDeli2(contactlist,
					';');
			for (size_t j = 0; j < vec2.size(); j++) {
				string contact = Util::trim(vec2.at(j));
				if (contact.size() > 0) {
					Util::log_debug("DEBUG: contact: ''%s'\n", contact.c_str());
					info.contacts.push_back(contact);
				}
			}
		}

		this->userinfo[username] = info;

	}

	Util::log_info("There are %d users loaded\n", userinfo.size());
	return 0;
}

int MessagerServer::write_userinfo_file() {

	const char* filename = this->user_info_file.c_str();
	Util::log_info("Writing %s\n", filename);
	FILE *f = fopen(filename, "w");
	if (f == NULL) {
		Util::log_error("Error opening file: {}\n", filename);
		return -1;
	}

	for (auto const& info : this->userinfo) {
		const UserInfo& x = info.second;
		fprintf(f, "%s|%s|", x.username.c_str(), x.password.c_str());
		for (size_t i = 0; i < x.contacts.size(); i++) {
			if (i > 0) {
				fprintf(f, ";");
			}
			fprintf(f, "%s", x.contacts[i].c_str());
		}
		fprintf(f, "\n");

	}
	fclose(f);

	return 0;
}
