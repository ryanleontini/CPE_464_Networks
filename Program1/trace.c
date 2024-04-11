#include <pcap.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

void printICMPHeader(const u_char *packet, int headerLength);

unsigned short in_cksum(unsigned short *addr,int len)
{
        register int sum = 0;
        u_short answer = 0;
        register u_short *w = addr;
        register int nleft = len;

        /*
         * Our algorithm is simple, using a 32 bit accumulator (sum), we add
         * sequential 16 bit words to it, and at the end, fold back all the
         * carry bits from the top 16 bits into the lower 16 bits.
         */
        while (nleft > 1)  {
                sum += *w++;
                nleft -= 2;
        }

        /* mop up an odd byte, if necessary */
        if (nleft == 1) {
                *(u_char *)(&answer) = *(u_char *)w ;
                sum += answer;
        }

        /* add back carry outs from top 16 bits to low 16 bits */
        sum = (sum >> 16) + (sum & 0xffff);     /* add hi 16 to low 16 */
        sum += (sum >> 16);                     /* add carry */
        answer = ~sum;                          /* truncate to 16 bits */
        return(answer);
}

void printTCPHeader(const u_char *packet, int headerLength) {
    printf("\n\n\tTCP Header\n");

    uint16_t src = *(uint16_t*)(packet + 14 + headerLength);
    src = ntohs(src);
    if (src == 80) {
        printf("\t\tSource Port:  HTTP\n");
    }
    else {
        printf("\t\tSource Port: %u\n", src);
    }
    
    uint16_t dest = *(uint16_t*)(packet + 16 + headerLength);
    dest = ntohs(dest);
    if (dest == 80) {
        printf("\t\tDest Port:  HTTP\n");
    }
    else {
        printf("\t\tDest Port:  %u\n", dest);
    }

    uint32_t sequenceNum = *(uint32_t *)(packet + 18 + headerLength);
    sequenceNum = ntohl(sequenceNum);
    printf("\t\tSequence Number: %u\n", sequenceNum);

    uint32_t ackNum = *(uint32_t *)(packet + 22 + headerLength);
    ackNum = ntohl(ackNum);
    printf("\t\tACK Number: %u\n", ackNum);

    uint8_t offset = *(uint8_t *)(packet + 26 + headerLength);
    printf("\t\tData Offset (bytes): %u\n", offset);
}

void printUDPHeader(const u_char *packet, int headerLength) {
    
    printf("\n\n\tUDP Header\n");
    uint16_t src = packet[14 + headerLength];
    uint16_t swapped = ntohs(src);
    src = swapped | packet[15 + headerLength];
    if (src == 53) {
        printf("\t\tSource Port: DNS\n");
    } 
    else {
        printf("\t\tSource Port: %u\n", src);
    }

    uint16_t dest = (ntohs(packet[16 + headerLength])) | packet[17 + headerLength];
    if (dest == 53) {
        printf("\t\tDest Port: DNS\n");
    } 
    else {
        printf("\t\tDest Port: %u\n", dest);
    }
}

void printIPHeader(const u_char *packet) {
    struct pcap_pkthdr header;
    int packetNum = 1;
    int i;
    int headerBytes;

    printf("\n\n\tIP Header\n");
    
    uint8_t firstByte = packet[14];
    uint8_t version = firstByte & 0xF0; // Header len
    int ipVersion = version / 16;
    // unsigned short checksum = in_cksum((unsigned short *)(packet + 14), ipHeaderLength);

    printf("\t\tIP Version: %u\n", ipVersion);

    uint8_t ihl = firstByte & 0x0F; // Header len
    headerBytes = ihl * 32 / 8;
    printf("\t\tHeader Len (bytes): %u\n", headerBytes);


    printf("\t\tTOS subfields:\n");
    uint8_t TOS = packet[15];
    uint8_t Diffserv = (TOS & 0xFC) / 4;
    printf("\t\t   Diffserv bits: %u\n", Diffserv);
    uint8_t ECN = (TOS & 0x3);
    printf("\t\t   ECN bits: %u\n", ECN);
    uint8_t TTL = packet[22];
    printf("\t\tTTL: %u\n", TTL);
    uint8_t Protocol = packet[23];
    if (Protocol == 0x0001) {
        printf("\t\tProtocol: ICMP\n");
    }
    else if (Protocol == 0x0006) {
        printf("\t\tProtocol: TCP\n");
    }
    else {
        printf("\t\tProtocol: Unknown\n");
    }

    unsigned short *checksum_position = (unsigned short *)(packet + 24); // 14 bytes Ethernet header + 10 bytes IP header offset
    unsigned short original_checksum = *checksum_position;
    original_checksum = ntohs(original_checksum);

    unsigned short checksum = in_cksum((unsigned short *)(packet + 14), headerBytes);
    
    if (checksum != 0) {
        printf("\t\tChecksum: Incorrect (0x%x)\n", original_checksum);
    }
    else {
        printf("\t\tChecksum: 0x%x\n", original_checksum);
    }

    printf("\t\tSender IP: "); // Print Sender IP
    for(i = 26; i < 30; i++) {
        printf("%d", packet[i]);
        if (i != 29) printf(".");
    }
    printf("\n\t\tDest IP: "); // Print Target IP
    for(i = 30; i < 34; i++) {
        printf("%d", packet[i]);
        if (i != 33) printf(".");
    }

    if (Protocol == 0x0001) {
        printICMPHeader(packet, headerBytes);
    }
    else if (Protocol == 0x0011) {
        printUDPHeader(packet, headerBytes);
    }
    else if (Protocol == 0x0006) {
        printTCPHeader(packet, headerBytes);
    }

}

void printICMPHeader(const u_char *packet, int headerLength) {
        printf("\n\n\tICMP Header");
        uint8_t ICMPType = packet[14 + headerLength];

        if (ICMPType == 8) {
            printf("\n\t\tType: Request");
        }
        else if (ICMPType == 0) {
            printf("\n\t\tType: Reply");
        }
        else {
            printf("\n\t\tType: %u", ICMPType);
        }

}

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
    printf("\n");
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

    while ((packet = pcap_next(handle, &header)) != NULL) {
        if (counter == 10) break;
        counter++;
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


        if (strcmp(typeString, "ARP") == 0) {
            printARPHeader(packet);
        } else if (strcmp(typeString, "IP") == 0) {
            printIPHeader(packet);
        }
        else {
            printf("Invalid type.");
        }
        
        printf("\n");

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
        // printf("\n");
    }
}

int main(void) {
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle;

    handle = pcap_open_offline("largeMix.pcap", errbuf);
    if (handle == NULL) {
        fprintf(stderr, "Couldn't open pcap file: %s\n", errbuf);
        return 2;
    }

    //printWholeHeader(handle);
    printEthernetHeader(handle);

    pcap_close(handle);
    return 0;

}
