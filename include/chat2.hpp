#ifndef CHAT2_HPP
#define CHAT2_HPP

#include <algorithm>
#include <arpa/inet.h>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <mutex>
#include <ncurses.h>
#include <netdb.h>
#include <netinet/in.h>
#include <random>
#include <string>
#include <sys/socket.h>
#include <thread> #include <unistd.h>
#include <vector>

enum packet_types {
  FILE_META = 0,
  FILE_CHUNK = 1,
  MSG = 2,
  CMND = 3,
  ACK = 4,
  PRESENCE = 5,
  POLL = 6,
  PING = 7,
};

typedef struct Packet {
  uint8_t type;
  uint32_t payload_length;
  const char *payload;
} Packet;

Packet gen_packet(const std::string &message, const uint8_t &type) {
  Packet pack;

  pack.type = type;
  pack.payload_length = message.length();
  pack.payload = message.c_str();

  return pack;
}

#endif
