/*
 * HttpMessage.cpp
 *
 *  Created on: Nov 20, 2019
 */

#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <stdio.h>
#include "Util.h"
#include "conf.h"

#include "HttpMessage.h"

HttpMessage::HttpMessage() {
//	bzero(this, sizeof(HttpMessage));
	strcpy(httpversion, "1.0");
	strcpy(retcode, "200");
	strcpy(retdesc, "OK");
	strcpy(servername, "Messenger/0.1");
	strcpy(contenttype, "application/json");
	set_message("{}");
}

void HttpMessage::set_message(const string& msg) {
	this->msg = msg;
	contentlength = msg.size();
}

std::string HttpMessage::toString() {
	std::string ret = "";
	char tmpstr[MAXLINE];

	bzero(tmpstr, MAXLINE);
	sprintf(tmpstr, "HTTP/%s %s %s\r\n", httpversion, retcode, retdesc);
	ret += tmpstr;

	bzero(tmpstr, MAXLINE);
	sprintf(tmpstr, "Server: %s\r\n", servername);
	ret += tmpstr;

	bzero(tmpstr, MAXLINE);
	sprintf(tmpstr, "Content-Type: %s\r\n", contenttype);
	ret += tmpstr;

	bzero(tmpstr, MAXLINE);
	sprintf(tmpstr, "Content-Length: %lu\r\n", contentlength);
	ret += tmpstr;

	ret += "\r\n";

	ret += msg;
	return ret;
}

int HttpMessage::parseFromString(const std::string& fulls) {
	size_t head_end_pos = -1;
	for (size_t i = 0; i < fulls.length(); i++) {
		if (i > 3 && fulls[i] == '\n' && fulls[i - 1] == '\r'
				&& fulls[i - 2] == '\n' && fulls[i - 3] == '\r') {
			head_end_pos = i;
			break;
		}
	}
	if (head_end_pos < 0) {
		Util::log_error("Parse message failed, \\r\\n\\r\\n not found.%s\n",
				fulls.c_str());
		return -1;
	}

	//bzero(this, sizeof(HttpMessage));

	std::string msg = fulls.substr(head_end_pos + 1);
	this->msg = msg;

	std::string s = fulls.substr(0, head_end_pos + 1);
	std::vector<std::string> vec;
	size_t start_pos = 0;
	for (size_t i = 0; i < s.length(); i++) {
		if (s[i] == '\n' && i > 0 && s[i - 1] == '\r') {
			std::string substr = s.substr(start_pos, i - 1 - start_pos);
			start_pos = i + 1;
			vec.push_back(substr);
		}
	}
	if (vec.size() != 5 || (vec.size() > 0 && !vec[vec.size() - 1].empty())) {
		Util::log_error("Parse message failed (split=%ld):%s\n", vec.size(),
				s.c_str());
		return -1;
	}

	int ncount = 0;
	if ((ncount = sscanf(vec[0].c_str(), "HTTP/%s %s %[0-9a-zA-Z ]",
			httpversion, retcode, retdesc)) != 3) {
		Util::log_error("Parse line1 failed(%d):%s\n", ncount, vec[0].c_str());
		return -1;
	}

	if ((ncount = sscanf(vec[1].c_str(), "Server: %s", servername)) != 1) {
		Util::log_error("Parse line2 failed(%d):%s\n", ncount, vec[1].c_str());
		return -1;
	}

	if ((ncount = sscanf(vec[2].c_str(), "Content-Type: %s", contenttype))
			!= 1) {
		Util::log_error("Parse line3 failed(%d):%s\n", ncount, vec[2].c_str());
		return -1;
	}

	if ((ncount = sscanf(vec[3].c_str(), "Content-Length: %lu", &contentlength))
			!= 1) {
		Util::log_error("Parse line4 failed(%d):%s\n", ncount, vec[3].c_str());
		return -1;
	}

	return 0;
}

