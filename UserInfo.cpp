/*
 * UserInfo.cpp
 *
 *  Created on: Nov 20, 2019
 */

#include <string>
#include "UserInfo.h"

_UserInfo::_UserInfo() :
		activate(false), port(0), ip(0), sockfd(0) {
}

json _UserInfo::to_json() const {
	json j;
	j["username"] = username;
	j["activate"] = activate;
	j["ip"] = (ip);
	j["port"] = (port);
	return j;
}

_UserInfo _UserInfo::parse(json& j) {
	_UserInfo user;
	user.username = j["username"];
	user.activate = j["activate"];
	user.ip = j["ip"];
	user.port = j["port"];
	return user;
}
