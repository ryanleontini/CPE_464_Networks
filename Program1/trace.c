#include "checksum.h"
#include <pcap.h>
#include <stdio.h>

void printARPHeader(const u_char *packet) {
    struct pcap_pkthdr header;
    int packetNum = 1;

        printf("\n\n\tARP header\n");
        uint16_t opcode = ntohs(*(uint16_t *)(packet + 20));

        printf("\t\tOpcode: "); // Print Opcode
        if (opcode == 0x0001) {
            printf("Request");
        }
        else if (opcode == 0x0002) {
            printf("Reply");
        }
        else {
            printf("Invalid Opcode.");
        }

        printf("\n\t\tSender MAC: "); // Print Sender MAC
        int i;
        for(i = 22; i < 28; i++) {
            printf("%x", packet[i]);
            if (i != 27) printf(":");
        }

        printf("\n\t\tSender IP: "); // Print Sender IP
        for(i = 28; i < 32; i++) {
            printf("%d", packet[i]);
            if (i != 31) printf(".");
        }
        printf("\n\t\tTarget MAC: "); // Print Target MAC
        for(i = 32; i < 38; i++) {
            printf("%x", packet[i]);
            if (i != 37) printf(":");
        }
        printf("\n\t\tTarget IP: "); // Print Target IP
        for(i = 38; i < 42; i++) {
            printf("%d", packet[i]);
            if (i != 41) printf(".");
        }
}

const char* getType(const u_char *packet) {
    uint16_t type = ntohs(*(uint16_t *)(packet + 12));
    const char* typeString;
    switch(type) {
        case 0x0806:
            typeString = "ARP";
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

    while ((packet = pcap_next(handle, &header)) != NULL) {
        printf("\n");
        printf("Packet number: %d  ", packetNum);
        printf("Packet Len: %u\n\n", header.len);
        printf("\tEthernet Header\n");

        printf("\t\tDest MAC: "); // Print first 6 bytes
        int i;
        for(i = 0; i < 6; i++) {
            printf("%x", packet[i]);
            if (i != 5) printf(":");
        }
        
        printf("\n\t\tSource MAC: "); // Print second 6 bytes
        for(i = 6; i < 12; i++) {
            printf("%x", packet[i]);
            if (i != 11) printf(":");
        }

        printf("\n\t\tType: "); // Print type
        const char* typeString;
        typeString = getType(packet);
        
        printARPHeader(packet);
        printf("\n\n");

        packetNum++;
    }
}

void printWholeHeader(pcap_t *handle) {
    const u_char *packet;
    struct pcap_pkthdr header;
    int packetNum = 1;

    while ((packet = pcap_next(handle, &header)) != NULL) {
        bpf_u_int32 i;
        for (i = 0; i < header.caplen; i++) {
            printf("%02x ", packet[i]);
            if ((i + 1) % 16 == 0) printf("\n");
        }
        printf("\n");
    }
}

int main(void) {
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle;

    handle = pcap_open_offline("ArpTest.pcap", errbuf);
    if (handle == NULL) {
        fprintf(stderr, "Couldn't open pcap file: %s\n", errbuf);
        return 2;
    }

    printEthernetHeader(handle);

    pcap_close(handle);
    return 0;

}
