#include "header.h"

void printPacket(struct packet *p){  //always follow this format for printing the packet
    printf("{\n");
    printf("    Version: %d\n", p->version);
    printf("    Header Length: %d\n", p->headerLength);
    printf("    Total Length: %d\n", p->totalLength);
    printf("    Source Department: %d\n", p->srcDept);
    printf("    Destination Department: %d\n", p->destDept);
    printf("    CheckSum: %d\n", p->checkSum);
    printf("    Hops: %d\n", p->hops);
    printf("    Type: %d\n", p->type);
    printf("    ACK: %d\n", p->ACK);
    if(p->headerLength == 6){
        printf("    Source Campus: %d\n", p->srcCampus);
        printf("    Destination Campus: %d\n", p->destCampus);
    }
    printf("    Data: %s\n", p->data);
    printf("}\n");
}

unsigned char* serialize( struct packet* p){
    unsigned char *buffer;
    buffer = malloc(2*3 + 1024);
    memset(buffer, 0, 1030);
    //code for serialization
    int size = 0;
    char *temp;
    printPacket(p);
    printf("%d\n", p->version << 4);
    *temp = p->version << 4 + p->headerLength;
    strcat(buffer, temp);
    printf("blen %ld\n", strlen(buffer));
    buffer[size++] = p->totalLength;
    buffer[size++] = ((p->srcDept << 13) + (p->destDept << 10) + (p->checkSum)) >> 8;
    buffer[size++] = ((p->srcDept << 13) + (p->destDept << 10) + (p->checkSum)) & 0xFF;
    buffer[size++] = p->hops << 5 + p->type << 2 + p->ACK;
    if (p->headerLength == 6) {
        buffer[size++] = p->srcCampus << 4 + p->destCampus;
    }
    strcat(buffer, p->data);
    return buffer;
}

struct packet *deserialize(unsigned char* buffer){
    struct packet *p;
    p = (struct packet *)malloc(sizeof(struct packet));
    // code for deserialization
    int b1, b2, line;
    int size = 0;
    line = buffer[size++] << 8 + buffer[size++];
    p->version = (line & 0xf000) >> 12;
    p->headerLength = (line & 0x0f00) >> 8;
    p->totalLength = (line & 0x00ff);
    line = buffer[size++] << 8 + buffer[size++];
    p->srcDept = (line & 0xe000) >> 13;
    p->destDept = (line & 0x1c00) >> 10;
    p->checkSum = (line & 0x03ff);
    if (p->headerLength == 6) {
        line = buffer[size++] << 8 + buffer[size++];
        p->hops = (line & 0xe000) >> 13;
        p->type = (line & 0x1c00) >> 10;
        p->ACK = (line & 0x0300) >> 8;
        p->srcCampus = (line & 0x00f0) >> 4;
        p->destCampus = (line & 0x000f);
    }
    else {
        line = buffer[size++];
        p->hops = (line & 0xe0) >> 5;
        p->type = (line & 0x1c) >> 2;
        p->ACK = (line & 0x03);
    }
    strcpy(p->data, buffer[size]);
    return p;
}

struct packet *generatePacket(int version, int headerLength, int totalLength, 
                              int srcDept, int destDept, int checkSum, int hops, 
                              int type, int ACK, int srcCampus, int destCampus, char* data) {
    //feel free to write your own function with more customisations. This is a very basic func 
    struct packet *p;
    p = (struct packet *)malloc(sizeof(struct packet));
    p->version = version;
    p->headerLength = headerLength;
    p->totalLength = totalLength;
    p->srcDept = srcDept;
    p->destDept = destDept;
    p->checkSum = checkSum;
    p->hops = hops;
    p->type = type;
    p->ACK = ACK;
    p->srcCampus = srcCampus;
    p->destCampus = destCampus;
    strcpy(p->data, data);
    return p;
}

int main(int argc, char* argv[]) {
    struct packet* p1 = generatePacket(0b0010, 6, 17, 0, 1, 65, 1, 0, 1, 0, 0, "HelloWorld!");
    struct packet* p2 = generatePacket(0b0010, 5, 18, 0, 1, 66, 1, 0, 1, 0, 0, "SanityCheck:(");
    printPacket(p1);
    printf("%d\n", p1->version);
    char *temp = serialize(p1);
    printf("%s %ld\n", temp, strlen(temp));
    printf("Here\n");
    printPacket(deserialize(temp));
    printf("\n");
    printPacket(p2);
    temp = serialize(p2);
    printf("%s\n", temp);
    printPacket(deserialize(temp));
}