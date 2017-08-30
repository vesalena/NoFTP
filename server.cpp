#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <ctime>
#include <time.h>
#include <map>
#include <vector>
#include "packet.h"

std::map<uint64_t, File> files;

int fill_packet(const Packet in, Packet *out, uint32_t total, uint32_t crc) {
    memset(out, 0, sizeof(*out));
    out->seq_number = ntohl(in.seq_number);
    out->seq_total = ntohl(total);
    out->type = ACK;
    out->id = in.id;
    if (crc) {
        uint32_t c = ntohl(crc);
        memcpy(out->data, &c, sizeof(c));
    }
    int bytes = Header_size;
    if (crc)
        bytes += sizeof(crc);
    return bytes;
}

void print(Packet pkt) {
    std::cout << "server: seq_number = " << pkt.seq_number << std::endl;
    std::cout << "server: seq_total  = " << pkt.seq_total << std::endl;
    std::cout << "server: type       = " << pkt.type << std::endl;
    std::cout << "server: id         = " << pkt.id << std::endl;
}

int main()
{
    int fd_sock;
    struct sockaddr_in addr, addr_to;

    // create and bind socket
    fd_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_sock < 0) {
        std::cout << "server: unable to create socket" << std::endl;
        return 1;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(Server_port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(fd_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cout << "server: unable to bind socket" << std::endl;
        return 1;
    }

    unsigned int addr_size = sizeof(addr_to);

    // receive message
    int bytes_read, bytes_write;
    Packet output, input;
    while (1)
    {
        bytes_read = recvfrom(fd_sock, &input, sizeof(input), 0, 
            (struct sockaddr *)&addr_to, &addr_size);
        if (read_packet(bytes_read, &input)) {
            break;
        }
        if (input.type != PUT) {
            std::cout << "server: invalid packet type" << std::endl;
            break;
        }
        std::cout << "server: receiver packet for id: " << input.id <<
                     ", seq_number = " << input.seq_number <<
                     ", seq_total = " << input.seq_total << std::endl;

        // add packet to memory
        auto &file = files[input.id];
        //print(input);
        if (file.data.size() == 0) {
            file.data.resize(input.seq_total * Data_size);
            file.chunks.resize(input.seq_total+1, -1);
        }
        if (file.chunks[input.seq_number] == -1) {
            file.chunks[input.seq_number] = 0;
            if (bytes_read < Data_size) {
                file.data.resize(file.data.size() - Data_size + bytes_read - Header_size);
            }
            memcpy(&file.data[(input.seq_number - 1) * Data_size], input.data, bytes_read - Header_size);
            file.total++;
        }
        else {
            // received packet is duplicate
        }

        // check packet end
        uint32_t crc = 0;
        if (file.total == input.seq_total) {
            std::cout << "server: calculate crc for id: " << input.id << std::endl;
            crc = crc32c(0, &file.data[0], file.data.size());
        }

        // create answer
        //std::cout << "crc = " << crc << ", size = " << file.data.size() << std::endl;
        bytes_write = fill_packet(input, &output, file.total, crc);
        
        // send something
        if (sendto(fd_sock, &output, bytes_write, 0, 
                  (struct sockaddr *)&addr_to, addr_size) < 0) {
            std::cout << "server: unable to send something" << std::endl;
        }
    }

    close(fd_sock);

    return 0;
}
