#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAXMSGSIZE 1024

int recieveMESG(int sockID, char* reply, size_t size, int flag) {
    int recv_count;
    if((recv_count = recv(sockID, reply, size, flag)) == -1) {
        perror("recieve name");                         //recieve username from new connection
        close(sockID);
        exit(1);
    }
    return recv_count;
}
int sendMESG(int sockID, char* msg, size_t size, int flag) {
    int send_count;
    if((send_count = send(sockID, msg,size, flag)) <= 0) {
        perror("send name");
        close(sockID);
        exit(1);
    }
    return send_count;
}

void* dataRec(void* param);

int main(int argc, char* argv[]) {
    if(argc != 3) {
        printf("Wrong number of arguments\n");
        exit(1);
    }
    char addr[INET6_ADDRSTRLEN]; memset(addr, 0, 1024);
    // char username[1024]; memset(username, 0, 1024);
    unsigned int port;
    strcpy(addr, argv[1]);
    port = atoi(argv[2]);
    // strcpy(username, argv[3]);

    int client_sockfd;
    struct sockaddr_in server_addrinfo;
    if((client_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {           //making socket
		perror("error : client socket");
		exit(1);
	}
    server_addrinfo.sin_family = AF_INET;
	server_addrinfo.sin_port = htons(port);
	if(inet_pton(AF_INET, addr, &server_addrinfo.sin_addr) <= 0) {    //converting ip from physical to network
		perror("Invalid address");
		close(client_sockfd);
		exit(1);
	}

    if((connect(client_sockfd, (struct sockaddr*) &server_addrinfo, sizeof(server_addrinfo))) == -1) {
		perror("connect");                                          //connecting to server
		close(client_sockfd);
		exit(1);
	}

    char initCMD[MAXMSGSIZE+1]; memset(initCMD,0,MAXMSGSIZE+1);
    if(fgets(initCMD, 1024, stdin) == NULL) {
        perror("fgets");
        close(client_sockfd);
        exit(1);
    }
    char initCMDsend[MAXMSGSIZE+1];
    strcpy(initCMDsend, initCMD);
    sendMESG(client_sockfd, initCMDsend, strlen(initCMDsend), 0);
    char* temp;
    char userN[MAXMSGSIZE+1];
    char ipAddrs[MAXMSGSIZE+1];
    char auth[MAXMSGSIZE+1];
    temp = strtok(initCMD, "@");
    strcpy(userN, temp);
    temp = strtok(NULL, "|");
    strcpy(ipAddrs,temp);
    in_addr_t ipTemp;
    if(inet_pton(AF_INET, ipAddrs, &ipTemp) <= 0) {    //converting ip from physical to network
		// perror("Invalid address");
        printf("Invalid ip addr\n");
		close(client_sockfd);
		exit(1);
	}


    temp = strtok(NULL,"|");
    strcpy(auth,temp);
    if(strncmp(auth, "r\n",1) == 0) {
        char passW[MAXMSGSIZE+1];
        if(fgets(passW, 1024, stdin) == NULL) {
            perror("fgets");
            close(client_sockfd);
            exit(1);
        }
        sendMESG(client_sockfd, passW, strlen(passW), 0);
    }
    // usleep(300);
    // strcpy(cmd, "LIST\n"); 
    
    // sendMESG(client_sockfd, cmd, strlen(cmd), 0);
    pthread_t tid;
    pthread_attr_t attr; 
    pthread_attr_init(&attr); 
    pthread_create(&tid, &attr, dataRec, &client_sockfd);

    while(1) {
        char cmd[MAXMSGSIZE+1]; memset(cmd, 0, MAXMSGSIZE+1);
        if(fgets(cmd, MAXMSGSIZE, stdin) == NULL) {
            perror("fgets");
            close(client_sockfd);
            exit(1);
        }
        int send_count = sendMESG(client_sockfd, cmd, strlen(cmd), 0);
        if(strcmp(cmd, "EXIT\n") == 0) {
            close(client_sockfd);
            exit(0);
        }
    }
    
    return 0;
}

void* dataRec(void* param) {    
    int conSock = *((int*) param);
    while(1) {
        char msg[MAXMSGSIZE+1]; memset(msg, 0, MAXMSGSIZE+1);
        int recv_count = recieveMESG(conSock, msg, MAXMSGSIZE, 0);
        if(recv_count == 0) {
            printf("Server exited\n");
            pthread_exit(0);
        }
        printf("%s", msg);
    }
}