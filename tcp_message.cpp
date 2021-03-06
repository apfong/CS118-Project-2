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
#include <vector>
#include <set>

using namespace std;
/*
 #include <cassert>
 #include <iostream>
 #include <sys/time.h>
 #include <netinet/in.h>
 */


// Default values for some variables
// Most are in units of bytes, except retransmission time(ms)
const uint16_t MAX_SEQ_NUM = 30720;
const uint16_t MAX_PKT_LEN = 1032;
//const uint16_t MAX_SEQ_NUM = 30720;
const uint16_t INIT_CWND_SIZE = 1024;
const uint16_t INIT_SS_THRESH = 15360;
const uint16_t MIN_SS_THRESH = INIT_CWND_SIZE;
const uint16_t CWND_MAX_SIZE = INIT_SS_THRESH;
const uint16_t RETRANS_TIMEOUT = 500;
// basic client's receiver window can always be 30720, but server should be
// able to properly handle cases when the window is reduced

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
    seq_num = ntohs((vec[0] << 8) | vec[1]);
    ack_num = ntohs((vec[2] << 8) | vec[3]);
    window = ntohs((vec[4] << 8) | vec[5]);
    flags = ntohs((vec[6] << 8) | vec[7]);
    
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
    
    uint16_t converted_seq_num = htons(seq_num);
    char first = converted_seq_num >> 8;
    char second = converted_seq_num;
    ret.push_back(first);
    ret.push_back(second);
    
    uint16_t converted_ack_num = htons(ack_num);
    first = converted_ack_num >> 8;
    second = converted_ack_num;
    ret.push_back(first);
    ret.push_back(second);
    
    uint16_t converted_window = htons(window);
    first = converted_window >> 8;
    second = converted_window;
    ret.push_back(first);
    ret.push_back(second);
    
    uint16_t converted_flags = htons(flags);
    first = converted_flags >> 8;
    second = converted_flags;
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
        cerr << "Char: " << *it << endl;
    }
    cerr << endl;
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
    ~PSTList();
    
    void handleNewSend(TcpPacket* new_packet);
    TcpPacket* handleTimeout();
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
    
    bool firstPair;
};

PSTList::PSTList(int &sockfd, sockaddr_in &clientAddr) {
    m_sockfd = sockfd;
    
    m_clientAddr = clientAddr;
    head = nullptr;
    tail = nullptr;
    numPackets = 1;
    lastAck = 0;
    firstPair = true;
}


void PSTList::handleNewSend(TcpPacket* new_packet) {
    if (head == nullptr) { //if empty list
        
        head = new PSTNode(new_packet, timestamp(), numPackets, nullptr);
        tail = head;
        
    } else { //at least one node
        
        tail->next = new PSTNode(new_packet, timestamp(), numPackets, nullptr);
        tail = tail->next;
        
    }
    
    //clean up vector of Pairs
    vector<Pair*>::iterator it = pairs.begin();
    while (it != pairs.end()) {
        if ((*it)->packetAckNum < lastAck) {
            delete (*it);
            pairs.erase(it);
        } else {
            it++;
        }
    }
    
    uint16_t ackSeqNum = (new_packet->getSeqNum() + new_packet->getDataSize()) % MAX_SEQ_NUM;
    
    if (firstPair && pairs.empty()) {
        pairs.push_back(new Pair(new_packet->getSeqNum(), 0));
        firstPair = false;
    }
    pairs.push_back(new Pair(ackSeqNum, numPackets));
    
    numPackets++;
}

TcpPacket * PSTList::handleTimeout() {
    //cerr<<"Sending packet w/ SEQ Num: "<< head->packet->getSeqNum() <<", ACK Num: " << head->packet->getAckNum() <<endl;
    TcpPacket * resendPacket = head->packet;
    
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
        //cerr<<"handleack seqnum: "<<(*it)->seqNum<<endl;
        if ((*it)->seqNum == seqNum) {
            lastAck = (*it)->packetAckNum;
            return;
        }
    }
    //Should never reach here; error messages for now
    cerr << "ERROR: SHOULD NEVER REACH THIS PART OF THE CODE (Means reached end of file?)" << endl;
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
        cerr << "ERROR: This should never happen? Timeout but empty list" << endl;
        t.tv_sec = 0;
        t.tv_usec = 0;
        return t;
    }
    
    long ms = head->timeSent + 500 - timestamp(); //How many ms left until timeout
    
    if (ms < 1) { 
        ms = 1;
    }
    
    t.tv_sec = 0;
    t.tv_usec = ms*1000;
    
    return t;
}

PSTList::~PSTList() {

    vector<Pair*>::iterator it = pairs.begin();
    while (it != pairs.end()) {
        delete (*it);
        it++;
    }

    PSTNode* curr = head;
    PSTNode* temp;
    while (curr != nullptr) {
        temp = curr->next;
        delete curr->packet;
        delete curr;
        curr = temp;
    }


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
 
 
 cerr << "Passed all tests!\n";
 
 
 
 }
 
 */






















