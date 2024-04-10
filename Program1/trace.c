#include <pcap.h>
#include <stdio.h>
#include <string.h>

void printICMPHeader(const u_char *packet);

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
    else {
        printf("\t\tProtocol: Unknown\n");
    }

    unsigned short checksum = in_cksum((unsigned short *)(packet + 14), headerBytes);
    if (checksum != 0xFFFF) {
        printf("\t\tChecksum: Incorrect (0x%X)\n", checksum);
    }
    else {
        printf("\t\tChecksum: 0x%X\n", checksum);
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

    printICMPHeader(packet);

}

void printICMPHeader(const u_char *packet){
    int offset = 34;
    printf("\n\n\tICMP Header");
    uint8_t ICMPType = packet[0 + offset];
    if (ICMPType == 0x8) {
        printf("\n\t\tType: Request");
    }
    if (ICMPType == 0x0) {
        printf("\n\t\tType: Reply");
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


        if (strcmp(typeString, "ARP") == 0) {
            printARPHeader(packet);
        } else if (strcmp(typeString, "IP") == 0) {
            printIPHeader(packet);
        }
        else {
            printf("Invalid type.");
        }
        
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

    handle = pcap_open_offline("IP_bad_checksum.pcap", errbuf);
    if (handle == NULL) {
        fprintf(stderr, "Couldn't open pcap file: %s\n", errbuf);
        return 2;
    }

    //printWholeHeader(handle);
    printEthernetHeader(handle);

    pcap_close(handle);
    return 0;

}
