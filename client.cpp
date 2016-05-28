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
#include "tcp_message.cpp"
using namespace std;

// Default values for some variables
// Most are in units of bytes, except retransmission time(ms)
const uint16_t MAX_PKT_LEN = 1032;
const uint16_t MAX_SEQ_NUM = 30720;
const uint16_t INIT_CWND_SIZE = 1024;
const uint16_t INIT_SS_THRESH = 30720;
const uint16_t RETRANS_TIMEOUT = 500;
// basic client's receiver window can always be 30720, but server should be
// able to properly handle cases when the window is reduced

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
	  string hoststring = "10.0.0.1";
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
    socklen_t serverAddrSize = sizeof(serverAddr);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(4000);//request.getPort();
	//serverAddr.sin_port = htons(80);     // short, network byte order
	serverAddr.sin_addr.s_addr = inet_addr(ipstr);
	memset(serverAddr.sin_zero, '\0', sizeof(serverAddr.sin_zero));

  int bytesRec = 0;
  bool establishedTCP = false;
  char * buf = new char[1024];
	memset(buf,'\0',sizeof(buf));

  /*
	while(true){
		string msg;
		cin >> msg;
    
  */
    // Setting up TCP connection
    if (!establishedTCP) {
      uint16_t seqNum = 1; // change to rand() % MAX_SEQ_NUM;
      uint16_t flags = 0x02;
      vector<char> data;
      TcpPacket* initTCP = new TcpPacket(seqNum, 0, 0, flags, data);
      vector<char> initPacket = initTCP->buildPacket();
      if (sendto(sockfd, &initPacket[0], initPacket.size(), 0, (struct sockaddr *)&serverAddr,
                (socklen_t)sizeof(serverAddr)) == -1) {
        perror("send error");
        return 1;
      }
      delete initTCP;
    }

    // After sending out syn packet
    while (true) {
      bytesRec = recvfrom(sockfd, buf, 1024, 0, (struct sockaddr*)&serverAddr, &serverAddrSize);
      cout<<"start recv"<<endl;
      if(bytesRec == -1){
        perror("error receiving");
        return 1;
      }
      cout<<"received"<<buf<<endl;

      // Finished receiving header
      if (bytesRec == 8) {
        // Dealing with 3 way handshake headers
        if (!establishedTCP) {
          vector<char> bufVec(buf, buf+1024);
          TcpPacket* header = new TcpPacket(bufVec);

          // if SYN=1 and ACK=1
          if (header->getSynFlag() && (header->getAckFlag())) {
            cerr << "Received TCP setup packet\n";
            uint16_t ackRes = 0;
            uint16_t seqNumRes = 0; //rand() % MAX_SEQ_NUM // from 0->MAX_SEQ_NUM
            uint16_t flags = 0x00;
            ackRes = header->getAckNum() + 1;
            seqNumRes = 1; //rand() % MAX_SEQ_NUM // from 0->MAX_SEQ_NUM
            flags = 0x06;
            vector<char> data;
            TcpPacket* res = new TcpPacket(seqNumRes, ackRes, INIT_CWND_SIZE, flags, data);
            vector<char> resPacket = res->buildPacket();
            if (sendto(sockfd, &resPacket[0], resPacket.size(), 0, (struct sockaddr *)&serverAddr,
                      (socklen_t)sizeof(serverAddr)) == -1) {
              perror("send error");
              return 1;
            }
            delete res;
            cerr << "Established TCP connection after 3 way handshake\n";
            establishedTCP = true;
          }

          delete header;
          continue;
        }


      /*
      // Get rest of data: UDP packets holding TCP packets
      while (recvfrom(sockfd, buf, 1024, 0, (struct sockaddr*)&clientAddr, &clientAddrSize)) {
        buf
      }
      */

      }

		cout<<"sent"<<endl;
	  }
/*
  }
*/
	close(sockfd);
	return 0;
}
