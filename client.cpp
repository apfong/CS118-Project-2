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
//const uint16_t MAX_SEQ_NUM = 30720;
const uint16_t INIT_CWND_SIZE = 1024;
const uint16_t INIT_SS_THRESH = 30720;
const uint16_t RETRANS_TIMEOUT = 500;
const uint16_t RWND_SIZE = 30720;
// basic client's receiver window can always be 30720, but server should be
// able to properly  cases when the window is reduced


bool operator< (const TcpPacket &left, const TcpPacket &right)
{
    return left.getSeqNum() < right.getSeqNum() && left.getSeqNum() > right.getSeqNum() - MAX_SEQ_NUM/2;
}


int main(int argc, char* argv[])
{
	// Accepting user input for port number and file-name
	string portstring = "4000";
	string hoststring = "10.0.0.1";
	if (argc > 1)
		portstring = argv[1];
	if (argc > 2)
		hoststring = argv[2];
	if (argc > 3) {
		cerr << "must have only 3 arguments";
		return 1;
	}
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
	//string hoststring = "10.0.0.1";
	const char * host = hoststring.c_str();
	//string portstring = "4000";
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
	set<TcpPacket> rwnd;
	uint16_t curr_window;

	int CURRENT_SEQ_NUM = 0;//rand() % MAX_SEQ_NUM // from 0->MAX_SEQ_NUM
	int CURRENT_ACK_NUM = 0;
	
	// Variables for timeout using select
	// ioctlsocket(FIONBIO)/fcntl(O_NONBLOCK), need this for non-blocking sockets?
	// need to create separate fd_set for each packet? how to set individual timeouts?
//	vector<int> packetFds;
//	packetFds.push(sockfd);
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 500000; // 500ms
	bool gotSynAck = false;
	if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(timeout)) < 0)
		cerr << "setsockopt failed\n";


	// Setting up TCP connection
	uint16_t flags = 0x02;
	vector<char> syn_data;
	TcpPacket* initTCP = new TcpPacket(CURRENT_SEQ_NUM, 0, 0, flags, syn_data);
	vector<char> initPacket = initTCP->buildPacket();
	cout << "Starting SEQ Num: " << CURRENT_SEQ_NUM << endl;
	cout << "Starting ACK Num: " << CURRENT_ACK_NUM << endl;
	if (sendto(sockfd, &initPacket[0], initPacket.size(), 0, (struct sockaddr *)&serverAddr, (socklen_t)sizeof(serverAddr)) == -1) {
		perror("send error");
		return 1;
	}

	// After sending out syn packet, keep resending if timeout reached before receiving response
	while (!establishedTCP) {
		bytesRec = recvfrom(sockfd, buf, buf_size, 0, (struct sockaddr*)&serverAddr, &serverAddrSize);
		if (bytesRec == -1) {
			if (EWOULDBLOCK) {
				if (!gotSynAck) {
					cerr << "Timed out, resending syn\n";
					if (sendto(sockfd, &initPacket[0], initPacket.size(), 0, (struct sockaddr *)&serverAddr, (socklen_t)sizeof(serverAddr)) == -1) {
						perror("send error");
						return 1;
					}
					continue;
				}
				else
					cerr << "Waiting for ACK to be received by server\n";
			} else {
				perror("error receiving");
				return 1;
			}
		} 
		if (bytesRec > 0) {
			cerr << "read 8 bytes\n";
			// Dealing with 3 way handshake headers
			vector<char> bufVec(buf, buf+buf_size);
			TcpPacket* header = new TcpPacket(bufVec);
			cout << "Receiving data packet " << header->getSeqNum() << endl;

			// if SYN=1 and ACK=1
			if (header->getSynFlag() && (header->getAckFlag())) {
				cerr << "Received TCP setup packet (SYN-ACK packet)\n";
				gotSynAck = true;
				delete initTCP;
				cerr << "Starting SEQ Num: " << CURRENT_SEQ_NUM << endl;
				CURRENT_ACK_NUM = header->getSeqNum() + 1;
				//cout << "Starting ACK Num: " << CURRENT_ACK_NUM << endl;
				cout << "Sending ACK packet " << CURRENT_ACK_NUM << endl; 
				flags = 0x04;
				vector<char> res_data;
				TcpPacket* res = new TcpPacket(CURRENT_SEQ_NUM, CURRENT_ACK_NUM, INIT_CWND_SIZE, flags, res_data);
				vector<char> resPacket = res->buildPacket();
				if (sendto(sockfd, &resPacket[0], resPacket.size(), 0, (struct sockaddr *)&serverAddr,
							(socklen_t)sizeof(serverAddr)) == -1) {
					perror("send error");
					return 1;
				}
				delete res;
			cerr << "asdlkf;jasdfjasdlf\n";
			}
			else if (gotSynAck) {
				// Received something other than a SYN-ACK packet, means connection setup
				cerr << "Established TCP connection after 3 way handshake\n";
				establishedTCP = true;
			}

			delete header;
		}
	}
	CURRENT_SEQ_NUM = (CURRENT_SEQ_NUM + 1) % MAX_SEQ_NUM;

	// Finished setting up TCP connection

	ofstream output("copiedfile.html");
	// Get rest of data: UDP packets holding TCP packets
	while((bytesRec = recvfrom(sockfd, buf, buf_size, 0, (struct sockaddr*)&serverAddr, &serverAddrSize))){
		if(bytesRec == -1){
			if (EWOULDBLOCK) {
				cerr << "Doing nothing, waiting for incoming data packets\n";
				continue;
			} else {
				perror("file receive error");
				return 1;
			}
		}
		if(bytesRec == 0){
			cerr << "no more to read"<<endl;
			break;
		}
		vector<char> recv_data(buf, buf+bytesRec);
		TcpPacket recv_packet(recv_data);// = new TcpPacket(recv_data);
		//cout<<"Received packet w/ SEQ Num: " << recv_packet.getSeqNum() <<", ACK Num: " << recv_packet.getAckNum() << endl;
		cout << "Receiving data packet " << recv_packet.getSeqNum() << endl;

		//cout << "Flags: " << recv_packet.getAckFlag() << " " << recv_packet.getSynFlag() << " " << recv_packet.getFinFlag() << endl;
		if (recv_packet.getFinFlag()) {
			cout << "----------------RECEIVED FIN ----------------------\n";
			// cout << "---NEED TO FIX BUG WHERE WE GET TOO MUCH DATA---\n";
			// cout << "----SEQ/ACK NUMS WILL BE OFF BECAUSE OF THIS----\n";
			//delete recv_packet;
			break; //for now just break; i think something's wrong with our last packet's data
		}

		if (CURRENT_ACK_NUM == recv_packet.getSeqNum()) { //Should this be something else??

			if(rwnd.empty()){
 				  		cout<<"empty buffer"<<endl;
 					  	output << recv_packet.getData();
 					  	//delete recv_packet;
 						CURRENT_ACK_NUM = (CURRENT_ACK_NUM + recv_packet.getDataSize()) % MAX_SEQ_NUM; //Bytes received
			}
			else{
				cout<<"not empty buffer"<<endl;
				rwnd.insert(recv_packet);
				set<TcpPacket>::iterator it = rwnd.begin();
			  	while(it != rwnd.end() && it->getSeqNum() == CURRENT_ACK_NUM){
			  		cout<<"writing out packet seq num: "<<it->getSeqNum()<<endl;
				  	output << it->getData();
				  	CURRENT_ACK_NUM = (CURRENT_ACK_NUM + it->getDataSize()) % MAX_SEQ_NUM;
				  	set<TcpPacket>::iterator rem = it;
				  	it++;
				  	//may be a memory leak
				  	//const TcpPacket* temp = &(*rem);
				  	//delete temp;
				  	rwnd.erase(rem);
			  	}
			  	rwnd.clear();
			}
		}
		else{   // received an out of order packet
			
			if(recv_packet.getSeqNum() > CURRENT_ACK_NUM){
				cout<< "received out of order packet, buffering" <<endl;
				rwnd.insert(recv_packet);
			}
			else{
				cout<<"duplicate/older packet dropped"<<endl;
			}
			//delete recv_packet;
		}
		//Send ACK
		//doesnt neccessarily need to be an in order packet
		//if (CURRENT_SEQ_NUM+1 == recv_packet->getAckNum()) {
		vector<char> data;
		flags = 0x4; //ACK flag

		//CURRENT_SEQ_NUM = (CURRENT_SEQ_NUM + 1) % MAX_SEQ_NUM; //Increment; only sending an ACK
		TcpPacket* ackPacket = new TcpPacket(CURRENT_SEQ_NUM, CURRENT_ACK_NUM, INIT_CWND_SIZE, flags, data);
		//cout << "Sending ACK w/ SEQ Num: " << CURRENT_SEQ_NUM << ", ACK Num: " << CURRENT_ACK_NUM << endl << endl;
		cout << "Sending ACK packet " << CURRENT_ACK_NUM << endl; 

		vector<char> ackPacketVector = ackPacket->buildPacket();
		//Send ACK to server
		if (sendto(sockfd, &ackPacketVector[0], ackPacketVector.size(), 0, (struct sockaddr *)&serverAddr, (socklen_t)sizeof(serverAddr)) == -1) {
			perror("Error while sending ACK");
			return 1;
		}
		//}
	}

	//Received FIN; send ACK
	vector<char> data;
	flags = 0x4; //ACK flag
	CURRENT_SEQ_NUM = (CURRENT_SEQ_NUM + 1) % MAX_SEQ_NUM; //Increment; only sending an ACK
	TcpPacket* ackPacket = new TcpPacket(CURRENT_SEQ_NUM, CURRENT_ACK_NUM, INIT_CWND_SIZE, flags, data);
	//cout << "Sending ACK w/ SEQ Num: " << CURRENT_SEQ_NUM << ", ACK Num: " << CURRENT_ACK_NUM << endl << endl;
	cout << "Sending ACK packet " << CURRENT_ACK_NUM << endl; 
	vector<char> ackPacketVector = ackPacket->buildPacket();
	//Send ACK to server
	if (sendto(sockfd, &ackPacketVector[0], ackPacketVector.size(), 0, (struct sockaddr *)&serverAddr, (socklen_t)sizeof(serverAddr)) == -1) {
		perror("Error while sending ACK");
		return 1;
	}
	delete ackPacket;

	//Now send FIN ACK
	flags = 0x5; //FIN ACK
	CURRENT_SEQ_NUM = (CURRENT_SEQ_NUM + 1) % MAX_SEQ_NUM;//Increment; only sending FIN ACK
	TcpPacket* finAck = new TcpPacket(CURRENT_SEQ_NUM, CURRENT_ACK_NUM, INIT_CWND_SIZE, flags, data);
	//cout << "Sending FIN ACK w/ SEQ Num: " << CURRENT_SEQ_NUM << ", ACK Num: " << CURRENT_ACK_NUM << endl;
	cout << "Sending ACK packet " << CURRENT_ACK_NUM << endl; 
	vector<char> finAckVector = finAck->buildPacket();
	//Sending ACK to server
	if (sendto(sockfd, &finAckVector[0], finAckVector.size(), 0, (struct sockaddr *)&serverAddr, (socklen_t)sizeof(serverAddr)) == -1) {
		perror("Error while sending FIN ACK");
		return 1;
	}

	//Listen for final ACK
	bool gotFinalAck = false;
	while (!gotFinalAck) {
		bytesRec = recvfrom(sockfd, buf, buf_size, 0, (struct sockaddr*)&serverAddr, &serverAddrSize);
		if (bytesRec == -1) {
			if (EWOULDBLOCK) {
				cerr << "Timed out while waiting on final ack, resending FIN-ACK\n" << finAck->getFinFlag()<< endl;
				if (sendto(sockfd, &finAckVector[0], finAckVector.size(), 0, (struct sockaddr *)&serverAddr, (socklen_t)sizeof(serverAddr)) == -1) {
					perror("Error while sending FIN ACK");
					return 1;
				}
			}
			else {
				perror("Error while listening for final ACK");
				return 1;
			}
		}
		vector<char> recv_data(buf, buf+buf_size);
		TcpPacket recv_packet(recv_data);
		//cout<<"Received final ACK w/ SEQ Num: "<<recv_packet.getSeqNum()<<", ACK Num: "<<recv_packet.getAckNum()<<endl<<endl;
		cout << "Receiving data packet " << recv_packet.getSeqNum() << endl;
		if (CURRENT_ACK_NUM == recv_packet.getSeqNum()) {
			CURRENT_ACK_NUM++; //RECEIVED ACK
		}
		break;
	}
	delete finAck;

	cout<<"Closing connection.\n\n";

	output.close();

	close(sockfd);
	return 0;
}
