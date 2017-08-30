#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fstream>
#include <queue>
#include <vector>
#include <map>
#include <algorithm>
#include <string>
#include "packet.h"

const uint64_t id1 = 0x5A;
const uint64_t id2 = 0xC1;

void print(Packet pkt) {
    std::cout << "client: seq_number = " << pkt.seq_number << std::endl;
    std::cout << "client: seq_total  = " << pkt.seq_total << std::endl;
    std::cout << "client: type       = " << pkt.type << std::endl;
    std::cout << "client: id         = " << pkt.id << std::endl;
}

int add_packets(
    std::vector<uint8_t> data1, 
    std::vector<uint8_t> data2,
    size_t *last1,
    size_t *last2,
    std::vector<Packet> &result) 
{
    Packet pkt;
    size_t size = 0;
    size_t len = data1.size(); 
    uint32_t total = len / Data_size + 1;
    for (size_t i = 0; i < total && len > 0; i++) {
        pkt.seq_number = ntohl(i+1);
        pkt.seq_total = ntohl(total);
        pkt.type = PUT;
        pkt.id = id1;

        if (len > Data_size)
            size = Data_size;
        else {
            size = len;
            *last1 = size;
            memset(&pkt.data, 0, Data_size);
            // print(pkt);
            // std::cout << "i = " << i << std::endl;
            // std::cout << "size = " << size << std::endl;
            // std::cout << "len = " << len << std::endl;
            // std::cout << "+... = " << i*Data_size << std::endl;
        }
        for (size_t j = 0; j < size; j++) {
            pkt.data[j] = data1[j + i*Data_size];
        }
        len -= size;
        result.push_back(pkt);
    }
    size = 0;
    len = data2.size(); 
    total = len / Data_size + 1;
    for (size_t i = 0; i < total && len > 0; i++) {
        pkt.seq_number = ntohl(i+1);
        pkt.seq_total = ntohl(total);
        pkt.type = PUT;
        pkt.id = id2;
        if (len > Data_size)
            size = Data_size;
        else {
            size = len;
            *last2 = size;
        }
        for (size_t j = 0; j < size; j++) {
            pkt.data[j] = data2[j + i*Data_size];
        }
        len -= size;
        result.push_back(pkt);
    }
    
    return 0;
}

int read_file(std::string name, std::vector<uint8_t> &buf) {
    std::fstream file(name, std::ios::in | std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cout << "client: unable to open file \"" << name << "\"" << std::endl;
        return -1;
    }
    auto size = file.tellg();
    if (buf.size() < size)
        buf.resize(size);
    file.seekg(0);
    file.read(reinterpret_cast<char*>(&buf[0]), size);
    //std::cout << "file\": " << name << "\" size = " << buf.size() << std::endl;
    file.close();
    return buf.size();
}

int main()
{
    // create socket
    int fd_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_sock < 0) {
        std::cout << "client: unable to create socket" << std::endl;
        return 1;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(0);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(fd_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cout << "client: unable to bind socket" << std::endl;
        return 1;
    }

    // set timeout for socket
    struct timeval time;
    time.tv_sec = Client_timeout;
    time.tv_usec = 0;
    if (setsockopt(fd_sock, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time)) < 0) {
        //std::cout << "unable to set socket timeout" << std::endl;
        perror("client: Error");
    }

    // fill server address
    struct sockaddr_in addr_to;
    addr_to.sin_family = AF_INET;
    addr_to.sin_port = htons(Server_port);
    addr_to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    // read files
    std::vector<uint8_t> data1, data2;
    uint32_t len1 = read_file("file1.jpg", data1);
    uint32_t len2 = read_file("file2.jpg", data2);
    if (len1 == 0 || len2 == 0) {
        std::cout << "client: null lenght" << std::endl;
        return 1;
    }

    // cals crc for packets
    uint32_t crc1 = 0, crc2 = 0;
    crc1 = crc32c(0, &data1[0], data1.size());
    crc2 = crc32c(0, &data2[0], data2.size());
    //std::cout << "crc1 = " << crc1 << std::endl;
    //std::cout << "crc2 = " << crc2 << std::endl;

    // create packets
    std::vector<Packet> packets;
    size_t last_1, last_2;
    add_packets(data1, data2, &last_1, &last_2, packets);

    // shuffle packets
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(packets.begin(), packets.end(), g);

    // send files
    Packet input;
    for (size_t i = 0; i < packets.size(); i++) {
        Packet pkt = packets[i];

        // trying to send message
        while (1) {
            
            size_t size = sizeof(pkt);
            if (pkt.seq_number == pkt.seq_total) {
                if (pkt.id == id1)
                    size = last_1 + Header_size;
                else if (pkt.id == id2)
                    size = last_2 + Header_size;
            }
            if (sendto(fd_sock, &pkt, size, 0, (struct sockaddr*)&addr_to, sizeof(addr_to)) < 0) {
                std::cout << "client: unable to send message" << std::endl;
                return 1;
            }
            std::cout<<"client: send packet for id: "<< pkt.id <<
                       ", seq_number = " << htonl(pkt.seq_number)<<
                       ", seq_total = " << htonl(pkt.seq_total) << std::endl;
            
            // receive message
            int bytes_read = recvfrom(fd_sock, &input, sizeof(input), 0, NULL, NULL);
            if (bytes_read > 0) {
                //std::cout << "receive packet" << std::endl;
                if (read_packet(bytes_read, &input)) {
                    break;
                }
                if (input.type != ACK || input.seq_number != htonl(pkt.seq_number)) {
                    std::cout << "client: invalid packet type" << std::endl;
                    break;
                }

                // check crc
                if (input.seq_total == htonl(pkt.seq_total)) {
                    uint32_t crc;
                    memcpy(&crc, &input.data, sizeof(uint32_t));
                    crc = htonl(crc);
                    if (input.id == id1 && crc == crc1) {
                        std::cout << "client: received valid checksum for id: " << id1 << std::endl;
                    }
                    else if (input.id == id2 && crc == crc2) {
                        std::cout << "client: received valid checksum for id: " << id2 << std::endl;    
                    }
                    else {
                        std::cout << "client: invalid crc: " << crc << std::endl;    
                    }
                }
                memset(&input, 0, sizeof(input));

                break;
            }
        }
    }

    close(fd_sock);

    return 0;
}
