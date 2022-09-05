CC=gcc
CXX=g++
DEBUGFLAG=-ggdb	#remove this for release
LOG_FLAG=-DLOG_LEVEL=3 -DPROG_INFO_STDOUT #-DPROG_DEBUG_STDOUT
#LOG_FLAG=-DLOG_LEVEL=3 -DPROG_DEBUG_STDOUT
CFLAGS=-Wall -ansi -pedantic $(DEBUGFLAG) $(LOG_FLAG)
CXXFLAGS=-Wall -ansi -pedantic -std=c++11 $(DEBUGFLAG) $(LOG_FLAG)
LDFLAGS=
EXECUTABLE=  messenger_server messenger_client

all: $(EXECUTABLE)
    
	
messenger_server: messenger_server_main.o  Util.o ServerListener.o MessagerServer.o HttpMessage.o UserInfo.o
	$(CXX) $(LDFLAGS) $^ -o $@ -lreadline -lpthread
	
messenger_client: messenger_client_main.o  Util.o ClientListner.o MessageClient.o HttpMessage.o UserInfo.o
	$(CXX) $(LDFLAGS) $^ -o $@ -lreadline -lpthread			

	
.PHONY: test
test:
	make -C test
	
clean:
	rm -fr ${EXECUTABLE} *.o *.x core core.*
	#make -C test clean

