typedef unsigned int u_int;
typedef unsigned short u_short;
typedef unsigned char u_char;

#include <netinet/in.h>
#include <sys/types.h>
#include <pcap/pcap.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include "checksum.h"

typedef struct {
    uint32_t sourceAddress;      
    uint32_t destinationAddress; 
    uint8_t  reserved;            
    uint8_t  protocol;            
    uint16_t tcpLength;           
} PseudoHeader;

void printEthernetHeader(pcap_t *handle);
void printIPHeader(const u_char *packet);
void printARPHeader(const u_char *packet);
void printICMPHeader(const u_char *packet, int headerLength);
void printUDPHeader(const u_char *packet, int headerLength);
void printTCPHeader(const u_char *packet, int IPHeaderLength, uint32_t IPSrc, uint32_t IPDest, uint8_t protocol);

const char* getType(const u_char *packet);

void printTCPHeader(const u_char *packet, int IPHeaderLength, uint32_t IPSrc, uint32_t IPDest, uint8_t protocol) {
    int totalSize;
    uint16_t ipTotalLength;
    uint16_t tcpLength;
    int pseudoHeaderSize;
    unsigned char *buf;
    uint16_t src;
    uint16_t dest;
    uint32_t sequenceNum;
    uint32_t ackNum;
    uint8_t offsetAndFlags;
    uint8_t offset;
    uint16_t flags;
    uint16_t windowSize;
    unsigned short checksum;
    unsigned short *checksum_position;
    unsigned short original_checksum;
    int syn;
    int rst;
    int fin;
    int ack;
    
    PseudoHeader pHeader;
    pHeader.sourceAddress = IPSrc;
    pHeader.destinationAddress = IPDest;
    pHeader.reserved = 0;
    pHeader.protocol = protocol;

    ipTotalLength = ntohs(*(uint16_t *)(packet + 14 + 2));
    tcpLength = ipTotalLength - IPHeaderLength;
    pHeader.tcpLength = htons(tcpLength);

    printf("\n\tTCP Header");

    pseudoHeaderSize = sizeof(PseudoHeader);
    totalSize = pseudoHeaderSize + tcpLength;
    buf = malloc(totalSize);
    if (!buf) {
        printf("Memory allocation failed\n");
        return;
    }
    memcpy(buf, &pHeader, pseudoHeaderSize);
    memcpy(buf + pseudoHeaderSize, packet + 14 + IPHeaderLength, tcpLength); /* Copy TCP header and data */

    printf("\n");

    src = *(uint16_t*)(packet + 14 + IPHeaderLength);
    src = ntohs(src);
    if (src == 80) {
        printf("\t\tSource Port:  HTTP\n");
    }
    else {
        printf("\t\tSource Port:  %u\n", src);
    }
    
    dest = *(uint16_t*)(packet + 16 + IPHeaderLength);
    dest = ntohs(dest);
    if (dest == 80) {
        printf("\t\tDest Port:  HTTP\n");
    }
    else {
        printf("\t\tDest Port:  %u\n", dest);
    }

    sequenceNum = *(uint32_t *)(packet + 18 + IPHeaderLength);
    sequenceNum = ntohl(sequenceNum);
    printf("\t\tSequence Number: %u\n", sequenceNum);

    ackNum = *(uint32_t *)(packet + 22 + IPHeaderLength);
    ackNum = ntohl(ackNum);
    printf("\t\tACK Number: %u\n", ackNum);

    offsetAndFlags = *(uint8_t *)(packet + 26 + IPHeaderLength);
    offset = (offsetAndFlags >> 4) & 0x0F;
    offset *= 4;
    printf("\t\tData Offset (bytes): %u\n", offset);

    flags = *(uint16_t *)(packet + 26 + IPHeaderLength);
    flags = ntohs(flags);

    syn = flags & 0x2;
    if (syn) {
        printf("\t\tSYN Flag: Yes\n");
    }
    else {
        printf("\t\tSYN Flag: No\n");
    }


    rst = flags &0x1;
    fin = flags &0x4;
    if (fin) {
        printf("\t\tRST Flag: Yes\n");
    }
    else {
        printf("\t\tRST Flag: No\n");
    }
    if (rst) {
        printf("\t\tFIN Flag: Yes\n");
    }
    else {
        printf("\t\tFIN Flag: No\n");
    }
    ack = flags &0x10;
    if (ack) {
        printf("\t\tACK Flag: Yes\n");
    }
    else {
        printf("\t\tACK Flag: No\n");
    }

    windowSize = *(uint16_t *)(packet + 28 + IPHeaderLength);
    windowSize = ntohs(windowSize);
    printf("\t\tWindow Size: %u\n", windowSize);
    
    checksum = in_cksum((unsigned short *)buf, totalSize);
    checksum_position = (unsigned short *)(packet + 14 + IPHeaderLength + 16); /* 14 bytes Ethernet header + 10 bytes IP header offset */
    original_checksum = *checksum_position;
    original_checksum = ntohs(original_checksum);

    if (checksum != 0) {
        printf("\t\tChecksum: Incorrect (0x%04x)\n", original_checksum);
    }
    else {
        printf("\t\tChecksum: Correct (0x%04x)\n", original_checksum);
    }

    free(buf);
}

void printUDPHeader(const u_char *packet, int headerLength) {
    uint16_t src;
    uint16_t swapped;
    uint16_t dest;
  
    printf("\n\tUDP Header\n");
    src = packet[14 + headerLength];
    swapped = ntohs(src);
    src = swapped | packet[15 + headerLength];
    if (src == 53) {
        printf("\t\tSource Port:  DNS\n");
    } 
    else {
        printf("\t\tSource Port:  %u\n", src);
    }

    dest = (ntohs(packet[16 + headerLength])) | packet[17 + headerLength];
    if (dest == 53) {
        printf("\t\tDest Port:  DNS\n");
    } 
    else {
        printf("\t\tDest Port:  %u\n", dest);
    }
}

void printIPHeader(const u_char *packet) {
    uint8_t firstByte;
    uint8_t version;
    uint8_t ihl;
    int ipVersion;
    uint8_t TOS;
    uint8_t Diffserv;
    uint8_t ECN;
    uint8_t TTL;
    uint8_t Protocol;
    unsigned short *checksum_position;
    unsigned short original_checksum;
    unsigned short checksum;

    uint32_t src;
    uint32_t dest;
    int i;
    int headerBytes;

    printf("\n\tIP Header\n");
    
    firstByte = packet[14];
    version = firstByte & 0xF0;
    ipVersion = version / 16;

    printf("\t\tIP Version: %u\n", ipVersion);

    ihl = firstByte & 0x0F;
    headerBytes = ihl * 32 / 8;
    printf("\t\tHeader Len (bytes): %u\n", headerBytes);


    printf("\t\tTOS subfields:\n");


    TOS = packet[15];
    Diffserv = (TOS & 0xFC) / 4;
    printf("\t\t   Diffserv bits: %u\n", Diffserv);

    ECN = (TOS & 0x3);
    printf("\t\t   ECN bits: %u\n", ECN);
    TTL = packet[22];
    printf("\t\tTTL: %u\n", TTL);
    Protocol = packet[23];
    if (Protocol == 0x0001) {
        printf("\t\tProtocol: ICMP\n");
    }
    else if (Protocol == 0x0006) {
        printf("\t\tProtocol: TCP\n");
    }
    else if (Protocol == 0x0011) {
        printf("\t\tProtocol: UDP\n");
    }
    else {
        printf("\t\tProtocol: Unknown\n");
    }

    checksum_position = (unsigned short *)(packet + 24);
    original_checksum = *checksum_position;
    original_checksum = ntohs(original_checksum);

    checksum = in_cksum((unsigned short *)(packet + 14), headerBytes);
    
    if (checksum != 0) {
        printf("\t\tChecksum: Incorrect (0x%04x)\n", original_checksum);
    }
    else {
        printf("\t\tChecksum: Correct (0x%04x)\n", original_checksum);
    }

    printf("\t\tSender IP: ");
    for(i = 26; i < 30; i++) {
        printf("%d", packet[i]);
        if (i != 29) printf(".");
    }
    printf("\n\t\tDest IP: ");
    for(i = 30; i < 34; i++) {
        printf("%d", packet[i]);
        if (i != 33) printf(".");
    }
    printf("\n");

    src = (*(uint32_t *)(packet + 26));
    dest = (*(uint32_t *)(packet + 30));

    if (Protocol == 0x0001) {
        printICMPHeader(packet, headerBytes);
    }
    else if (Protocol == 0x0011) {
        printUDPHeader(packet, headerBytes);
    }
    else if (Protocol == 0x0006) {
        printTCPHeader(packet, headerBytes, src, dest, Protocol);
    }

}

void printICMPHeader(const u_char *packet, int headerLength) {
        uint8_t ICMPType;
        printf("\n\tICMP Header");
        ICMPType = packet[14 + headerLength];

        if (ICMPType == 8) {
            printf("\n\t\tType: Request");
        }
        else if (ICMPType == 0) {
            printf("\n\t\tType: Reply");
        }
        else {
            printf("\n\t\tType: %u", ICMPType);
        }

        printf("\n");

}

void printARPHeader(const u_char *packet) {
    uint16_t opcode;
    int i;

    printf("\n\tARP header\n");
    opcode = ntohs(*(uint16_t *)(packet + 20));

    printf("\t\tOpcode: ");
    if (opcode == 0x0001) {
        printf("Request");
    }
    else if (opcode == 0x0002) {
        printf("Reply");
    }
    else {
        printf("Invalid Opcode.");
    }

    printf("\n\t\tSender MAC: ");
    for(i = 22; i < 28; i++) {
        printf("%x", packet[i]);
        if (i != 27) printf(":");
    }

    printf("\n\t\tSender IP: ");
    for(i = 28; i < 32; i++) {
        printf("%d", packet[i]);
        if (i != 31) printf(".");
    }
    printf("\n\t\tTarget MAC: ");
    for(i = 32; i < 38; i++) {
        printf("%x", packet[i]);
        if (i != 37) printf(":");
    }
    printf("\n\t\tTarget IP: ");
    for(i = 38; i < 42; i++) {
        printf("%d", packet[i]);
        if (i != 41) printf(".");
    }
    printf("\n\n");
}

const char* getType(const u_char *packet) {
    uint16_t type = ntohs(*(uint16_t *)(packet + 12));
    const char* typeString;
    switch(type) {
        case 0x0806:
            typeString = "ARP";
            break;
        case 0x0800:
            typeString = "IP";
            break;
        default:
            typeString = "Unknown";
    }
    printf("%s", typeString);
    return typeString;
}

void printEthernetHeader(pcap_t *handle) {
    const u_char *packet;
    struct pcap_pkthdr header;
    int packetNum = 1;
    int counter = 0;
    int i;
    const char* typeString;

    while ((packet = pcap_next(handle, &header)) != NULL) {
        counter++;
        printf("\n");
        printf("Packet number: %d  ", packetNum);
        printf("Packet Len: %u\n\n", header.len);
        printf("\tEthernet Header\n");

        printf("\t\tDest MAC: ");
        for(i = 0; i < 6; i++) {
            printf("%x", packet[i]);
            if (i != 5) printf(":");
        }
        
        printf("\n\t\tSource MAC: "); 
        for(i = 6; i < 12; i++) {
            printf("%x", packet[i]);
            if (i != 11) printf(":");
        }

        printf("\n\t\tType: ");
        typeString = getType(packet);
        printf("\n");

        if (strcmp(typeString, "ARP") == 0) {
            printARPHeader(packet);
        } else if (strcmp(typeString, "IP") == 0) {
            printIPHeader(packet);
        }
        else {
            printf("Invalid type.");
        }
        
        packetNum++;
    }
}

int main(int argc, char *argv[]) {
    char *filename;
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle;

    if (argc != 2) { 
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return EXIT_FAILURE;
    }

    filename = argv[1];

    handle = pcap_open_offline(filename, errbuf);
    if (handle == NULL) {
        fprintf(stderr, "Couldn't open pcap file: %s\n", errbuf);
        return 2;
    }

    printEthernetHeader(handle);

    pcap_close(handle);
    return 0;

}


