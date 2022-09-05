/*
 * Util.cpp
 *
 */

#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>
#include <string.h>
#include <cstdlib>
#include<vector>
#include <string>
#include <sstream>
#include <iterator>
#include <iostream>
#include <fstream>
#include <iomanip>

#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <strings.h>

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include  <netdb.h>

#include "Util.h"
#define MAXLINE 4096
namespace Util {

// trim from start
std::string & ltrim(std::string &s) {
	s.erase(s.begin(),
			std::find_if(s.begin(), s.end(),
					std::not1(std::ptr_fun<int, int>(std::isspace))));
	return s;
}

// trim from end
std::string & rtrim(std::string &s) {
	s.erase(
			std::find_if(s.rbegin(), s.rend(),
					std::not1(std::ptr_fun<int, int>(std::isspace))).base(),
			s.end());
	return s;
}

// trim from both ends
std::string & trim(std::string &s) {
	return ltrim(rtrim(s));
}

char * dupstr(char* s) {
	char *r;
	r = (char*) malloc(strlen(s) + 1);
	bzero(r, strlen(s) + 1);
	strcpy(r, s);
	return (r);
}

std::vector<std::string> split(const std::string &input) {
	std::istringstream buffer(input);
	std::vector<std::string> ret((std::istream_iterator<std::string>(buffer)),
			std::istream_iterator<std::string>());
	return ret;
}

std::vector<std::string> splitByDeli(const std::string &rawinput, char deli) {
	std::string buffer;
	for (size_t i = 0; i < rawinput.length(); i++) {
		char c = rawinput.at(i);
		if (c == deli) {
			buffer += ' ';
			buffer += c;
			buffer += ' ';
		} else {
			buffer += c;
		}
	}
	return split(buffer);
}

std::vector<std::string> splitByDeli2(const std::string &rawinput, char deli) {
	std::string buffer;
	for (size_t i = 0; i < rawinput.length(); i++) {
		char c = rawinput.at(i);
		if (c == deli) {
			buffer += ' ';
		} else {
			buffer += c;
		}
	}
	return split(buffer);
}

std::vector<std::string> splitcmdline(const std::string &rawinput) {

	std::string buffer;
	for (size_t i = 0; i < rawinput.length(); i++) {
		char c = rawinput.at(i);
		if (c == '|' || c == '>' || c == '<' || c == '&') {
			buffer += ' ';
			buffer += c;
			buffer += ' ';
		} else {
			buffer += c;
		}
	}
	return split(buffer);
}

std::vector<std::string> splitcmdline3(const std::string &rawinput) {

	std::vector<std::string> ret;
	std::string buffer;
	for (size_t i = 0; i < rawinput.length(); i++) {
		char c = rawinput.at(i);
		if (isspace(c)) {
			buffer = trim(buffer);
			if (ret.size() < 2 && !buffer.empty()) {
				ret.push_back(buffer);
				buffer.clear();
			} else {
				buffer += c;
			}
		} else {
			buffer += c;
		}
	}
	buffer = trim(buffer);
	if (!buffer.empty()) {
		ret.push_back(buffer);
	}
	return ret;
}

bool startsWith(const std::string& s, const std::string& sub) {
	return s.find(sub) == 0;
}

void printvec(const std::vector<std::string>& vec) {
	for (size_t i = 0; i < vec.size(); i++) {
		printf("(%s)", vec.at(i).c_str());
	}
	printf("\n");
}

std::string getvar(const std::string& varname) {
	const char * val = ::getenv(varname.c_str());
	if (val == 0) {
		return "";
	} else {
		return val;
	}
}

bool isExcutableIn(const ::std::string& executable,
		const std::string& dirname) {
	DIR *d;
	struct dirent *dir;
	d = opendir(dirname.c_str());
	if (d) {
		while ((dir = readdir(d)) != NULL) {
			if (executable == dir->d_name) {
				std::string file = dirname + "/" + executable;
				if (isExecutable(file)) {
					return true;
				}
			}
		}
		closedir(d);
	}
	return false;
}

bool isExecutable(const std::string& file) {
	struct stat sb;
	if (stat(file.c_str(), &sb) != 0) {
		return false;
	}
	if (S_ISREG(sb.st_mode)) {
		return (sb.st_mode & S_IXUSR);
	}
	return false;
}

char** vec2argv(const std::vector<std::string>& vec) {
	size_t n = vec.size();
	char** argv = new char*[n + 1];
	for (size_t i = 0; i < n; i++) {
		argv[i] = dupstr((char*) vec[i].c_str());
	}
	argv[n] = (char*) 0;
	return argv;
}

void free_argv(char** argv) {
	char* p;
	int i = 0;
	while ((p = argv[i++])) {
		free(p);
	}
	delete argv;
}

ssize_t /* Write "n" bytes to a descriptor. */
writen(int fd, const void *vptr, size_t n) {
	size_t nleft;
	ssize_t nwritten;
	const char *ptr;

	ptr = (const char *) vptr;
	nleft = n;
	while (nleft > 0) {
		if ((nwritten = write(fd, ptr, nleft)) <= 0) {
			if (nwritten < 0 && errno == EINTR)
				nwritten = 0; /* and call write() again */
			else
				return (-1); /* error */
		}

		nleft -= nwritten;
		ptr += nwritten;
	}
	return (n);
}
/* end writen */

//ssize_t writen(int fd, const std::string& buf) {
//	return writen(fd, buf.c_str(), buf.length());
//}
ssize_t writen(int fd, std::string buf) {
	return writen(fd, buf.c_str(), buf.length());
}

ssize_t writen(int fd, const char* str) {
	return writen(fd, str, strlen(str));
}

ssize_t /* read as many as possible from a descriptor. */
readed(int fd, std::string& buf) {
	ssize_t nread;
	char tmpbuf[MAXLINE];
	buf.clear();
	while (true) {
		if ((nread = read(fd, tmpbuf, MAXLINE)) <= 0) {
			if (nread < 0 && errno == EINTR) {
				nread = 0; /* and call write() again */
				continue;
			} else if (nread == 0) {
				return 0;
			} else
				return (-1); /* error */
		}
		for (int i = 0; i < nread; i++) {
			buf.push_back((char) tmpbuf[i]);
		}
		if (nread < MAXLINE)
			break;
	}
	return buf.length();
}

ssize_t read_once(int fd, std::string& buf) {
	ssize_t nread;
	char tmpbuf[MAXLINE];
	buf.clear();
	if ((nread = read(fd, tmpbuf, MAXLINE)) <= 0) {
		if (nread < 0 && errno == EINTR) {
			nread = 0; /* and call write() again */
		} else if (nread == 0) {
			return 0;
		} else
			return (-1); /* error */
	}
	for (int i = 0; i < nread; i++) {
		buf.push_back((char) tmpbuf[i]);
	}
	return buf.length();
}

ssize_t read_once(int fd, std::string& buf, size_t length) {
	ssize_t nread;
	char* tmpbuf = new char[length];
	buf.clear();
	if ((nread = read(fd, tmpbuf, length)) <= 0) {
		if (nread < 0 && errno == EINTR) {
			nread = 0; /* and call write() again */
		} else if (nread == 0) {
			delete[] tmpbuf;
			return 0;
		} else {
			delete[] tmpbuf;
			return (-1); /* error */
		}
	}
	for (int i = 0; i < nread; i++) {
		buf.push_back((char) tmpbuf[i]);
	}
	delete[] tmpbuf;
	return buf.length();
}

FILE* get_log_file() {
	static FILE* log_file = -0;
	if (!log_file) {
		log_file = fopen("MsgServer.log", "w");
	}
	return log_file;
}

std::string vatostr(const char *fmt, va_list ap) {
	char buf[MAXLINE + 1];
	vsnprintf(buf, MAXLINE, fmt, ap);
	return buf;
}

std::string vatostr2(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	char buf[MAXLINE + 1];
	vsnprintf(buf, MAXLINE, fmt, ap);
	return buf;
	va_end(ap);
	return buf;
}

void log_error(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	std::string msg = vatostr(fmt, ap);
	fprintf(stderr, "%s", msg.c_str());
	fprintf(get_log_file(), "%s", msg.c_str());
	va_end(ap);
	return;
}

void log_info(const char *fmt, ...) {
#ifdef LOG_LEVEL
	if (LOG_LEVEL<2) {
		return;
	}
	va_list ap;
	va_start(ap, fmt);
	std::string msg= vatostr(fmt,ap);
#if defined(PROG_INFO_STDOUT) || defined(PROG_DEBUG_STDOUT)
	fprintf(stdout,"%s", msg.c_str());
#endif
	fprintf(get_log_file(),"%s",msg.c_str());
	va_end(ap);
#endif
	return;
}

void log_debug(const char *fmt, ...) {
#ifdef LOG_LEVEL
	if (LOG_LEVEL<3) {
		return;
	}
	va_list ap;
	va_start(ap, fmt);
	std::string msg= vatostr(fmt,ap);
#ifdef PROG_DEBUG_STDOUT
	fprintf(stdout,"%s",msg.c_str());
#endif
	fprintf(get_log_file(),"%s", msg.c_str());
	va_end(ap);
#endif
	return;
}

bool file_exists(const char* filename) {
	std::ifstream f(filename);
	if (f.good()) {
		f.close();
		return true;
	} else {
		f.close();
		return false;
	}
}

std::vector<std::string> readlines(const char* filename) {
	std::ifstream file(filename);
	std::string str;
	std::vector<std::string> file_contents;
	while (std::getline(file, str)) {
		file_contents.push_back(str);
	}
	file.close();
	return file_contents;
}

unsigned char rand_lim(unsigned int limit) {
	/* return a random number between 0 and limit inclusive.
	 */
	static int rand_seeded = 0;
	if (!rand_seeded) {
		srand(time(0));
		rand_seeded = 1;
	}

	unsigned int x = rand();
	return (unsigned char) x - x / (limit + 1) * (limit + 1);
}

std::string toHex(const std::string& s, bool upper_case) {
	std::ostringstream ret;

	for (std::string::size_type i = 0; i < s.length(); ++i)
		ret << std::hex << std::setfill('0') << std::setw(2)
				<< (upper_case ? std::uppercase : std::nouppercase)
				<< (int) ((unsigned char) s[i]);
	return ret.str();
}
std::string toUpper(const std::string& s) {
	std::string str = s;
	std::transform(str.begin(), str.end(), str.begin(), ::toupper);
	return str;
}

int host2ip(const std::string& hostname) {
	struct sockaddr whereto;
	struct hostent *hp;
	struct sockaddr_in *to;
	memset(&whereto, 0, sizeof(struct sockaddr));
	to = (struct sockaddr_in *) &whereto;
	hp = gethostbyname(hostname.c_str());
	if (!hp) {
		return -1;
	} else {
		to->sin_family = hp->h_addrtype;
		memcpy(&(to->sin_addr.s_addr), hp->h_addr, hp->h_length);
		return ntohl(to->sin_addr.s_addr);
	}

}

std::string getLocalHostname() {
	char target[255 + 1];
	if (gethostname(target, 255) < 0) {
		return "";
	}
	return target;
}

int getLocalIP() {

	char target[255 + 1];
	if (gethostname(target, 255) < 0) {
		return -1;
	}
	return host2ip(target);
}

std::string itoaddress(int ip) {
	struct in_addr ip_addr;
	ip_addr.s_addr = htonl(ip);
	return inet_ntoa(ip_addr);
}

size_t getFileSize(std::string fname) {
	struct stat st;
	stat(fname.c_str(), &st);
	return st.st_size;
}

std::string to_string(size_t n) {
	char str[128];
	sprintf(str, "%ld", n);
	return str;
}

const char* staticMap(char c) {
	switch (c) {
	case '\a':
		return "\\a";
	case '\b':
		return "\\b";
	case '\t':
		return "\\t";
	case '\n':
		return "\\n";
	case '\v':
		return "\\v";
	case '\f':
		return "\\f";
	case '\r':
		return "\\r";

	case '\"':
		return "\\\"";
	case '\'':
		return "\\\'";
	case '\?':
		return "\\\?";
	case '\\':
		return "\\\\";
	}
	return nullptr;
}

std::string escape_cpp(const std::string& input) {
	std::stringstream ss;
	for (char c : input) {
		const char* str = staticMap(c);
		if (str) {
			ss << str;
		} else if (!isprint(static_cast<unsigned char>(c))) {
			ss << "\\u" << std::hex << std::setfill('0') << std::setw(4)
					<< (static_cast<unsigned int>(static_cast<unsigned char>(c)));
		} else {
			ss << c;
		}
	}
	return ss.str();
}

}
