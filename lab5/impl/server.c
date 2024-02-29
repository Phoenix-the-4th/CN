#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdio.h> 
#include <stdlib.h> 
#include <pthread.h>
#include <stdbool.h>

#define max_clients 1024

int client_id[max_clients];
char name[max_clients][1024];
int no_of_clients = 0;
pthread_t thread_id[max_clients*2]; 
int tcnt = 0;
struct group {
    char g_name[1024] ;
    int g_size;
    int g_members[1024];
};
int gcnt = 0;
struct group grps[max_clients];
char history[1024*8];
pthread_mutex_t c_lock; 
pthread_mutex_t g_lock; 
pthread_mutex_t h_lock; 

/*// Function to split a string into words using ':' as delimiter
char **splitString(const char *input, int *num_words, char delimiter) {
    char *words = (char *)malloc(1024 * sizeof(char *));
    if (words == NULL) {
        printf("Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    char *token;
    char *copy = strdup(input); // Duplicate input string to avoid modification

    token = strtok(copy, &delimiter);
    *num_words = 0;
    while (token != NULL) {
        words[*num_words] = strdup(token); // Duplicate each token
        (*num_words)++;
        if (*num_words >= 1024) {
            printf("Maximum number of words reached\n");
            break;
        }
        token = strtok(NULL, &delimiter);
    }

    free(copy); // Free the duplicated string
    return words;
}*/

void send_data(char* data, int cl_id){
    int send_count;
	if ((send_count = send(cl_id, data, strlen(data), 0)) == -1) {
		perror("Failed send");
		close(cl_id);
        exit(EXIT_FAILURE);
	}
    printf("Succesful send\n");
    fflush(stdout);
}

int create_connection(char* addr, int port) {
	int server_sockfd;
	struct sockaddr_in server_addrinfo;
	
	//socket
	if((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("server: socket");
        exit(1);
    }
	
	int yes = 1;
	if(setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){
        perror("setsockopt");
        close(server_sockfd);
        exit(1);
    }

	server_addrinfo.sin_family = AF_INET;
    server_addrinfo.sin_port = htons(port);
    server_addrinfo.sin_addr.s_addr = htonl(INADDR_ANY);

	if (inet_pton(AF_INET, addr, &server_addrinfo.sin_addr) <= 0) {
        //printf("\nInvalid address/ Address not supported \n");
        close(server_sockfd);
        exit(1);
    }

	//  BIND
	if(bind(server_sockfd, (struct sockaddr*) &server_addrinfo, sizeof(server_addrinfo)) == -1){
        perror("server: bind");
        close(server_sockfd);
        exit(1);
    }

	// LISTEN 
	if(listen(server_sockfd, 1024) == -1){ // 1024 indicates the number of pending connections
        perror("listen");
        close(server_sockfd);
        exit(1);
    }
	return server_sockfd;

}

int client_connect(int socket_id) {
	//ACCEPT
	struct sockaddr_in client_addrinfo;
	socklen_t sin_size = sizeof(client_addrinfo);
	int new_server_sockfd = accept(socket_id, (struct sockaddr*) &client_addrinfo, &sin_size);
    if(new_server_sockfd == -1){
        perror("accept");
        close(socket_id);
        exit(1);
    }

	char client_IP[INET6_ADDRSTRLEN];
    unsigned int client_port;

	//inet_ntop converts the networkaddresses from binary to text form
	//is not being used in the code
	if(inet_ntop(client_addrinfo.sin_family, &client_addrinfo.sin_addr, client_IP, sizeof(client_IP)) <= 0){
        //printf("\nAddress Conversion Error\n");
        close(socket_id);
        close(new_server_sockfd);
        exit(1);
    }
    //client_port = ntohs(client_addrinfo.sin_port);
    //printf("Server: Got connection from %s:%u\n", client_IP, client_port);
	return new_server_sockfd;

}


int set_name(int cl_id){
    char reply[1024];
    memset(reply, 0, 1024);
    int recv_count;
    if((recv_count = recv(cl_id, reply, 1024, 0)) == -1){
        perror("Failed recv");
        close(cl_id);
        exit(EXIT_FAILURE);
    }
    for (int i = 0; reply[i] != '\0'; i++) {
        if (reply[i] == '\n') {
            reply[i] = '\0';
            break;
        }
    }
    int pos = 0;
    pthread_mutex_lock(&c_lock); 
    client_id[no_of_clients] = cl_id;
    strcpy(name[no_of_clients], reply);
    pos = no_of_clients;
    no_of_clients++;
    pthread_mutex_unlock(&c_lock);
    return pos;
	
}

void * server_logic(void *i){
    int cl_id = (int) i;
    int cno = set_name(cl_id);
    char outbound[1024];
    char inbound[1024];
    while(1){
        memset(outbound, 0, 1024);
        memset(inbound, 0, 1024);
        int recv_count;
        if ((recv_count = recv(cl_id, inbound, 1024, 0)) == -1){
            perror("Failed recv");
            close(cl_id);
            exit(EXIT_FAILURE);
        }
        char temp_history[1024];
        pthread_mutex_lock(&c_lock); 
        sprintf(temp_history, "%s-%s", name[cno], inbound);
        strcat(history, temp_history);
        pthread_mutex_unlock(&c_lock);
        if (strcmp(inbound,"LIST\n")==0||strcmp(inbound,"LIST")==0) {
            for (int i = 0; i < max_clients; ++i) {
                if (client_id[i] != -1) {
                    strcat(outbound, name[i]);
                    strcat(outbound, ":");
                }
            }
            outbound[strlen(outbound) - 1] = '\n';
            send_data(outbound, cl_id);
            fflush(stdout);
        }
        else if (strncmp(inbound,"MSGC",4)==0) {
            char msgc[1024], rcv_name[1024], msg[1024];
            sscanf(inbound, "%[^:]:%[^:]:%[^\n]", msgc, rcv_name, msg);
            int pos = -1;
            for (int i = 0; i < max_clients; i++) {
                if (strcmp(name[i], rcv_name) ==0 ) {
                    pos = i;
                }
            }
            if (pos ==-1) {
                strcat(outbound, "USER NOT FOUND\n");
                send_data(outbound, cl_id);
                continue;
            }
            strcat(outbound, name[cno]);
            strcat(outbound, ":");
            strcat(outbound, msg);
            outbound[strlen(outbound) ] = '\n';
            send_data(outbound, client_id[pos]);
            fflush(stdout);
        }
        else if (strncmp(inbound, "GRPS", 4) == 0) {
            char cmd[1024], grp_members[1024], grp_name[1024];
            sscanf(inbound, "%[^:]:%[^:]:%[^\n]", cmd, grp_members, grp_name);
            struct group temp_grp;
            strcpy(temp_grp.g_name,grp_name);
            temp_grp.g_size= 0;
            bool valid_group = true;
            printf("grp members: %s",grp_members);
            char *members = strtok(grp_members, ",");
            while (members!=NULL ) {
                printf("%s\n",members);
                fflush(stdout);
                int pos = -1;
                for (int i = 0; i < max_clients; i++) {
                    if (strcmp(name[i], members)==0) {
                        pos = i;
                    }
                }
                bool valid_client = true;
                if (pos ==-1) {
                    valid_client = false;
                }
                if (client_id[pos] == -1) {
                    valid_client = false;
                }
                if (!valid_client) {
                    valid_group = false;
                    printf("INVALID USERS LIST\n");
                    fflush(stdout);
                    strcat(outbound, "INVALID USERS LIST\n");
                    send_data(outbound, cl_id);
                    break;
                }
                temp_grp.g_members[temp_grp.g_size] = client_id[pos];
                temp_grp.g_size++;
                members = strtok(NULL, ",");
            }
            if (!valid_group) continue;
            pthread_mutex_lock(&g_lock);
            int pos = -1;
            for (int i = 0; i < max_clients; i++) {
                if (strcmp(grps[i].g_name, temp_grp.g_name)==0) {
                    pos = i;
                }
            }
            if(pos ==-1) {
                pos = gcnt;
                gcnt++;
            }
            strcpy(grps[pos].g_name, temp_grp.g_name);
            for (int i =0 ; i <max_clients;i++) {
                grps[pos].g_members[i] = -1;
            }
            for (int i =0 ; i < temp_grp.g_size; i++) {
                grps[pos].g_members[i] = temp_grp.g_members[i];
            }
            grps[pos].g_size = temp_grp.g_size;
            pthread_mutex_unlock(&g_lock);
            sprintf(outbound,"GROUP %s CREATED\n",grp_name);
            send_data(outbound,cl_id);
        }
        else if (strncmp(inbound, "MCST", 4) == 0) {
            char cmd[1024], grp_name[1024], grp_msg[1024];
            sscanf(inbound, "%[^:]:%[^:]:%[^\n]", cmd, grp_name, grp_msg);
            int pos = -1;
            for (int i = 0; i < max_clients; i++) {
                if(strcmp(grps[i].g_name, grp_name)==0){
                    pos = i;
                }
            }
            if (pos == -1) {
                sprintf(outbound, "GROUP %s NOT FOUND\n", grp_name);
                send_data(outbound,cl_id);
            }
            for (int i = 0; i < grps[pos].g_size; i++) {
                int pos_temp = -1;
                for (int i = 0; i < max_clients; i++) {
                    if (client_id[i] == grps[pos].g_members[i]) {
                        pos_temp = i;
                    }
                }
                printf("%d\n",pos_temp);
                if (pos_temp == -1) continue;
                sprintf(outbound, "%s:%s\n", name[cno], grp_msg);
                send_data(outbound,grps[pos].g_members[i]);
            }
        }
        else if(strncmp(inbound, "BCST", 4) == 0){
            char cmd[1024],  brd_msg [1024];
            sscanf(inbound, "%[^:]:%[^\n]", cmd, brd_msg);
            for(int i =0; i < no_of_clients; i++){
                if (client_id[i] == -1) continue;
                if (client_id[i] == cl_id) continue;
                sprintf(outbound, "%s:%s\n", name[cno], brd_msg);
                send_data(outbound, client_id[i]);
            }
        }
        else if(strcmp(inbound, "HIST\n") == 0){
            send_data(history, cl_id);
        }
        else if(strcmp(inbound, "EXIT\n") == 0){
            client_id[cno] = -1;
        }
        else {
            send_data("INVALID COMMAND\n", cl_id);
        }
    }
}

int main(int argc, char *argv[])
{   
    if (argc != 3)
	{
		printf("Use 2 cli arguments\n");
		return -1;
	}
    //ptr = fopen("./Hello.txt", "w"); 
    
    for(int i =0 ; i<max_clients; i++){
        client_id[i] = -1;
    }
	
	// extract the address and port from the command line arguments
	// extract the address and port from the command line arguments
	char addr[INET6_ADDRSTRLEN];
	strcpy(addr, argv[1]);
	int port = atoi(argv[2]);

    int socket_id = create_connection(addr, port);
    while(1){
        if (no_of_clients <= max_clients) {
            int cl_id =  client_connect(socket_id);
            pthread_t tid; 
            thread_id[tcnt]= tid;
            tcnt++;
            pthread_create(&tid, NULL, server_logic, (void *) cl_id); 
        }
    }
    for (int t = 0; t < tcnt; t++) {
        pthread_join(thread_id[t], NULL);  
    }
    close(socket_id);
    return 0;    
}