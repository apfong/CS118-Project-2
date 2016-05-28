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
  int bytesRec = 0;
  bool establishedTCP = false;
  bool startedHandshake = false;
  uint16_t ackRes = 0;
  uint16_t seqNumRes = 0; //rand() % MAX_SEQ_NUM // from 0->MAX_SEQ_NUM
  uint16_t flags = 0x00;

  while(true){
  	
  	bytesRec = recvfrom(sockfd, buf, 1024, 0, (struct sockaddr*)&clientAddr, &clientAddrSize);
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

        // if SYN=1 and ACK=0
        if (header->getSynFlag() && !(header->getAckFlag()) && !startedHandshake) {
          cerr << "Received TCP setup packet\n";
          startedHandshake = true;
          ackRes = header->getAckNum() + 1;
          seqNumRes = 1; //rand() % MAX_SEQ_NUM // from 0->MAX_SEQ_NUM
          flags = 0x06;
          vector<char> data;
          TcpPacket* res = new TcpPacket(seqNumRes, ackRes, INIT_CWND_SIZE, flags, data);
          vector<char> resPacket = res->buildPacket();
          if (sendto(sockfd, &resPacket[0], resPacket.size(), 0, (struct sockaddr *)&clientAddr,
                    (socklen_t)sizeof(clientAddr)) == -1) {
            perror("send error");
            return 1;
          }
          delete res;
        }
        if (header->getAckNum() == seqNumRes + 1) {
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

  }
  close(sockfd);
  return 0;
}
