#include <vector>
#include <set>

using namespace std;
/*
 #include <cassert>
 #include <iostream>
 #include <sys/time.h>
 #include <netinet/in.h>
 */

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

#include <cassert>
#include <sys/time.h>

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


//==========================================//
//============TcpPacket=====================//
//==========================================//
class TcpPacket {
public:
    TcpPacket(uint16_t s, uint16_t a, uint16_t w, uint16_t f, vector<char> d);
    TcpPacket(vector<char> vec);
    
    uint16_t getSeqNum() const;
    uint16_t getAckNum() const;
    uint16_t getWindow();
    
    bool getAckFlag();
    bool getSynFlag();
    bool getFinFlag();
    
    uint16_t getDataSize() const;
    
    vector<char> buildPacket();
    string getData() const;
    
    void testPrint();
    
private:
    uint16_t seq_num;
    uint16_t ack_num;
    uint16_t window;
    uint16_t flags; //xxxxxxxxxxxxxASF
    vector<char> data;
};

TcpPacket::TcpPacket(uint16_t s, uint16_t a, uint16_t w, uint16_t f, vector<char> d) {
    seq_num = s;
    ack_num = a;
    window = w;
    flags = f;
    data = d;
}

TcpPacket::TcpPacket(vector<char> vec) {
    seq_num = (vec[0] << 8) | vec[1];
    ack_num = (vec[2] << 8) | vec[3];
    window = (vec[4] << 8) | vec[5];
    flags = (vec[6] << 8) | vec[7];
    
    //read rest of data and store into data.. have to convert from uint16 to char
    vector<char>::iterator it;
    for (it = (vec.begin()+8); it != vec.end(); it++) {
        data.push_back(*it);
    }
    
}

string TcpPacket::getData() const{
    string s(data.begin(), data.end());
    return s;
}

uint16_t TcpPacket::getSeqNum() const {
    return seq_num;
}

uint16_t TcpPacket::getAckNum() const {
    return ack_num;
}

uint16_t TcpPacket::getWindow() {
    return window;
}

bool TcpPacket::getAckFlag() {
    uint16_t mask = 1<<2;
    return (flags & mask);
}

bool TcpPacket::getSynFlag() {
    uint16_t mask = 1<<1;
    return (flags & mask);
}

bool TcpPacket::getFinFlag() {
    uint16_t mask = 1<<0;
    return (flags & mask);
}

uint16_t TcpPacket::getDataSize() const {
    return data.size();
}

vector<char> TcpPacket::buildPacket() {
    vector<char> ret;
    
    char first = seq_num >> 8;
    char second = seq_num;
    ret.push_back(first);
    ret.push_back(second);
    
    first = ack_num >> 8;
    second = ack_num;
    ret.push_back(first);
    ret.push_back(second);
    
    first = window >> 8;
    second = window;
    ret.push_back(first);
    ret.push_back(second);
    
    first = flags >> 8;
    second = flags;
    ret.push_back(first);
    ret.push_back(second);
    
    vector<char>::iterator it;
    for (it = data.begin(); it != data.end(); it++) {
        ret.push_back(*it);
    }
    
    return ret;
}

void TcpPacket::testPrint() {
    vector<char>::iterator it;
    for (it = data.begin(); it != data.end(); it++) {
        cout << "Char: " << *it << endl;
    }
    cout << endl;
}

// void TcpPacket::startTimer(){
//     thread(timeout).detach();
// }

// void TcpPacket::timeout(){
//     this_thread::sleep_for(chrono::milliseconds(500));
//     if(retransmit)
//     cerr<<"asynch timeout"<<endl;
//     exit(1);
// }


//==========================================//
//============PSTNode=======================//
//==========================================//

long timestamp() {
    timeval* t = new timeval;
    gettimeofday(t, NULL);
    long ret = ((long)t->tv_sec)*1000 + ((long)t->tv_usec)/1000;
    delete t;
    return ret;
}

class PSTNode {
public:
    PSTNode(TcpPacket* p, long t, int id, PSTNode* n);
    
    TcpPacket* packet;
    
    long timeSent;
    int packetNum;
    
    PSTNode* next;
};

PSTNode::PSTNode(TcpPacket* p, long t, int id, PSTNode* n) {
    packet = p;
    
    timeSent = t;
    packetNum = id;
    
    next = n;
}



//==========================================//
//============Pair==========================//
//==========================================//
class Pair {
public:
    Pair(uint16_t s, int p);
    
    uint16_t seqNum;
    int packetAckNum;
};

Pair::Pair(uint16_t s, int p) {
    seqNum = s;
    packetAckNum = p;
}


//==========================================//
//============PSTList=======================//
//==========================================//

class PSTList {
public:
    PSTList(int &sockfd, sockaddr_in &clientAddr);
    //TODO: DESTRUCTOR
    
    void handleNewSend(TcpPacket* new_packet);
    vector<char> handleTimeout();
    void handleAck(uint16_t seqNum);
    
    timeval getTimeout();
private:
    int m_sockfd;
    sockaddr_in m_clientAddr;
    
    PSTNode* head;
    PSTNode* tail;
    int numPackets;
    int lastAck;
    vector<Pair*> pairs;
};

PSTList::PSTList(int &sockfd, sockaddr_in &clientAddr) {
    m_sockfd = sockfd;
    
    m_clientAddr = clientAddr;
    head = nullptr;
    tail = nullptr;
    numPackets = 1;
    lastAck = 0;
}


void PSTList::handleNewSend(TcpPacket* new_packet) {
    if (head == nullptr) { //if empty list
        
        head = new PSTNode(new_packet, timestamp(), numPackets, nullptr);
        tail = head;
        
    } else { //at least one node
        
        tail->next = new PSTNode(new_packet, timestamp(), numPackets, nullptr);
        tail = tail->next;
        
    }
    
    uint16_t ackSeqNum = new_packet->getSeqNum() + new_packet->getDataSize();
    //TODO: CHECK IF IT GOES PAST MAX SEQ NUM??
    
    if (pairs.empty()) {
        pairs.push_back(new Pair(new_packet->getSeqNum(), 0));
    }
    pairs.push_back(new Pair(ackSeqNum, numPackets));
    
    numPackets++;
}

vector<char> PSTList::handleTimeout() {
    cout<<"Sending packet w/ SEQ Num: "<< head->packet->getSeqNum() <<", ACK Num: " << head->packet->getAckNum() <<endl;
    vector<char> resendPacket = head->packet->buildPacket();
    
    head->timeSent = timestamp();
    
    if (head != tail) { //only do the rest if there's two or more nodes --> need to reorder the list
        
        tail->next = head;
        tail = head;
        
        head = head->next;
        tail->next = nullptr;
        
    }
    return resendPacket;
}



void PSTList::handleAck(uint16_t seqNum) {
    
    vector<Pair*>::iterator it;
    for (it = pairs.begin(); it != pairs.end(); it++) {
        if ((*it)->seqNum == seqNum) {
            lastAck = (*it)->packetAckNum;
            return;
        }
    }
    //Should never reach here; error messages for now
    cout << "ERROR: SHOULD NEVER REACH THIS PART OF THE CODE" << endl;
}



timeval PSTList::getTimeout() {
    
    while (head != nullptr && head->packetNum <= lastAck) {
        PSTNode* temp = head;
        head = head->next;
        
        delete temp->packet;
        delete temp;
    }
    
    timeval t;
    
    if (head == nullptr) { //Calling timeout on an empty list; shouldn't happen; error message for now
        cout << "ERROR: This should never happen? Timeout but empty list" << endl;
        t.tv_sec = 0;
        t.tv_usec = 0;
        return t;
    }
    
    long ms = head->timeSent + 500 - timestamp(); //How many ms left until timeout
    
    if (ms < 0) { //Shouldn't happen, I think?
        cout << "ERROR: Timeout sould never be less than 0?" << endl;
        ms = 0;
    }
    
    t.tv_sec = 0;
    t.tv_usec = ms*1000;
    
    return t;
}



//TODO: DESTRUCTOR



/*
 
 int main() {
 
 vector<char> data;
 data.push_back('h');
 data.push_back('e');
 data.push_back('l');
 data.push_back('l');
 data.push_back('o');
 
 TcpPacket test_flags_101(3, 9, 12, 5, data);
 assert(test_flags_101.getSeqNum() == 3);
 assert(test_flags_101.getAckNum() == 9);
 assert(test_flags_101.getWindow() == 12);
 
 assert(test_flags_101.getAckFlag());
 assert(!test_flags_101.getSynFlag());
 assert(test_flags_101.getFinFlag());
 
 TcpPacket test_flags_010(3, 9, 12, 2, data);
 assert(!test_flags_010.getAckFlag());
 assert(test_flags_010.getSynFlag());
 assert(!test_flags_010.getFinFlag());
 
 TcpPacket test(3, 9, 12, 5, data);
 TcpPacket test2(test.buildPacket());
 test2.testPrint();
 
 assert(test.getSeqNum() == 3);
 assert(test.getAckNum() == 9);
 assert(test.getWindow() == 12);
 
 assert(test.getAckFlag());
 assert(!test.getSynFlag());
 assert(test.getFinFlag());
 
 assert(test2.getSeqNum() == 3);
 assert(test2.getAckNum() == 9);
 assert(test2.getWindow() == 12);
 
 assert(test2.getAckFlag());
 assert(!test2.getSynFlag());
 assert(test2.getFinFlag());
 
 
 cout << "Passed all tests!\n";
 
 
 
 }
 
 */






















