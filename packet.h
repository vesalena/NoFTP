#ifndef PACKET_H
#define PACKET_H

#include <array>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

const int Udp_max_size      = 1472;
const int Server_port       = 3426;
const int Client_timeout    = 3;	// in seconds
const int Header_size       = 17;
const int Data_size         = Udp_max_size - Header_size;

#pragma pack(push, 1)
struct Packet {
    uint32_t seq_number;
    uint32_t seq_total;
    uint8_t type;
    uint64_t id;
    uint8_t data[Data_size];
};
#pragma pack(pop)

struct File {
    std::vector<uint8_t> data;
    std::vector<int> chunks;
    uint32_t total;
};

enum Packet_Type {
    ACK = 0,
    PUT = 1,
};

uint32_t crc32c(uint32_t crc, const unsigned char *buf, size_t len)
{
    int k;
    crc = ~crc;
    while (len--) {
      crc ^= *buf++;
      for (k = 0; k < 8; k++)
        crc = crc & 1 ? (crc >> 1) ^ 0x82f63b78 : crc >> 1;
    }
    return ~crc;
}

int read_packet(size_t len, Packet *pkt) {
    pkt->seq_number = htonl(pkt->seq_number);
    pkt->seq_total = htonl(pkt->seq_total);
    if (len > sizeof(*pkt)) {
        std::cout << "wrond packet size" << std::endl;
        return 1;
    }
    if (pkt->seq_number <= 0) {
      std::cout << "sequence number <= 0" << std::endl;
      return 1;
    }
    if (pkt->type == PUT && pkt->seq_number > pkt->seq_total) {
      std::cout << "sequence number > sequence total " << std::endl;
      return 1;
    }
    return 0;
}

#endif // PACKET_H