/*
 * HttpMessage.h
 *
 *  Created on: Nov 20, 2019
 */

#ifndef HTTPMESSAGE_H_
#define HTTPMESSAGE_H_
#include <string>
using namespace std;

struct HttpMessage {
	char httpversion[32]; //1.0
	char retcode[32]; //e.g. 200,404
	char retdesc[256]; //e.g. OK
	char servername[256];
	char contenttype[256];
	size_t contentlength;
	string msg;
	HttpMessage();
	void set_message(const string& msg);
	std::string toString();
	int parseFromString(const std::string& fulls);
};

#endif /* HTTPMESSAGE_H_ */
