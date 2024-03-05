#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#define MAXCLIENTS 1024

char grpNames[1024][1024];
int grpIndex[1024];
char grpUsers[1024][1024][1024];
int grps = 0;

char hist[1024+1];

char clientNames[MAXCLIENTS][1024];
char ipAddr[MAXCLIENTS][1024];
pthread_t threadID[MAXCLIENTS];
int clientSocks[MAXCLIENTS];
bool active[MAXCLIENTS];
int root[MAXCLIENTS];
char passwd[1024];

sem_t enterCS;
sem_t updtClientNum;
sem_t updtStatus;
int clientNum = 0;


struct data{
    int indx;
    int socket;
};

int recv_data(int socketID, char* reply, int size, int flag) {
    int recv_count;
    if((recv_count = recv(socketID, reply, size, flag)) == -1) {
        perror("recieve call");                         
        close(socketID);
        exit(1);
    }
    return recv_count;
}
int send_data(int socketID, char* msg, int size, int flag) {
    int send_count;
    if((send_count = send(socketID, msg,size, flag)) == -1) {
        perror("send call");
        close(socketID);
        exit(1);
    }
    return  send_count;
}

int getClientIndex(char* findName){
    // int curClients = getSetClientNum(0,0);
    int curClients = clientNum;
    int j;
    for(j=0; j<curClients; j++) {
        if(strcmp(findName, clientNames[j]) == 0) break;
    }
    if(j == curClients) return -1;
    else return j;
}
int getGroupIndex(char* findName) {
    int curGrps = grps;
    int j;
    for(j=0; j<curGrps; j++) {
        if(strcmp(findName, grpNames[j]) == 0) break;
    }
    if(j == curGrps) return -1;
    else return j;
}
int srchUserInGrp(int indGr, char* userN) {
    int k;
    int numUsersInGrp = grpIndex[indGr];
    for(k=0; k<numUsersInGrp; k++) {
        if(strcmp(userN, grpUsers[indGr][k]) == 0) {
            break;
        }
    }
    if(k==numUsersInGrp) return -1;
    else return k;
}


void* runner(void* args) {
    struct data* info = ((struct data*) args);
    int conSock = info->socket;
    int indx = info->indx;
    
    while(1) {
        char msg[1024]; memset(msg, 0, 1024); 
        int recv_count = recv_data(conSock, msg, 1023, 0);
        sem_wait(&enterCS);

        char histEntry[2500]; memset(histEntry, 0, 2500);
        sprintf(histEntry, "%s-%s", clientNames[indx], msg);
        strcat(hist, histEntry);

        char cmd[1024];
        char tok1[1024];
        char tok2[1024];
        char* temp = strtok(msg, ":");
        if(temp != NULL) strcpy(cmd, temp);
        temp = strtok(NULL, ":");
        if(temp != NULL) strcpy(tok1, temp);
        temp = strtok(NULL, ":");
        if(temp != NULL) strcpy(tok2, temp);
        

        if(strncmp(cmd, "LIST\n",4) == 0) {
            char listOfClients[1024]; 
            int status = makeList(listOfClients);
            send_data(conSock, listOfClients, strlen(listOfClients), 0);
        }
        else if(strncmp(cmd, "MSGC", 4) == 0) {
            bool destExist = false;
            int curClients = clientNum;
            char* destName = tok1;
            int destSock;
            int destInd;
            for(int i=0; i<curClients; i++) {
                if(strcmp(tok1, clientNames[i]) == 0 && active[i]) {
                    destExist = true;
                    destSock = clientSocks[i];
                    destInd = i;
                    break;
                }
            }
            char reply[2500]; memset(reply, 0, 2500);
            if(!destExist) {
                strcpy(reply, "USER NOT FOUND\n");
                send_data(conSock, reply, strlen(reply), 0);
            }
            else {
                sprintf(reply, "%s:%s",clientNames[indx], tok2);
                send_data(destSock, reply, strlen(reply), 0);

                char fileName[3000]; memset(fileName,0,3000);
                if(destInd > indx) sprintf(fileName, "01_%s-%s.txt",destName,clientNames[indx]);
                else sprintf(fileName, "01_%s-%s.txt",clientNames[indx],destName);
                FILE* logFile = fopen(fileName, "a+");
                fputs(reply, logFile);
                fflush(logFile);
                fclose(logFile);
            }
            
        }
        else if(strncmp(cmd, "GRPS",4) == 0) {
            char* userList = tok1;
            char* grpName = tok2;
            grpName[strlen(grpName)-1] = '\0';
            int status = insert(grpName, userList);
            char reply[2500]; memset(reply, 0, 2500);
            if(status == -1) {
                strcpy(reply, "INVALID USERS LIST\n");
                send_data(conSock, reply, strlen(reply), 0);
            }
            else {
                sprintf(reply, "GROUP %s CREATED\n", grpName);
                send_data(conSock, reply, strlen(reply), 0);
            }
        }
        else if(strncmp(cmd, "MCST", 4) == 0) {
            char* grpName = tok1;
            char* msg = tok2;
            char sendData[2500]; memset(sendData,0,2500);
            sprintf(sendData, "%s:%s", clientNames[indx],msg);
            int status = mcst(grpName, sendData, indx);
            if(status == -1) {
                char reply[2500]; memset(reply, 0, 2500);
                sprintf(reply, "GROUP %s NOT FOUND\n", grpName);
                send_data(conSock, reply, strlen(reply), 0);
            }
        }
        else if(strncmp(cmd, "BCST", 4) == 0) {
            char* msg = tok1;
            char sendData[2500]; memset(sendData,0,2500);
            sprintf(sendData, "%s:%s",clientNames[indx],msg);
            int status = bcst(sendData, indx);
        }
        else if(strncmp(cmd, "HIST\n",4) == 0) {
            send_data(conSock, hist, strlen(hist), 0);
        }
        else if(strncmp(cmd, "EXIT\n",4) == 0) {
            active[indx] = false;
            root[indx] = 0;                          //check

            char listOfClients[1024];
            int status0 = makeList(listOfClients);
            int status = bcstLIST(listOfClients);

            sem_post(&enterCS);
            close(conSock);
            pthread_exit(0);
        }
        else if(strncmp(cmd, "CAST", 4) == 0) {
            char *msg = tok1;
            char sendData[2500]; memset(sendData,0,2500);
            sprintf(sendData, "%s:%s",clientNames[indx],msg);
            int status = cast(sendData, indx);
        }
        else if(strncmp(cmd, "HISF", 4) == 0) {
            if(root[indx] != 2) {
                char unAuthReply[1024]; memset(unAuthReply, 0, 1024);
                strcpy(unAuthReply, "EROR:UNAUTHORIZED\n");
                send_data(conSock, unAuthReply, strlen(unAuthReply), 0);
            }
            else {
                char* hisfCMD = tok1;
                char opT[1024];
                char* temp = strtok(hisfCMD, "|");
                strcpy(opT, temp);
                if(strncmp(opT, "-t 03\n",5) == 0) {
                    char fileName[100]; strcpy(fileName,"03_bcst.txt");
                    sendFile(fileName, indx);
                }
                else if(strncmp(opT,"-t 01",5) == 0) {
                    char userN[1024];
                    temp = strtok(NULL, " ");
                    temp = strtok(NULL, " ");
                    strcpy(userN, temp);
                    userN[strlen(userN)-1] = '\0';
                    int userInd = getClientIndex(userN);

                    char fileName[3000]; memset(fileName,0,3000);
                    if(userInd > indx) sprintf(fileName, "01_%s-%s.txt",userN,clientNames[indx]);
                    else sprintf(fileName, "01_%s-%s.txt",clientNames[indx],userN);
                    sendFile(fileName, indx);
                }
                else {
                    char grpN[1024];
                    temp = strtok(NULL, " ");
                    temp = strtok(NULL, " ");
                    strcpy(grpN, temp);
                    grpN[strlen(grpN)-1] = '\0';
                    int curGrpInd = getGroupIndex(grpN);
                    int userInd = srchUserInGrp(curGrpInd, clientNames[indx]);
                    if(userInd == -1) {
                        char unAuthReply[1000]; strcpy(unAuthReply, "EROR:UNAUTHORIZED\n");
                        send_data(conSock, unAuthReply, strlen(unAuthReply), 0);
                    }
                    else {
                        char fileName[3000]; memset(fileName,0,3000);
                        sprintf(fileName, "02_%s.txt", grpN);
                        sendFile(fileName, indx);
                    }
                }
            }
        }
        else {
            char reply[1024]; memset(reply,0,1024);
            strcpy(reply, "EROR\n"); 
            send_data(conSock, reply, strlen(reply), 0);
        }
        sem_post(&enterCS);
    }
    pthread_exit(0);
}

int insert(char* grpName, char* userList) {
    char tempUsers[1024][1024];
    char* temp;
    char* tokStr = userList;
    int indx = 0;
    while((temp = strtok(tokStr,",")) != NULL) {
        strcpy(tempUsers[indx], temp);
        indx += 1;
        tokStr = NULL;
    }
    for(int i=0; i<indx; i++) {
        int clientIndx = getClientIndex(tempUsers[i]);
        if(clientIndx == -1) return -1;
        if(active[clientIndx] != true) return -1;
    }

    int i;
    for(i=0; i<grps; i++) {
        if(strcmp(grpName, grpNames[i]) == 0) break;
    }
    if(i < grps) {
        for(int k=0; k<1024; k++) memset(grpUsers[i][k], 0, 1024);
    }
    else grps+=1;

    for(int k=0; k<indx; k++) {
        strcpy(grpUsers[i][k], tempUsers[k]);
    }
    grpIndex[i] = indx;
    strcpy(grpNames[i], grpName); 
    return 1;
}

int mcst(char* grpName, char* msg, int selfIndx) {
    int i;
    for(i=0; i<grps; i++) {
        if(strcmp(grpNames[i], grpName) == 0) break;
    }
    if(i == grps) return -1;
    for(int k=0; k<grpIndex[i]; k++) {
        int clientID = getClientIndex(grpUsers[i][k]);
        if(active[clientID] && (clientID != selfIndx)) {
            send_data(clientSocks[clientID], msg, strlen(msg), 0);
            
        }
    }
    char fileName[3000];
    sprintf(fileName, "02_%s.txt", grpName);
    FILE* logFile = fopen(fileName, "a+");
    fputs(msg, logFile);
    fflush(logFile);
    fclose(logFile);
    return 1;
}

int bcst(char* msg, int selfIndx) {
    int curClients = clientNum;
    for(int i=0; i<curClients; i++) {
        if(active[i] && (i != selfIndx)) {
            send_data(clientSocks[i], msg, strlen(msg), 0);
        }
    }
    FILE* logFile = fopen("03_bcst.txt", "a+");
    fputs(msg, logFile);
    fflush(logFile);
    fclose(logFile);
    return 1;
}
int cast(char* msg, int selfIndx) {
    int curClients = clientNum;
    int selfAuth = root[selfIndx];
    for(int i=0; i<curClients; i++) {
        if(active[i] && (i!=selfIndx) && (selfAuth == root[i])) {
            send_data(clientSocks[i], msg, strlen(msg), 0);
        }
    }
    return 1;
}
int bcstLIST(char* msg) {
    int curClients = clientNum;
    for(int i=0; i<curClients; i++) {
        if(active[i]) {
            send_data(clientSocks[i], msg, strlen(msg), 0);
        }
    }
    return 1;
}

int makeList(char* listOfClients) {
    memset(listOfClients, 0, 1024);
    int curClients = clientNum;
    strcat(listOfClients, "LIST-");
    for(int i=0; i<curClients; i++) {
        if(active[i]) {
            strcat(listOfClients, clientNames[i]);
            strcat(listOfClients, "|");
            if(root[i] == 2) strcat(listOfClients, "r");
            else if(root[i]==1) strcat(listOfClients, "n");
            strcat(listOfClients, ":");
        }
    }
    listOfClients[strlen(listOfClients)-1] = '\n';
    return 1;
}

int sendFile(char* fileName, int destIndx) {
    char buffer[1024]; memset(buffer,0,1024);
    FILE* logFile = fopen(fileName, "r");
    if(logFile == NULL) {
        logFile = fopen(fileName, "w+");
    }
    while(fgets(buffer, 1000, logFile) != NULL) {
        send_data(clientSocks[destIndx], buffer, strlen(buffer), 0);
        memset(buffer,0,1024);
    }
    return 1;
}

int main(int argc, char* argv[]) {
    if(argc != 4) {
        printf("Wrong number of arguments\n");
        exit(1);
    }
    for(int i=0; i<MAXCLIENTS; i++) active[i] = false;
    for(int i=0; i<MAXCLIENTS; i++) root[i] = 0;
    memset(hist, 0, 1024);

    int server_sockfd;
	struct sockaddr_in server_addrinfo;
	int yes = 1;
    unsigned int port = atoi(argv[2]);
    char addr[INET6_ADDRSTRLEN];
    strcpy(addr, argv[1]);
    // char* buf = argv[3];
    // buf[strlen(buf)-1] = '\0';
    strcpy(passwd, argv[3]);

    if((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("server: socket");
        exit(1);
	}
	if(setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){
        perror("setsockopt");
        close(server_sockfd);
        exit(1);
    }

    server_addrinfo.sin_family = AF_INET;
    server_addrinfo.sin_port = htons(port);
    if(inet_pton(AF_INET, addr, &server_addrinfo.sin_addr) <= 0) {
		perror("Invalid address");
        close(server_sockfd);
        exit(1);
	}                                               //bind socket to port
    if(bind(server_sockfd, (struct sockaddr*) &server_addrinfo, sizeof(server_addrinfo)) == -1) {
		perror("error: bind");
		close(server_sockfd);
        exit(1);
	}

    if(listen(server_sockfd, 1024) == -1) {			//server listening
		perror("error: listen");
        close(server_sockfd);
        exit(1);
	}
    

    if(sem_init(&enterCS, 0, 1) == -1) {
        perror("sem init");
        exit(1);
    }
    if(sem_init(&updtClientNum, 0, 1) == -1) {
        perror("sem init");
        exit(1);
    }
    if(sem_init(&updtStatus, 0, 1) == -1) {
        perror("sem init");
        exit(1);
    }
    while(1) {
        struct sockaddr_in client_addrinfo; 
        socklen_t sin_size = sizeof(client_addrinfo);
        clientSocks[clientNum] = accept(server_sockfd, (struct sockaddr *) &(client_addrinfo), &sin_size);
        // printf("Con acc\n");
        
        char initCmd[1024]; memset(initCmd, 0, 1024);
        char auth[1024]; memset(auth, 0, 1024);
        char userN[1024]; memset(userN, 0, 1024);
        int recv_count;

        recv_count = recv_data(clientSocks[clientNum], initCmd, 1023, 0); //rec initial cmd

        char* temp;
        temp = strtok(initCmd, "|");
        strcpy(userN, temp);
        temp = strtok(NULL, "|");
        strcpy(auth,temp);
        strcpy(clientNames[clientNum], userN);
        if(strncmp(auth,"r\n",1) == 0) {
            printf("inside\n"); fflush(stdout);
            char passwdRec[1024]; memset(passwdRec, 0, 1024);
            recv_count = recv_data(clientSocks[clientNum], passwdRec, 1023, 0);
            passwdRec[strlen(passwdRec)-1] = '\0';
            printf("%s\n", passwdRec);
            printf("%s\n",passwd);
            fflush(stdout);
            char reply[1024]; memset(reply, 0, 1024);
            
            if(strcmp(passwdRec, passwd) != 0) {
                strcpy(reply, "EROR:PASSWORD REJECTED\n");
                int send_count = send_data(clientSocks[clientNum], reply, strlen(reply), 0);
                close(clientSocks[clientNum]);
                continue;
            }
            else {
                strcpy(reply, "PASSWORD ACCEPTED\n");
                int send_count = send_data(clientSocks[clientNum], reply, strlen(reply), 0);
                root[clientNum] = 2;
            }
        }
        else {
            root[clientNum] = 1;
        }
        

        active[clientNum] = true;
        struct data* info = (struct data*)malloc(sizeof(struct data));
        info->indx = clientNum;
        info->socket = clientSocks[clientNum];
        pthread_attr_t attr; 
        pthread_attr_init(&attr); 
        pthread_create(&threadID[clientNum], &attr, runner, (void*)info);
        pthread_detach(threadID[clientNum]);
        clientNum += 1;
        char listOfClients[1024];
        makeList(listOfClients);
        bcstLIST(listOfClients);
    }
    
    return 0;
}
