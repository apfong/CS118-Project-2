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
  TcpPacket(uint16_t s, uint16_t a, uint16_t w, uint16_t f);
  TcpPacket(vector<uint16_t> vec);
    
  uint16_t getSeqNum();
  uint16_t getAckNum();
  uint16_t getWindow();
    
  bool getAckFlag();
  bool getSynFlag();
  bool getFinFlag();
    
  vector<uint16_t> buildHeader();
    
private:
  uint16_t seq_num;
  uint16_t ack_num;
  uint16_t window;
  uint16_t flags; //xxxxxxxxxxxxxASF
  vector<char> data;
};

TcpPacket::TcpPacket(uint16_t s, uint16_t a, uint16_t w, uint16_t f) {
  seq_num = s;
  ack_num = a;
  window = w;
  flags = f;
}

TcpPacket::TcpPacket(vector<uint16_t> vec) {
  seq_num = vec[0];
  ack_num = vec[1];
  window = vec[2];
  flags = vec[3];
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

vector<uint16_t> TcpPacket::buildHeader() {
  vector<uint16_t> ret;
  ret.push_back(seq_num);
  ret.push_back(ack_num);
  ret.push_back(window);
  ret.push_back(flags);
  return ret;
}













int main() {
  TcpPacket test_flags_101(3, 9, 12, 5);
  assert(test_flags_101.getSeqNum() == 3);
  assert(test_flags_101.getAckNum() == 9);
  assert(test_flags_101.getWindow() == 12);
    
  assert(test_flags_101.getAckFlag());
  assert(!test_flags_101.getSynFlag());
  assert(test_flags_101.getFinFlag());
    
  TcpPacket test_flags_010(3, 9, 12, 2);
  assert(!test_flags_010.getAckFlag());
  assert(test_flags_010.getSynFlag());
  assert(!test_flags_010.getFinFlag());
    
  TcpPacket test(3, 9, 12, 5);
  TcpPacket test2(test.buildHeader());
    
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
