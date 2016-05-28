#include <string.h>
#include <thread>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <fstream>
using namespace std;

int main(int argc, char* argv[])
{
	//char * buf = new char[1024];

	// create a socket using TCP IP
  	int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if(sockfd == -1){
  	perror("bad socket");
  	return 1;
  }

  	 struct addrinfo hints;
	  struct addrinfo* res;
	  // prepare hints
	  memset(&hints, 0, sizeof(hints));
	  hints.ai_family = AF_INET; // IPv4
	  hints.ai_socktype = SOCK_STREAM; // TCP
	  // gets a list of addresses stored in res
	  int status = 0;
	  string hoststring = "localhost";
	  const char * host = hoststring.c_str();
	  string portstring = "4000";
	  const char * port = portstring.c_str();
	  if ((status = getaddrinfo(host, port, &hints, &res)) != 0) {
	    std::cerr << "getaddrinfo: " << gai_strerror(status) << std::endl;
	    return 2;
	  }
	  //for(struct addrinfo* p = res; p != 0; p = p->ai_next) {
    struct addrinfo* p = res;
    // convert address to IPv4 address
    struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;

    // convert the IP to a string and print it:
    char ipstr[INET_ADDRSTRLEN] = {'\0'};
    inet_ntop(p->ai_family, &(ipv4->sin_addr), ipstr, sizeof(ipstr));
    cout <<"ipaddr "<<ipstr<<endl;
	//create a struct sockaddr with the given port and ip address
	struct sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(4000);//request.getPort();
	//serverAddr.sin_port = htons(80);     // short, network byte order
	//serverAddr.sin_addr.s_addr = inet_addr(ipstr);
	serverAddr.sin_addr.s_addr = inet_addr("10.0.0.1");
	memset(serverAddr.sin_zero, '\0', sizeof(serverAddr.sin_zero));

	while(true){
		string msg;
		cin >> msg;
		if(sendto(sockfd, &msg[0], msg.size(), 0, (struct sockaddr *)&serverAddr, (socklen_t)sizeof(serverAddr))==-1){
			perror("send error");
			return 1;
		}
		cout<<"sent"<<endl;
	}
	close(sockfd);
	return 0;
}
