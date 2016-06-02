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

	const int buf_size = 1024;
	char * buf = new char[buf_size];
	memset(buf,'\0',sizeof(buf));
	cout<<"start recv"<<endl;
	struct sockaddr_in clientAddr;
	socklen_t clientAddrSize = sizeof(clientAddr);
	int bytesRec = 0;
	bool establishedTCP = false;
	bool startedHandshake = false;
	uint16_t flags = 0x00;
	int CURRENT_SEQ_NUM = 0;//rand() % MAX_SEQ_NUM // from 0->MAX_SEQ_NUM
	int CURRENT_ACK_NUM = 0;

	// Variables for timeout using select
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 500000; // 500ms
	int rv;
	int maxSocketFd = sockfd + 1;

	while(true){
		if (!establishedTCP) {
			bytesRec = recvfrom(sockfd, buf, buf_size, 0, (struct sockaddr*)&clientAddr, &clientAddrSize);
			if(bytesRec == -1){
				perror("error receiving");
				return 1;
			}
		}

		// Finished receiving header
		// Dealing with 3 way handshake headers
		if (!establishedTCP) {
			vector<char> bufVec(buf, buf+buf_size);
			TcpPacket* header = new TcpPacket(bufVec);

			// if SYN=1 and ACK=0
			if (header->getSynFlag() && !(header->getAckFlag()) && !startedHandshake) {
				cerr << "Received TCP setup packet\n";
				startedHandshake = true;
				CURRENT_ACK_NUM = header->getSeqNum() + 1; //I think SEQ, not ACK??
				flags = 0x06;
				vector<char> data;
				TcpPacket* res = new TcpPacket(CURRENT_SEQ_NUM, CURRENT_ACK_NUM, INIT_CWND_SIZE, flags, data);
				vector<char> resPacket = res->buildPacket();
				if (sendto(sockfd, &resPacket[0], resPacket.size(), 0, (struct sockaddr *)&clientAddr,
							(socklen_t)sizeof(clientAddr)) == -1) {
					perror("send error");
					return 1;
				}
				delete res;
				//continue;
			}

			if (header->getAckNum() == CURRENT_SEQ_NUM + 1) {
				cerr << "Established TCP connection after 3 way handshake\n";
				establishedTCP = true;
				CURRENT_SEQ_NUM++;
				cout << "Starting SEQ Num: " << CURRENT_SEQ_NUM << endl;
				CURRENT_ACK_NUM = header->getSeqNum() + 1;
				cout << "Starting ACK Num: " << CURRENT_ACK_NUM << endl;
			}

			delete header;
			continue; // keep receiving new packets, until handshake is established
		}

		// Preparing to open file
		//stringstream filestream;
		//string line;

		cerr << "Got past establishing TCP\n";
		string resFilename = "index.html";
		//streampos size;

		// Dealing with root file request
		if (resFilename == "/") {
			resFilename += "index.html";
		}

		// Prepending starting directory to requested filename
		//resFilename.insert(0, tFiledir);

		cerr << "trying to get file from path: " << resFilename << endl;
		ifstream resFile (resFilename, ios::in|ios::binary);

		if (resFile.good()) {
			cerr << "//////////////////////////////////////////////////////////////////////////////\n";
			cerr << "\nOpened file: " << resFilename << endl;
			resFile.open(resFilename);

			ifstream resFile(resFilename, std::ios::binary);
			vector<char> payload((std::istreambuf_iterator<char>(resFile)),
					std::istreambuf_iterator<char>());
			cout<<"size: "<<payload.size()<<endl;
			//string payloadStr(payload.begin(), payload.end());
			//int payloadSize = payload.size();


			vector<char>::iterator packetPoint = payload.begin();

			//this will break the file up into small packets to be sent over
			while(packetPoint < payload.end()) {

				int end = (payload.end() - packetPoint > 1024) ? 1024 : (payload.end() - packetPoint);
				vector<char> packet_divide(packetPoint, packetPoint + end);
				packetPoint += end;

				flags = 0x4; //ACK flag

				TcpPacket* tcpfile = new TcpPacket(CURRENT_SEQ_NUM, CURRENT_ACK_NUM, INIT_CWND_SIZE, flags, packet_divide);

				cout<<"Sending packet w/ SEQ Num: "<< CURRENT_SEQ_NUM <<", ACK Num: " << CURRENT_ACK_NUM <<endl;

				vector<char> tcpfile_packet = tcpfile->buildPacket();
				// Sending response object
				if (sendto(sockfd, &tcpfile_packet[0], tcpfile_packet.size(), 0, (struct sockaddr *)&clientAddr,
							(socklen_t)sizeof(clientAddr)) == -1) {
					perror("send error");
					return 1;
				}

				CURRENT_SEQ_NUM += tcpfile->getData().size();				
				delete tcpfile;

				//Listen for ACK
				bytesRec = recvfrom(sockfd, buf, buf_size, 0, (struct sockaddr*)&clientAddr, &clientAddrSize);
				if(bytesRec == -1){
					perror("Error while listening for ACK");
					return 1;
				}

				vector<char> recv_data(buf, buf+buf_size);
				TcpPacket recv_packet(recv_data);
				cout<<"Received ACK w/ SEQ Num: "<< recv_packet.getSeqNum() << ", ACK Num: " << recv_packet.getAckNum() << endl << endl;

				if (CURRENT_ACK_NUM == recv_packet.getSeqNum()) {
					CURRENT_ACK_NUM++; //Received ACK
				}



			}
			cerr << "GOT to the end of the file\n\n";

			//Sending FIN ACK
			flags = 0x5;
			vector<char> data;
			TcpPacket* finAck = new TcpPacket(CURRENT_SEQ_NUM, CURRENT_ACK_NUM, INIT_CWND_SIZE, flags, data);
			cout<<"Sending FIN ACK w/ SEQ Num: "<<CURRENT_SEQ_NUM<<", ACK Num: "<<CURRENT_ACK_NUM<<endl;
			vector<char> finAckVector = finAck->buildPacket();
			if (sendto(sockfd, &finAckVector[0], finAckVector.size(), 0, (struct sockaddr *)&clientAddr, (socklen_t)sizeof(clientAddr)) == -1) {
				perror("send error");
				return 1;
			}

			CURRENT_SEQ_NUM++; //Only increment by 1 bc only sent FIN ACK
			delete finAck;

			//Now in FIN_WAIT_1 State, waiting for an ACK
			bytesRec = recvfrom(sockfd, buf, buf_size, 0, (struct sockaddr*)&clientAddr, &clientAddrSize);
			if (bytesRec == -1) {
				perror("Error while listening for ACK");
				return 1;
			}
			vector<char> recv_data(buf, buf+buf_size);
			TcpPacket recv_packet(recv_data);
			cout<<"Received ACK w/ SEQ Num: "<<recv_packet.getSeqNum()<<", ACK Num: "<<recv_packet.getAckNum()<<endl<<endl;
			if (CURRENT_ACK_NUM == recv_packet.getSeqNum()) {
				CURRENT_ACK_NUM++; //Received ACK
			}

			//Now in FIN_WAIT_2 State, waiting for a FIN ACK
			bytesRec = recvfrom(sockfd, buf, buf_size, 0, (struct sockaddr*)&clientAddr, &clientAddrSize);
			if (bytesRec == -1) {
				perror("Error while listening for FIN ACK");
				return 1;
			}
			vector<char> recv_data2(buf, buf+buf_size);
			TcpPacket recv_packet2(recv_data2);
			if (recv_packet2.getFinFlag()) {

				cout<<"Received FIN ACK w/ SEQ Num: "<<recv_packet2.getSeqNum()<<", ACK Num: "<<recv_packet2.getAckNum()<<endl;
				if (CURRENT_ACK_NUM == recv_packet2.getSeqNum()) {
					CURRENT_ACK_NUM++; //Received FIN ACK
				}

				//Sending final ACK
				flags = 0x4; //ACK
				TcpPacket* finalAck = new TcpPacket(CURRENT_SEQ_NUM, CURRENT_ACK_NUM, INIT_CWND_SIZE, flags, data);
				cout<<"Sending final ACK w/ SEQ Num: "<<CURRENT_SEQ_NUM<<", ACK Num: "<<CURRENT_ACK_NUM<<endl<<endl;
				vector<char> finalAckVector = finalAck->buildPacket();
				if (sendto(sockfd, &finalAckVector[0], finalAckVector.size(), 0, (struct sockaddr *)&clientAddr, (socklen_t)sizeof(clientAddr)) == -1) {
					perror("send error");
					return 1;
				}

				CURRENT_SEQ_NUM++; //Only increment by 1 bc only sent FIN ACK
				delete finalAck;

				cout<<"Closing connection. TIME_WAIT state has yet to be implemented.\n\n";


			} else {
				perror("Error; expected FIN ACK packet");
				return 1;
			}
		}
		resFile.close();
		startedHandshake = false;
		establishedTCP = false;
		CURRENT_SEQ_NUM = 0;
		CURRENT_ACK_NUM = 0;

	}
	close(sockfd);
	return 0;
}
