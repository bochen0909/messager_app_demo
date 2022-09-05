/*
 * Util.h
 *
 */

#ifndef MYUTIL_H_
#define MYUTIL_H_

#include <string>
#include <vector>
#include <stdarg.h>
#include <unistd.h>
/**
 * Utility methods class.
 * All methods are .
 * Actually put them in a name space is better.
 */
namespace Util {

std::string &ltrim(std::string &s);

std::string &rtrim(std::string &s);

// trim from both ends
std::string &trim(std::string &s);
inline std::string trim(char* s) {
	std::string ss = s;
	return trim(ss);
}

char * dupstr(char* s);
std::vector<std::string> split(const std::string &input);
std::vector<std::string> splitByDeli(const std::string &input, char deli);
std::vector<std::string> splitByDeli2(const std::string &input, char deli);
std::vector<std::string> splitcmdline(const std::string &input);
std::vector<std::string> splitcmdline3(const std::string &input);
void printvec(const std::vector<std::string>& vec);
bool startsWith(const std::string& s, const std::string& sub);

std::string getvar(const std::string& varname);
bool isExcutableIn(const ::std::string& executable, const std::string& dirname);
char** vec2argv(const std::vector<std::string>& vec);
void free_argv(char** argv);
bool isExecutable(const std::string& file);

ssize_t writen(int fd, const void *vptr, size_t n);
//ssize_t writen(int fd, const std::string& buf);
ssize_t writen(int fd, std::string buf);
ssize_t writen(int fd, const char* str);
ssize_t readed(int fd, std::string& buf);
ssize_t read_once(int fd, std::string& buf);
ssize_t read_once(int fd, std::string& buf, size_t length);

std::string vatostr2(const char *fmt, ...);
void log_error(const char *fmt, ...);
void log_info(const char *fmt, ...);
void log_debug(const char *fmt, ...);
std::string toHex(const std::string& s, bool upper_case = true);
std::string toUpper(const std::string& s);

bool file_exists(const char* filename);
size_t getFileSize(std::string fname);
std::vector<std::string> readlines(const char* filename);
unsigned char rand_lim(unsigned int limit);
std::string to_string(size_t n);

/**
 * return ip in host bytes order
 */
int host2ip(const std::string& hostname);
int getLocalIP();
std::string getLocalHostname();

/**
 * ip in host bytes order
 */
std::string itoaddress(int ip);

const char* staticMap(char c);

std::string escape_cpp(const std::string& input);
}

#endif /* UTIL_H_ */
