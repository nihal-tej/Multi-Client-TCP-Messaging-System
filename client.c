#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
// Time stamp Code
char *timestamp()
{
    static char q[20];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    sprintf(q, "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
    return q;
}
// Code for printing the error message
void error(char *msg)
{
    printf("[%s] ", timestamp());
    perror(msg);
    exit(1);
}
// Receiver Thread for receiving the buffer message from the client.
void *receiver(void *arg)
{
    int sockfd = *(int *)arg;
    free(arg);
    char buffer[512];
    ssize_t n;
    while (1)
    {
        bzero(buffer, sizeof(buffer));
        n = read(sockfd, buffer, sizeof(buffer) - 1);
        if (n <= 0)
        {
            printf("[%s] Server disconnected\n", timestamp());
            break;
        }
        buffer[n] = '\0';
        printf("[%s] %s\n", timestamp(), buffer);
    }

    return NULL;
}

// Main function
int main(int argc, char *argv[])
{
    int sockfd, portno;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    if (argc < 3)
    {
        fprintf(stderr, "[%s] Usage: %s hostname port\n", timestamp(), argv[0]);
        exit(1);
    }

    portno = atoi(argv[2]);

    server = gethostbyname(argv[1]);
    if (server == NULL)
        error("No such host");

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("Socket error");

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;

    memcpy(&serv_addr.sin_addr.s_addr,
           server->h_addr_list[0],
           server->h_length);

    serv_addr.sin_port = htons(portno);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("Connect error");

    printf("[%s] Connected to server!\n", timestamp());

   /* The below loop is the username input in which it checks whether the
    username is unique and only accepts the client if the username is unique */
    char name[256];
    char response[50];

    while (1)
    {
        printf("[%s] Enter username: ", timestamp());
        fgets(name, sizeof(name), stdin);
        name[strcspn(name, "\n")] = 0;

        char joinMsg[300];
        bzero(joinMsg, sizeof(joinMsg));

        strcpy(joinMsg, "join ");
        strcat(joinMsg, name);

        // sending message to the server
        write(sockfd, joinMsg, strlen(joinMsg));

        bzero(response, sizeof(response));
        // receiving message from the server
        int n = read(sockfd, response, sizeof(response) - 1);

        if (n <= 0)
        {
            printf("[%s] Server disconnected\n", timestamp());
            close(sockfd);
            exit(1);
        }

        response[n] = '\0';

        if (strncmp(response, "OK", 2) == 0)
        {
            printf("[%s] Username accepted!\n", timestamp());
            break;
        }
        else
        {
            printf("[%s] Username taken, try again\n", timestamp());
        }
    }
    // starting the receiver thread
    int *p = malloc(sizeof(int));
    *p = sockfd;
    // Initializing the receiver thread
    pthread_t tid;
    pthread_create(&tid, NULL, receiver, p);
    // Menu loop for the functionalities
    while (1)
    {
        int choice;
        printf("\n[%s]\n==== MENU ====\n", timestamp());
        printf("1 -> List users\n");
        printf("2 -> Direct Message\n");
        printf("3 -> Broadcast\n");
        printf("4 -> Rename User\n");
        printf("5 -> List Groups\n");
        printf("6 -> Create Group\n");
        printf("7 -> Join Group\n");
        printf("8 -> Rename Group\n");
        printf("9-> Message in group\n");
        printf("10-> Leave Group\n");
        printf("11 -> Delete Group\n");
        printf("12 -> Exit\n");
        printf("Enter choice: ");
        char temp[10];
        fgets(temp, sizeof(temp), stdin);
        choice = atoi(temp);
        char msg[256], user[256], final[512], joinMsg[300];
        if (choice == 1)
        {
            write(sockfd, "list", 4);
        }
        else if (choice == 2)
        {
            printf("[%s] Enter receiver: ", timestamp());
            fgets(user, sizeof(user), stdin);
            printf("[%s] Enter message: ", timestamp());
            fgets(msg, sizeof(msg), stdin);
            user[strcspn(user, "\n")] = 0;
            msg[strcspn(msg, "\n")] = 0;
            bzero(final, sizeof(final));
            snprintf(final, sizeof(final), "dm %s %s", user, msg);
            write(sockfd, final, strlen(final));
        }
        else if (choice == 3)
        {
            printf("[%s] Enter message: ", timestamp());
            fgets(msg, sizeof(msg), stdin);
            msg[strcspn(msg, "\n")] = 0;
            bzero(final, sizeof(final));
            strcpy(final, "broadcast ");
            strcat(final, msg);
            write(sockfd, final, strlen(final));
        }
        else if (choice == 5)
        {
            write(sockfd, "listgroup", 9);
        }
        else if (choice == 6)
        {
            printf("[%s] Enter group name: ", timestamp());
            fgets(name, sizeof(name), stdin);
            name[strcspn(name, "\n")] = 0;
            bzero(joinMsg, sizeof(joinMsg));
            strcpy(joinMsg, "creategroup ");
            strcat(joinMsg, name);
            write(sockfd, joinMsg, strlen(joinMsg));
        }
        else if (choice == 7)
        {
            printf("[%s] Enter group name: ", timestamp());
            fgets(name, sizeof(name), stdin);
            name[strcspn(name, "\n")] = 0;
            bzero(joinMsg, sizeof(joinMsg));
            strcpy(joinMsg, "joingroup ");
            strcat(joinMsg, name);
            write(sockfd, joinMsg, strlen(joinMsg));
        }
        else if (choice == 10)
        {
            printf("[%s] Enter group name: ", timestamp());
            fgets(name, sizeof(name), stdin);
            name[strcspn(name, "\n")] = 0;
            bzero(joinMsg, sizeof(joinMsg));
            strcpy(joinMsg, "leavegroup ");
            strcat(joinMsg, name);
            write(sockfd, joinMsg, strlen(joinMsg));
        }
        else if (choice == 11)
        {
            printf("[%s] Enter group name: ", timestamp());
            fgets(name, sizeof(name), stdin);
            name[strcspn(name, "\n")] = 0;
            bzero(joinMsg, sizeof(joinMsg));
            strcpy(joinMsg, "deletegroup ");
            strcat(joinMsg, name);
            write(sockfd, joinMsg, strlen(joinMsg));
        }
        else if (choice == 12)
        {
            write(sockfd, "exit", 4);
            printf("[%s] Disconnected\n", timestamp());
            break;
        }
        else if (choice == 4)
        {
            printf("[%s] Enter new username: ", timestamp());
            fgets(name, sizeof(name), stdin);
            name[strcspn(name, "\n")] = 0;
            char msg2[300];
            strcpy(msg2, "renameuser ");
            strcat(msg2, name);
            write(sockfd, msg2, strlen(msg2));
        }
        else if (choice == 8)
        {
            char old[256], newn[256];
            printf("[%s] Old group name: ", timestamp());
            fgets(old, sizeof(old), stdin);
            printf("[%s] New group name: ", timestamp());
            fgets(newn, sizeof(newn), stdin);
            old[strcspn(old, "\n")] = 0;
            newn[strcspn(newn, "\n")] = 0;
            char msg2[512];
            strcpy(msg2, "renamegroup ");
            strcat(msg2, old);
            strcat(msg2, " ");
            strcat(msg2, newn);
            write(sockfd, msg2, strlen(msg2));
        }
        else if (choice == 9)
        {
            printf("[%s] Enter group name: ", timestamp());
            fgets(user, sizeof(user), stdin);
            printf("[%s] Enter message: ", timestamp());
            fgets(msg, sizeof(msg), stdin);
            user[strcspn(user, "\n")] = 0;
            msg[strcspn(msg, "\n")] = 0;
            bzero(final, sizeof(final));
            strcpy(final, "dmgroup ");
            strcat(final, user);
            strcat(final, " ");
            strcat(final, msg);
            write(sockfd, final, strlen(final));
        }
        else
        {
            printf("[%s] Invalid choice\n", timestamp());
        }
    }

    pthread_cancel(tid);
    pthread_join(tid, NULL);
    close(sockfd);
    return 0;
}