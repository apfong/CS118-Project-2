#include <string>
#include <thread>
#include <iostream>

int main()
{
	char * buf = new char[1024];

	// create a socket using TCP IP
  	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

  	 struct addrinfo hints;
	  struct addrinfo* res;
	  // prepare hints
	  memset(&hints, 0, sizeof(hints));
	  hints.ai_family = AF_INET; // IPv4
	  hints.ai_socktype = SOCK_STREAM; // TCP
	  // gets a list of addresses stored in res
	  int status = 0;
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

	sendto(sockfd, buf, 1024, 0, addr, length);

}
