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
#include <sys/fcntl.h>
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
	hints.ai_socktype = SOCK_DGRAM; // UDP
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
	//serverAddr.sin_addr.s_addr = inet_addr(ipstr);
	serverAddr.sin_addr.s_addr = inet_addr("10.0.0.1");
	memset(serverAddr.sin_zero, '\0', sizeof(serverAddr.sin_zero));

	int bytesRec = 0;
	bool establishedTCP = false;
	const int buf_size = 1032;
	char * buf = new char[buf_size];
	memset(buf,'\0',sizeof(buf));

	int CURRENT_SEQ_NUM = 0;//rand() % MAX_SEQ_NUM // from 0->MAX_SEQ_NUM
	int CURRENT_ACK_NUM = 0;
	
	// Variables for timeout using select
	// ioctlsocket(FIONBIO)/fcntl(O_NONBLOCK), need this for non-blocking sockets?
	// need to create separate fd_set for each packet? how to set individual timeouts?
//	vector<int> packetFds;
//	packetFds.push(sockfd);
	struct timeval timeout;
	timeout.tv_sec = 2;
	timeout.tv_usec = 500000; // 500ms
	bool gotSynAck = false;
	if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(timeout)) < 0)
		cerr << "setsockopt failed\n";


	// Setting up TCP connection
	uint16_t flags = 0x02;
	vector<char> data;
	TcpPacket* initTCP = new TcpPacket(CURRENT_SEQ_NUM, 0, 0, flags, data);
	vector<char> initPacket = initTCP->buildPacket();
	cout << "Starting SEQ Num: " << CURRENT_SEQ_NUM << endl;
	cout << "Starting ACK Num: " << CURRENT_ACK_NUM << endl;
	if (sendto(sockfd, &initPacket[0], initPacket.size(), 0, (struct sockaddr *)&serverAddr, (socklen_t)sizeof(serverAddr)) == -1) {
		perror("send error");
		return 1;
	}

	// After sending out syn packet, keep resending if timeout reached before receiving response
	bool placeholder = true;
	while (placeholder) {
		placeholder = false;

		while (!gotSynAck) {
			bytesRec = recvfrom(sockfd, buf, buf_size, 0, (struct sockaddr*)&serverAddr, &serverAddrSize);
			if (bytesRec == -1) {
				if (EWOULDBLOCK) {
					cerr << "Timed out, resending syn\n";
					if (sendto(sockfd, &initPacket[0], initPacket.size(), 0, (struct sockaddr *)&serverAddr, (socklen_t)sizeof(serverAddr)) == -1) {
						perror("send error");
						return 1;
					}
				} else {
					perror("error receiving");
					return 1;
				}
			} 
			if (bytesRec == 8) {
				// Dealing with 3 way handshake headers
				if (!establishedTCP) {
					vector<char> bufVec(buf, buf+buf_size);
					TcpPacket* header = new TcpPacket(bufVec);

					// if SYN=1 and ACK=1
					if (header->getSynFlag() && (header->getAckFlag())) {
						cerr << "Received TCP setup packet\n";
						gotSynAck = true;
						delete initTCP;
						CURRENT_SEQ_NUM++;
						cout << "Starting SEQ Num: " << CURRENT_SEQ_NUM << endl;
						CURRENT_ACK_NUM = header->getSeqNum() + 1;
						cout << "Starting ACK Num: " << CURRENT_ACK_NUM << endl;
						uint16_t flags = 0x06;
						vector<char> data;
						TcpPacket* res = new TcpPacket(CURRENT_SEQ_NUM, CURRENT_ACK_NUM, INIT_CWND_SIZE, flags, data);
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
				}
			}
		}

		// Finished receiving header
		if (bytesRec == 8 && establishedTCP) {
			cerr << "Received some bytes: " << bytesRec << endl;

			ofstream output("copiedfile.html");
			// Get rest of data: UDP packets holding TCP packets
			while((bytesRec = recvfrom(sockfd, buf, buf_size, 0, (struct sockaddr*)&serverAddr, &serverAddrSize))){
				if(bytesRec == -1){
					if (EWOULDBLOCK) {
						cerr << "Doing nothing, waiting for incoming data packets\n";
					} else {
						perror("file receive error");
						return 1;
					}
				}
				if(bytesRec == 0){
					cerr << "no more to read"<<endl;
					break;
				}
				vector<char> recv_data(buf, buf+buf_size);
				TcpPacket recv_packet(recv_data);
				cout<<"Received packet w/ SEQ Num: " << recv_packet.getSeqNum() <<", ACK Num: " << recv_packet.getAckNum() << endl;

				if (recv_packet.getFinFlag()) {
				  cout << "----------------RECEIVED FIN ACK----------------\n";
				  cout << "---NEED TO FIX BUG WHERE WE GET TOO MUCH DATA---\n";
				  cout << "----SEQ/ACK NUMS WILL BE OFF BECAUSE OF THIS----\n";
				  break; //for now just break; i think something's wrong with our last packet's data
				}

				if (CURRENT_ACK_NUM == recv_packet.getSeqNum()) { //Should this be something else??
				  
				  /*RESTORE THIS ONCE WE FIX THE LAST PACKET TOO MUCH DATA PROBLEM
				  if (recv_packet.getFinFlag()) {
				    CURRENT_ACK_NUM++; //Only increment by 1 bc only received FIN ACK
				    break; //break out of loop
				  }
				  */

					output << recv_packet.getDataSize();
					CURRENT_ACK_NUM += recv_packet.getDataSize(); //Bytes received
				}

				//Send ACK
				if (CURRENT_SEQ_NUM+1 == recv_packet.getAckNum()) {
					vector<char> data;
					uint16_t flags = 0x4; //ACK flag

					CURRENT_SEQ_NUM++; //Increment; only sending an ACK
					TcpPacket* ackPacket = new TcpPacket(CURRENT_SEQ_NUM, CURRENT_ACK_NUM, INIT_CWND_SIZE, flags, data);
					cout << "Sending ACK w/ SEQ Num: " << CURRENT_SEQ_NUM << ", ACK Num: " << CURRENT_ACK_NUM << endl << endl;

					vector<char> ackPacketVector = ackPacket->buildPacket();
					//Send ACK to server
					if (sendto(sockfd, &ackPacketVector[0], ackPacketVector.size(), 0, (struct sockaddr *)&serverAddr, (socklen_t)sizeof(serverAddr)) == -1) {
						perror("Error while sending ACK");
						return 1;
					}
				}
			}

			//Received FIN ACK; send ACK
			vector<char> data;
			uint16_t flags = 0x4; //ACK flag
			CURRENT_SEQ_NUM++; //Increment; only sending an ACK
			TcpPacket* ackPacket = new TcpPacket(CURRENT_SEQ_NUM, CURRENT_ACK_NUM, INIT_CWND_SIZE, flags, data);
			cout << "Sending ACK w/ SEQ Num: " << CURRENT_SEQ_NUM << ", ACK Num: " << CURRENT_ACK_NUM << endl << endl;
			vector<char> ackPacketVector = ackPacket->buildPacket();
			//Send ACK to server
			if (sendto(sockfd, &ackPacketVector[0], ackPacketVector.size(), 0, (struct sockaddr *)&serverAddr, (socklen_t)sizeof(serverAddr)) == -1) {
			  perror("Error while sending ACK");
			  return 1;
			}
			delete ackPacket;

			//Now send FIN ACK
			flags = 0x5; //FIN ACK
			CURRENT_SEQ_NUM++; //Increment; only sending FIN ACK
			TcpPacket* finAck = new TcpPacket(CURRENT_SEQ_NUM, CURRENT_ACK_NUM, INIT_CWND_SIZE, flags, data);
			cout << "Sending FIN ACK w/ SEQ Num: " << CURRENT_SEQ_NUM << ", ACK Num: " << CURRENT_ACK_NUM << endl;
			vector<char> finAckVector = finAck->buildPacket();
			//Sending ACK to server
			if (sendto(sockfd, &finAckVector[0], finAckVector.size(), 0, (struct sockaddr *)&serverAddr, (socklen_t)sizeof(serverAddr)) == -1) {
			  perror("Error while sending FIN ACK");
			  return 1;
			}
			delete finAck;

			//Listen for final ACK
			bytesRec = recvfrom(sockfd, buf, buf_size, 0, (struct sockaddr*)&serverAddr, &serverAddrSize);
			if (bytesRec == -1) {
				if (EWOULDBLOCK) {
					cerr << "SHOULDNT tIMEOUT HERE, do nothing\n";
				}
				else {
					perror("Error while listening for final ACK");
					return 1;
				}
			}
			vector<char> recv_data(buf, buf+buf_size);
			TcpPacket recv_packet(recv_data);
			cout<<"Received final ACK w/ SEQ Num: "<<recv_packet.getSeqNum()<<", ACK Num: "<<recv_packet.getAckNum()<<endl<<endl;
			if (CURRENT_ACK_NUM == recv_packet.getSeqNum()) {
			  CURRENT_ACK_NUM++; //RECEIVED ACK
			}

			cout<<"Closing connection.\n\n";

			output.close();
		}
		//cout<<"sent"<<endl;
	}
	close(sockfd);
	return 0;
}
