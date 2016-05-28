#include <string>
#include <vector>

using namespace std;

#include <cassert>
#include <iostream>


//==========================================//
//============TcpPacket=====================//
//==========================================//
class TcpPacket {
public:
    TcpPacket(uint16_t s, uint16_t a, uint16_t w, uint16_t f, vector<char> d);
    TcpPacket(vector<char> vec);
    
    uint16_t getSeqNum();
    uint16_t getAckNum();
    uint16_t getWindow();
    
    bool getAckFlag();
    bool getSynFlag();
    bool getFinFlag();
    
    vector<char> buildPacket();
    
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

uint16_t TcpPacket::getSeqNum() {
    return seq_num;
}

uint16_t TcpPacket::getAckNum() {
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
























