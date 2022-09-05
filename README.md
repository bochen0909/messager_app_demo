Project: A Messenger Application 
==================
 

Build
==================
In the project folder run _make_, for example 

	$ make
	g++ -Wall -ansi -pedantic -std=c++11 -ggdb       -DLOG_LEVEL=3 -DPROG_INFO_STDOUT    -c -o messenger_server_main.o messenger_server_main.cpp                                
	g++ -Wall -ansi -pedantic -std=c++11 -ggdb       -DLOG_LEVEL=3 -DPROG_INFO_STDOUT    -c -o Util.o Util.cpp                                                                  
	g++ -Wall -ansi -pedantic -std=c++11 -ggdb       -DLOG_LEVEL=3 -DPROG_INFO_STDOUT    -c -o ServerListener.o ServerListener.cpp                                              
	g++ -Wall -ansi -pedantic -std=c++11 -ggdb       -DLOG_LEVEL=3 -DPROG_INFO_STDOUT    -c -o MessagerServer.o MessagerServer.cpp                                              
	g++ -Wall -ansi -pedantic -std=c++11 -ggdb       -DLOG_LEVEL=3 -DPROG_INFO_STDOUT    -c -o HttpMessage.o HttpMessage.cpp                                                    
	g++ -Wall -ansi -pedantic -std=c++11 -ggdb       -DLOG_LEVEL=3 -DPROG_INFO_STDOUT    -c -o UserInfo.o UserInfo.cpp                                                          
	g++  messenger_server_main.o Util.o ServerListener.o MessagerServer.o HttpMessage.o UserInfo.o -o messenger_server -lreadline -lpthread                                     
	g++ -Wall -ansi -pedantic -std=c++11 -ggdb       -DLOG_LEVEL=3 -DPROG_INFO_STDOUT    -c -o messenger_client_main.o messenger_client_main.cpp                                
	g++ -Wall -ansi -pedantic -std=c++11 -ggdb       -DLOG_LEVEL=3 -DPROG_INFO_STDOUT    -c -o ClientListner.o ClientListner.cpp                                                
	g++ -Wall -ansi -pedantic -std=c++11 -ggdb       -DLOG_LEVEL=3 -DPROG_INFO_STDOUT    -c -o MessageClient.o MessageClient.cpp                                                
	g++  messenger_client_main.o Util.o ClientListner.o MessageClient.o HttpMessage.o UserInfo.o -o messenger_client -lreadline -lpthread 
	
To clean the build
	       
	$ make clean
	rm -fr messenger_server messenger_client *.o *.x core       


Quick Start 
==================
There are sample a user file and configuration files to run messenger server and messenger client. 

start messenger server first
 
	$ ./messenger_server user.txt conf.txt
	There are 4 users loaded
	Bind file socket at port 7777
	Using IP w530:7777 or 127.0.1.1:7777 for clients to connect
	Finish starting listener thread tid=1783863040

then start messenger client (in a new console)

	$ ./messenger_client clientconf.txt 
	Bind client socket at port 5100
	client listening on 127.0.1.1:5100
	connected to server localhost:7777
	Finish starting listener thread tid=139742421153536
	Messenger: 
	
optionally port number can be specified for messenger_client

	$ ./messenger_client clientconf.txt 12345
	Bind client socket at port 12345
	client listening on 127.0.1.1:12345
	connected to server localhost:7777
	Finish starting listener thread tid=140289531471616
	Messenger: 
	

Simple Design Description 
==========================

Messenger Server
-----------------

Messenger server reads user and configuration files. Then it starts a new thread, bind and listens 
on the configurated port number using `select` call (refer: ServerListener::startlisten()) to wait 
for connection. Main thread does nothing currently except wait for shutdown.   

When a client logs into the server, user, location and connection are stored. Through 
the connection client may call methods to server (e.g. logout, invite etc) and server may
notify users (e.g. friend online/offline, new friend, etc).  When a client quits or shutdown 
abnormally, the socket resources are reclaimed and its friends are notified.

When server shutdown (by Ctl-C), resources are cleaned up and threads are joined.  

Messenger Client
-----------------

Messenger Client reads the configuration file. Then it starts a new thread, bind and listens 
on the configurated port number using `select` call (refer: ClientListner::startlisten()).
to wait for connection. 
 
Main thread waits for user's input and executes the command correspondly.   

A client needs login or sign up to connect the server. Friends are fetched after login.  
When a client chat to a friend, it estimates a connection to the server directly.
When the client logs out or quits, connection is closed.   

Client can quit by `exit` command or `Ctl-D`. When server shutdown, client also quits.

	
Client Commands 
==================
Get help
-----------------

	Messenger: help
	help            Display information about built-in commands.
	r               register a new user
	l               login with a existing user
	m               send message to a friend
	i               invite a new friend
	ia              accept a invitation
	logout          logout current user.
	lf              list friends
	query           query available users
	!               Execute external shell command.
	exit            Exit.



Register new user
-----------------

SYNOPSIS:  r [username]

Example:
 
	Messenger: r
	Enter   your username: newuser
	Enter   your password: 
	Confirm your password: 
	Server>> register succeeds
	no friends.

Login 
-----------------

SYNOPSIS:  l [username]

Example:
 
    Messenger: l 
	Enter   your username: eve
	Enter   your password: 
	Server>> login succeeds
	2 friends:
	ada                  offline    0.0.0.0              0         
	tom                  offline    0.0.0.0              0       


Logout 
-----------------

SYNOPSIS:  logout

Example:

	bob: logout
	Messenger: 

Exit 
-----------------

SYNOPSIS:  exit

Example:


	Messenger: exit
	Listener stopped....


List friends
-----------------

SYNOPSIS:  lf

Example:
 
	eve: lf
	2 friends:
	ada                  offline    0.0.0.0              0         
	tom                  offline    0.0.0.0              0 



List users
-----------------

SYNOPSIS:  query

Example:
 
	eve: query
	5 users:
	ada                  offline    0.0.0.0              0         
	bob                  offline    0.0.0.0              0         
	eve                  online     127.0.1.1            5100      
	newuser              offline    0.0.0.0              0         
	tom                  offline    0.0.0.0              0  



Invite a friend
-----------------

SYNOPSIS:  i <username> [message]

Example:
	
bob's screen
 
	bob: i eve I want to be your friend
	Server>> The request has been sent

eve's screen

	eve: 
	Server>> bob invited you as a friend and said `I want to be your friend`.

	
accept invitation
-----------------

SYNOPSIS:  ia <username> [message]

Example:

eve's screen

	eve: ia bob nice to meet you
	Server>> OK

bob's screen

	bob: 
	Server>> eve accepted your invitation and said `nice to meet you`.
	
 
 
Chat
-----------------

SYNOPSIS:  m

Example:

eve's screen

	eve: m bob  are you busiy?

bob's screen

	eve>> are you busiy?
	



	