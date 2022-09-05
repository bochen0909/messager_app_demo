/*
 * UserInfo.h
 *
 *  Created on: Nov 20, 2019
 */

#ifndef USERINFO_H_
#define USERINFO_H_
#include <string>
#include <vector>
#include "json.hpp"
using json = nlohmann::json;
using namespace std;

typedef struct _UserInfo {
public:
	string username;
	string password;
	bool activate;
	vector<string> contacts;
	int port;
	int ip;
	int sockfd;
	_UserInfo();
	json to_json() const;

	static _UserInfo parse(json& j);

} UserInfo;

#endif /* USERINFO_H_ */
