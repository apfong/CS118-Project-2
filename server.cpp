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

// Default values for some variables
// Most are in units of bytes, except retransmission time(ms)
#define MAX_PKT_LEN 1032
#define MAX_SEQ_NUM 30720
#define INIT_CWND_SIZE 1024
#define INIT_SS_THRESH 30720
#define RETRANS_TIMEOUT 500
// basic client's receiver window can always be 30720, but server should be
// able to properly handle cases when the window is reduced

int main()
{
    // create a socket using TCP IP
  int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if(sockfd == -1){
  	perror("bad socket");
  	return 1;
  }
  int port = 4000;
  // allow others to reuse the address
  int yes = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
    perror("setsockopt");
    return 1;
  }

  // bind address to socket
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);     // short, network byte order
  addr.sin_addr.s_addr = inet_addr("10.0.0.1");
  memset(addr.sin_zero, '\0', sizeof(addr.sin_zero));

  if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
    perror("bind");
    return 2;
  }

  // set socket to listen status
  // if (listen(sockfd, 1) == -1) {   // how many connections the queue will hold onto
  //   perror("listen");
  //   return 3;
  // }
  char * buf = new char[1024];
	memset(buf,'\0',sizeof(buf));
  	cout<<"start recv"<<endl;
	struct sockaddr_in clientAddr;
    socklen_t clientAddrSize = sizeof(clientAddr);

  while(true){
  	
  	if(recvfrom(sockfd, buf, 1024, 0, (struct sockaddr*)&clientAddr, &clientAddrSize) == -1){
  		perror("error receive");
  		return 1;
  	}
  	cout<<"received"<<buf<<endl;

  }
  close(sockfd);
  return 0;
}
