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


int main(int argc, char* argv[])
{
	// Accepting user input for port number and file-name
	int port = 4000;
	string resFilename = "index.html";
	if (argc > 1)
		port = atoi(argv[1]);
	if (argc > 2)
		resFilename = argv[2];
	if (argc > 3) {
		cerr << "must have only 3 arguments";
		return 1;
	}
	// create a socket using TCP IP
	int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sockfd == -1){
		perror("bad socket");
		return 1;
	}
	//int port = 4000;
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
	//cerr<<"start recv"<<endl;
	struct sockaddr_in clientAddr;
	socklen_t clientAddrSize = sizeof(clientAddr);
	int bytesRec = 0;
	bool establishedTCP = false;
	bool startedHandshake = false;
	uint16_t flags = 0x00;
	uint16_t cwnd_size = INIT_CWND_SIZE;
	uint32_t cwnd_pos = 0;
	uint16_t ss_thresh = INIT_SS_THRESH;
	int CURRENT_SEQ_NUM = 0;//rand() % MAX_SEQ_NUM // from 0->MAX_SEQ_NUM
	int CURRENT_ACK_NUM = 0;
	int lastAck = 0;
	bool slow_start = true;
	bool max_size_reached = false;

	// Variables for timeout using select
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 500000; // 500ms
	// List for dealing with timeout for multiple packets
	PSTList* pstList = new PSTList(sockfd, clientAddr);
	//cerr<<"server sockfd: "<<sockfd<<endl;
	//cerr<<"server clientaddr: "<<clientAddr.sin_port <<endl;

	bool synAckAcked = false;

	if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(timeout)) < 0)
		cerr << "setsockopt failed when setting it to " << timeout.tv_usec/1000 << "ms\n";

	while(true){
		if (!establishedTCP) {
			bytesRec = recvfrom(sockfd, buf, buf_size, 0, (struct sockaddr*)&clientAddr, &clientAddrSize);
			if(bytesRec == -1){
				if (EWOULDBLOCK) {
					continue;
				} 
				else {
					perror("error receiving");
					return 1;
				}
			}

			// Finished receiving header
			// Dealing with 3 way handshake headers
			vector<char> bufVec(buf, buf+MAX_PKT_LEN);
			TcpPacket* header = new TcpPacket(bufVec);

			// if SYN=1 and ACK=0
			if (header->getSynFlag() && !(header->getAckFlag())) {
				cerr << "Received SYN packet\n";
				startedHandshake = true;
				CURRENT_ACK_NUM = (header->getSeqNum() + 1) % MAX_SEQ_NUM;
				// Sending SYN-ACK
				flags = 0x06;
				vector<char> data;
				TcpPacket* res = new TcpPacket(CURRENT_SEQ_NUM, CURRENT_ACK_NUM, INIT_CWND_SIZE, flags, data);
				vector<char> resPacket = res->buildPacket();
				if (sendto(sockfd, &resPacket[0], resPacket.size(), 0, (struct sockaddr *)&clientAddr,
							(socklen_t)sizeof(clientAddr)) == -1) {
					perror("send error");
					return 1;
				}
				cout << "Sending packet " << CURRENT_SEQ_NUM << " " << cwnd_size << " " << ss_thresh << endl;
				delete res;
				//cerr << "finished sending SYN-ACK packet\n";
				continue;
			}
			else if (!header->getSynFlag() && header->getAckFlag() && startedHandshake) {
				cerr << "Established TCP connection after 3 way handshake\n";
				establishedTCP = true;
				synAckAcked = true;
				CURRENT_SEQ_NUM++;
				cerr << "Starting SEQ Num: " << CURRENT_SEQ_NUM << endl;
				CURRENT_ACK_NUM = (CURRENT_ACK_NUM + 1) % MAX_SEQ_NUM; //I think SEQ, not ACK??
				cerr << "Starting ACK Num: " << CURRENT_ACK_NUM << endl;
			}

			delete header;
			continue;
		}

		cerr << "CURACK#####: " << CURRENT_ACK_NUM << endl;

		//cerr << "Got past establishing TCP\n";

		// Prepending starting directory to requested filename
		//resFilename.insert(0, tFiledir);

		//cerr << "trying to get file from path: " << resFilename << endl;
		ifstream resFile (resFilename, ios::in|ios::binary);

		if (resFile.good()) {
			//cerr << "\n//////////////////////////////////////////////////////////////////////////////\n";
			//cerr << "\nOpened file: " << resFilename << endl;
			resFile.open(resFilename);

			ifstream resFile(resFilename, std::ios::binary);
			vector<char> payload((std::istreambuf_iterator<char>(resFile)),
					std::istreambuf_iterator<char>());
			size_t payloadSize = payload.size();
			//cerr<<"size: "<<payloadSize<<endl;

			vector<char>::iterator packetPoint = payload.begin();

			 //this will break the file up into small packets to be sent over
			while(cwnd_pos < payloadSize + 1){ // packetPoint != payload.end()){

				if(packetPoint - payload.begin() < cwnd_pos + cwnd_size){
					int end = (payload.end() - packetPoint > INIT_CWND_SIZE) ? INIT_CWND_SIZE : (payload.end() - packetPoint);
					vector<char> packet_divide(packetPoint, packetPoint + end);
					//cerr<<"packet size: "<<end<<endl;
					packetPoint += end;

					flags = 0x4; //ACK flag

					TcpPacket* tcpfile = new TcpPacket(CURRENT_SEQ_NUM, CURRENT_ACK_NUM, INIT_CWND_SIZE, flags, packet_divide);
					
					//cout<<"Sending packet w/ SEQ Num: "<< tcpfile->getSeqNum() <<", ACK Num: " << tcpfile->getAckNum() <<endl;
					cout << "Sending packet " << CURRENT_SEQ_NUM << " " << cwnd_size << " " << ss_thresh << endl;

					vector<char> tcpfile_packet = tcpfile->buildPacket();
					// Sending response object
					pstList->handleNewSend(tcpfile);

					if (sendto(sockfd, &tcpfile_packet[0], tcpfile_packet.size(), 0, (struct sockaddr *)&clientAddr,
								(socklen_t)sizeof(clientAddr)) == -1) {
						perror("send error");
						return 1;
					}
					//cout<<"packet sent, waiting for receive"<<endl;
					CURRENT_SEQ_NUM = (CURRENT_SEQ_NUM + tcpfile->getDataSize()) % MAX_SEQ_NUM;				
					//delete tcpfile;

				}

				// Listen for ACK
				timeout = pstList->getTimeout();
				//cerr << "Current timeout timer: " << timeout.tv_usec/1000 << endl;

				if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(timeout)) < 0)
					cerr << "setsockopt failed when setting it to " << timeout.tv_usec/1000 << "ms\n";

				//cerr << "Trying to receive bytes with timeout set at: " << timeout.tv_usec/1000 << endl;
				bytesRec = recvfrom(sockfd, buf, buf_size, 0, (struct sockaddr*)&clientAddr, &clientAddrSize);
				//cerr << "Finished receiving bytes: " << bytesRec << endl;
				if(bytesRec == -1){
					if (EWOULDBLOCK) {
						// One of sent packets timed out while waiting for an ACK

						TcpPacket * resendPacket = pstList->handleTimeout();
						cout << "Sending packet " << resendPacket->getSeqNum() << " " << cwnd_size << " " << ss_thresh << " Retransmission\n";
						vector<char> packetvector = resendPacket->buildPacket();
						if (sendto(sockfd, &packetvector[0], packetvector.size(), 0, (struct sockaddr *)&clientAddr,
								(socklen_t)sizeof(clientAddr)) == -1) {
							perror("send error");
							return 1;
						}
						cwnd_size = cwnd_size - cwnd_size % INIT_CWND_SIZE;
						ss_thresh = (MIN_SS_THRESH > cwnd_size/2) ? MIN_SS_THRESH : cwnd_size/2;
						cwnd_size = ss_thresh;
						slow_start = false;
						max_size_reached = false;
						continue;
					}
					else {
						perror("Error while listening for ACK");
						return 1;
					}
				}

				vector<char> recv_data(buf, buf+buf_size);
				TcpPacket recv_packet(recv_data);
				//cout<<"Received ACK w/ SEQ Num: "<< recv_packet.getSeqNum() << ", ACK Num: " << recv_packet.getAckNum() << endl << endl;
				cout << "Receiving packet " << recv_packet.getAckNum() << endl;
				pstList->handleAck(recv_packet.getAckNum());

				int temp_pos = recv_packet.getAckNum();
				if(temp_pos > lastAck){
					cwnd_pos += temp_pos - lastAck;
					lastAck = temp_pos;
				}
				else if(temp_pos < lastAck - MAX_SEQ_NUM/2){
					cwnd_pos += MAX_SEQ_NUM - (lastAck - temp_pos);
					lastAck = temp_pos;
				}
				//cerr <<"congestion window pos: " << cwnd_pos<<endl;

				if(!max_size_reached){
					if(slow_start){
						//cerr<<"increasing cwnd size"<<endl;
						cwnd_size += INIT_CWND_SIZE;
						if(cwnd_size > CWND_MAX_SIZE){
							cwnd_size = CWND_MAX_SIZE;
							max_size_reached = true;
						}
						if(cwnd_size >= ss_thresh)
							slow_start = false;
					}
					else{
						//cerr<<"congestion avoidance" <<endl;
						cwnd_size += INIT_CWND_SIZE/(cwnd_size / INIT_CWND_SIZE);
						if(cwnd_size > CWND_MAX_SIZE){
							cwnd_size = CWND_MAX_SIZE;
							max_size_reached = true;
						}
					}
				}

			}
			

			//cerr << "GOT to the end of the file\n\n";

			//Sending FIN
			flags = 0x1;
			vector<char> data;
			TcpPacket* fin = new TcpPacket(CURRENT_SEQ_NUM, CURRENT_ACK_NUM, INIT_CWND_SIZE, flags, data);
			//cout<<"Sending FIN w/ SEQ Num: "<<CURRENT_SEQ_NUM<<", ACK Num: "<<CURRENT_ACK_NUM<<endl;
			cout << "Sending packet " << CURRENT_SEQ_NUM << " " << cwnd_size << " " << ss_thresh << " FIN\n";
			vector<char> finVector = fin->buildPacket();
			if (sendto(sockfd, &finVector[0], finVector.size(), 0, (struct sockaddr *)&clientAddr, (socklen_t)sizeof(clientAddr)) == -1) {
				perror("send error");
				return 1;
			}

			// Setting normal 500ms timer
			timeout.tv_sec = 0;
			timeout.tv_usec = 500000;
			if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(timeout)) < 0)
				cerr << "setsockopt failed when setting it to 500ms\n";

			//Now in FIN_WAIT_1 State, waiting for an ACK
			bool gotAckForFin = false;
			while (!gotAckForFin) {
				bytesRec = recvfrom(sockfd, buf, buf_size, 0, (struct sockaddr*)&clientAddr, &clientAddrSize);
				if (bytesRec == -1) {
					if (EWOULDBLOCK) {
						//cerr << "Timed out when waiting for ACK for fin, resending FIN\n";
						//cerr << "Resending FIN w/ SEQ #: " << CURRENT_SEQ_NUM << ", ACK NUM: " << CURRENT_ACK_NUM << endl;
						cout << "Sending packet " << CURRENT_SEQ_NUM << " " << cwnd_size << " " << ss_thresh << " Retransmission FIN\n";
						if (sendto(sockfd, &finVector[0], finVector.size(), 0, (struct sockaddr *)&clientAddr, (socklen_t)sizeof(clientAddr)) == -1) {
							perror("send error");
							return 1;
						}
					}
					else {
						perror("Error while listening for ACK");
						return 1;
					}
				}
				else {
					vector<char> recv_data(buf, buf+bytesRec);
					TcpPacket * recv_packet = new TcpPacket(recv_data);
					if (recv_packet->getAckFlag() && recv_packet->getSeqNum() == CURRENT_ACK_NUM) {//&& recv_packet->getAckNum() == CURRENT_SEQ_NUM){ // TODO: look at what seq# to use here
						cerr << "Seq#: " << recv_packet->getSeqNum() << endl;
						cerr << "Got ack for fin\n";
						gotAckForFin = true;
						delete fin;
					}
					delete recv_packet;
				}
			}
			// Increment sequence number only on successful transmission of FIN
			CURRENT_SEQ_NUM = (CURRENT_SEQ_NUM + 1) % MAX_SEQ_NUM; //Only increment by 1 bc only sent FIN ACK


			vector<char> recv_data(buf, buf+buf_size);
			TcpPacket recv_packet(recv_data);
			//cout<<"Received ACK w/ SEQ Num: "<<recv_packet.getSeqNum()<<", ACK Num: "<<recv_packet.getAckNum()<<endl<<endl;
			cout << "Receiving packet " << recv_packet.getAckNum() << endl;
            CURRENT_ACK_NUM = (CURRENT_ACK_NUM + 1) % MAX_SEQ_NUM; //Received ACK

			//Now in FIN_WAIT_2 State, waiting for a FIN ACK
			bool gotFinAck = false;
			while(!gotFinAck) {
				bytesRec = recvfrom(sockfd, buf, buf_size, 0, (struct sockaddr*)&clientAddr, &clientAddrSize);
				if (bytesRec == -1) {
					if (EWOULDBLOCK) {
						cerr << "Doing nothing, in timeout of waiting for a FIN-ACK\n";
						continue;
					}
					else {
						perror("Error while listening for FIN ACK");
						return 1;
					}
				}
				vector<char> recv_data2(buf, buf+buf_size);
				TcpPacket recv_packet2(recv_data2);
				cout << "Receiving packet " << recv_packet2.getAckNum() << endl;
				if (recv_packet2.getFinFlag() && recv_packet2.getSeqNum() == CURRENT_ACK_NUM) {
					gotFinAck = true;
				}
			}
			CURRENT_ACK_NUM = (CURRENT_ACK_NUM + 1) % MAX_SEQ_NUM; //Received FIN ACK

            bool transferDone = false;
            bool firstSend = true;
            // Setting normal 1000ms timer
            timeout.tv_sec = 1;
            timeout.tv_usec = 500000;
            if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(timeout)) < 0)
            cerr << "setsockopt failed when setting it to 1000ms\n";
            while (!transferDone) {
                
                //Sending final ACK
                flags = 0x4; //ACK
                TcpPacket* finalAck = new TcpPacket(CURRENT_SEQ_NUM, CURRENT_ACK_NUM, INIT_CWND_SIZE, flags, data);
                //cout<<"Sending final ACK w/ SEQ Num: "<<CURRENT_SEQ_NUM<<", ACK Num: "<<CURRENT_ACK_NUM<<endl<<endl;
                if (firstSend) {
                    cout << "Sending packet " << CURRENT_SEQ_NUM << " " << cwnd_size << " " << ss_thresh << endl;
                    firstSend = false;
                } else {
                    cout << "Sending packet " << CURRENT_SEQ_NUM << " " << cwnd_size << " " << ss_thresh << " Retransmission\n";
                }
                vector<char> finalAckVector = finalAck->buildPacket();
                if (sendto(sockfd, &finalAckVector[0], finalAckVector.size(), 0, (struct sockaddr *)&clientAddr, (socklen_t)sizeof(clientAddr)) == -1) {
                    perror("send error");
                    return 1;
                }
                delete finalAck;
                
                bytesRec = recvfrom(sockfd, buf, buf_size, 0, (struct sockaddr*)&clientAddr, &clientAddrSize);
                if (bytesRec == -1) {
                    if (EWOULDBLOCK) {
                        transferDone = true;
                    }
                    else {
                        perror("Error while listening for FIN ACK");
                        return 1;
                    }
                }
                
            }
            
            

			cerr<<"Closing connection. \n\n";

			resFile.close();
		}
		
		startedHandshake = false;
		establishedTCP = false;
		cwnd_pos = 0;
		cwnd_size = INIT_CWND_SIZE;
		ss_thresh = INIT_SS_THRESH;
		slow_start = true;
		max_size_reached = false;
		CURRENT_SEQ_NUM = 0;
		CURRENT_ACK_NUM = 0;

	}
	delete pstList;
	close(sockfd);
	return 0;
}


