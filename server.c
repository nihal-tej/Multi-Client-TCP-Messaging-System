// SERVER
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>

pthread_mutex_t lock;
// structures for client and server
struct client
{
    char name[256];
    int sock;
};

struct group
{
    char grp_name[256];
    int m;
    int sock[30];
};

struct client clients[100];
struct group groups[100];
int client_count = 0;
int group_count = 0;

// TIMEstamps
char *timestamp()
{
    static char q[20];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    sprintf(q, "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
    return q;
}

void error(char *msg)
{
    printf("[%s] ", timestamp());
    perror(msg);
    exit(1);
}

// Function for parsing
void words(char *s, char *first, char *rest)
{
    int i = 0, j = 0;
    while (s[i] && s[i] != ' ')
    {
        first[i] = s[i];
        i++;
    }
    first[i] = '\0';
    if (s[i] == ' ')
        i++;
    while (s[i])
        rest[j++] = s[i++];
    rest[j] = '\0';
}
// To get name of client by socket number
char *get_name(int sock)
{
    for (int i = 0; i < client_count; i++)
        if (clients[i].sock == sock)
            return clients[i].name;
    return NULL;
}
// To check whether the name is present or not
int is_name_taken(char *name)
{
    for (int i = 0; i < client_count; i++)
        if (strcmp(clients[i].name, name) == 0)
            return 1;
    return 0;
}

// BROADCAST to all clients including sender client
void broadcast(char *msg, int sock)
{
    pthread_mutex_lock(&lock);
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].sock != sock)
            write(clients[i].sock, msg, strlen(msg));
    }
    pthread_mutex_unlock(&lock);
}

// DM for private messaging
void send_private(int sock, char *sender, char *user, char *msg)
{
    pthread_mutex_lock(&lock);
    int found = 0;
    char final[512];
    sprintf(final, "[%s] [DM from %s]: %s\n", timestamp(), sender, msg);
    for (int i = 0; i < client_count; i++)
    {
        if (strcmp(clients[i].name, user) == 0)
        {
            write(clients[i].sock, final, strlen(final));
            found = 1;
        }
    }
    if (!found)
    {
        char err[] = "Receiver client not found\n";
        write(sock, err, strlen(err));
    }
    pthread_mutex_unlock(&lock);
}

// GROUP MESSAGE -->which sends message for all except the sender if it is in the group
void send_groupmessage(int sock, char *sender, char *grp, char *msg)
{
    pthread_mutex_lock(&lock);
    char final[512];
    sprintf(final, "[%s] [%s to group %s]: %s\n", timestamp(), sender, grp, msg);
    int found = 0;
    for (int i = 0; i < group_count; i++)
    {
        if (strcmp(groups[i].grp_name, grp) == 0)
        {
            for (int j = 0; j < groups[i].m; j++)
            {
                if (groups[i].sock[j] != sock)
                {
                    write(groups[i].sock[j], final, strlen(final));
                }
            }
            found = 1;
            break;
        }
    }
    if (!found)
        write(sock, "Group not found\n", 16);
    pthread_mutex_unlock(&lock);
}
// GROUP creation
void create_group(char *arr, int sock)
{
    pthread_mutex_lock(&lock);
    int found = 0;
    char final[512];
    for (int i = 0; i < group_count; i++)
    {
        if (strcmp(groups[i].grp_name, arr) == 0)
        {
            found = 1;
            break;
        }
    }
    // if group already found with this name then ,we will push this message
    if (found)
    {
        sprintf(final, "[%s] [group with this name is already present %s]\n", timestamp(), arr);
        write(sock, final, strlen(final));
    }
    else
    {
        // As max number of groups possible are 100
        if (group_count == 100)
        {
            sprintf(final, "[%s] [%s-cannot add as Number of groups creation had reached max!!!!]\n", timestamp(), arr);
            write(sock, final, strlen(final));
        }
        else
        {
            strcpy(groups[group_count].grp_name, arr);
            groups[group_count].sock[0] = sock;
            groups[group_count].m = 1;
            group_count++;
            sprintf(final, "[%s] [group is created %s]\n", timestamp(), arr);
            pthread_mutex_unlock(&lock);
            broadcast(final, sock);
            pthread_mutex_lock(&lock);
        }
    }
    pthread_mutex_unlock(&lock);
}
// Function for join in the group for a user
void join_group(char *arr, int sock)
{
    pthread_mutex_lock(&lock);
    int found = 0;
    char final[512];
    for (int i = 0; i < group_count; i++)
    {
        if (strcmp(groups[i].grp_name, arr) == 0)
        {
            for (int j = 0; j < groups[i].m; j++)
            {
                // If already present in group
                if (groups[i].sock[j] == sock)
                {
                    sprintf(final, "[%s] [Already present in the group %s]", timestamp(), arr);
                    write(sock, final, strlen(final));
                    found = 1;
                    break;
                }
            }
            if (!found)
            {
                found = 1;
                // Had reached max number of clienst in group
                if (groups[i].m == 30)
                {
                    sprintf(final, "[%s] [%s Number of users in group had reached max!!!!]\n", timestamp(), arr);
                    write(sock, final, strlen(final));
                    break;
                }
                // Added to group
                sprintf(final, "[%s] [Added in the group %s]\n", timestamp(), arr);
                groups[i].sock[groups[i].m] = sock;
                groups[i].m++;
                char *name = get_name(sock);
                pthread_mutex_unlock(&lock);
                send_groupmessage(sock, name, arr, final);
                pthread_mutex_lock(&lock);
                break;
            }
            break;
        }
    }
    if (!found)
    {
        sprintf(final, "[%s] [No group is present with name %s]\n", timestamp(), arr);
        write(sock, final, strlen(final));
    }
    pthread_mutex_unlock(&lock);
}
// Function for leaving from the group
void leave_group(char *arr, int sock)
{
    pthread_mutex_lock(&lock);
    int found = 0;
    char final[512];
    for (int i = 0; i < group_count; i++)
    {
        if (strcmp(groups[i].grp_name, arr) == 0)
        {
            for (int j = 0; j < groups[i].m; j++)
            {
                if (groups[i].sock[j] == sock)
                {
                    groups[i].sock[j] = groups[i].sock[groups[i].m - 1];
                    groups[i].m--;
                    sprintf(final, "[%s] [%s left from group %s]\n", timestamp(), get_name(sock),arr);
                    write(sock, final, strlen(final));
                    found = 1;
                    break;
                }
            }
        }
    }
    if (!found)
    {
        sprintf(final, "[%s] [Not present in group %s]\n", timestamp(), arr);
        write(sock, final, strlen(final));
    }
    pthread_mutex_unlock(&lock);
}
// Function for deleting the group
void delete_group(char *arr, int sock)
{
    pthread_mutex_lock(&lock);
    int found = 0;
    char final[512];
    for (int i = 0; i < group_count; i++)
    {
        if (strcmp(groups[i].grp_name, arr) == 0)
        {
            groups[i] = groups[group_count - 1];
            group_count--;
            found = 1;
            sprintf(final, "[%s] [%s group is deleted]\n", timestamp(), arr);
            pthread_mutex_unlock(&lock);
            broadcast(final, sock);
            pthread_mutex_lock(&lock);
            break;
        }
    }
    if (!found)
    {
        sprintf(final, "[%s] [%s group is not present]\n", timestamp(), arr);
        write(sock, final, strlen(final));
    }
    pthread_mutex_unlock(&lock);
}
// Functions for Renaming of user and group
void rename_user(int sock, char *newname)
{
    pthread_mutex_lock(&lock);
    if (is_name_taken(newname))
    {
        write(sock, "Name exists\n", 12);
        pthread_mutex_unlock(&lock);
        return;
    }
    char old[256];
    strcpy(old, get_name(sock));
    for (int i = 0; i < client_count; i++)
        if (clients[i].sock == sock)
        {
            strcpy(clients[i].name, newname);
            break;
        }
    char msg[512];
    sprintf(msg, "[%s] Name of %s is changed to %s\n", timestamp(), old, newname);
    pthread_mutex_unlock(&lock);
    broadcast(msg, sock);
}

void rename_group(char *old, char *new, int sock)
{
    pthread_mutex_lock(&lock);
    int found = 0;
    char msg[512];
    for (int i = 0; i < group_count; i++)
    {
        if (strcmp(groups[i].grp_name, old) == 0)
        {
            for (int j = 0; j < group_count; j++)
            {
                if (strcmp(groups[j].grp_name, new) == 0)
                {
                    found = 1;
                    sprintf(msg, "[%s] [%s Group is already present]\n", timestamp(), new);
                    write(sock, msg, strlen(msg));
                }
            }
            if (!found)
            {
                strcpy(groups[i].grp_name, new);
                sprintf(msg, "[%s] Group Name of %s is changed to %s\n", timestamp(), old, new);
                pthread_mutex_unlock(&lock);
                broadcast(msg, sock);
                pthread_mutex_lock(&lock);
                found = 1;
            }
            break;
        }
    }
    if (!found)
    {
        sprintf(msg, "[%s] [%s Group is not present]\n", timestamp(), old);
        write(sock, msg, strlen(msg));
    }
    pthread_mutex_unlock(&lock);
}
// remove the socket number from clients and all groups list
// so that no further request or use of socket of this clien
void remove_socket(int s)
{
    pthread_mutex_lock(&lock);
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].sock == s)
        {
            clients[i] = clients[client_count - 1];
            client_count--;
            break;
        }
    }
    for (int i = 0; i < group_count; i++)
    {
        for (int j = 0; j < groups[i].m; j++)
        {
            if (groups[i].sock[j] == s)
            {
                groups[i].sock[j] = groups[i].sock[groups[i].m - 1];
                groups[i].m--;
                break;
            }
        }
    }
    pthread_mutex_unlock(&lock);
}
// PROCESS function which will parse and assign to different function based on request by client
int process_message(int sock, char *msg)
{
    char cmd[256], rest[256];
    words(msg, cmd, rest);
    char *sender = get_name(sock);
    if (sender == NULL)
    {
        return 0;
    }
    if (strcmp(cmd, "broadcast") == 0)
    {
        char final[512];
        sprintf(final, "[%s] %s: %s\n", timestamp(), sender, rest);
        broadcast(final, sock);
    }
    else if (strcmp(cmd, "dm") == 0)
    {
        char u[256], m[256];
        words(rest, u, m);
        send_private(sock, sender, u, m);
    }
    else if (strcmp(cmd, "dmgroup") == 0)
    {
        char u[256], m[256];
        words(rest, u, m);
        send_groupmessage(sock, sender, u, m);
    }
    else if (strcmp(cmd, "creategroup") == 0)
        create_group(rest, sock);
    else if (strcmp(cmd, "joingroup") == 0)
        join_group(rest, sock);
    else if (strcmp(cmd, "leavegroup") == 0)
        leave_group(rest, sock);
    else if (strcmp(cmd, "deletegroup") == 0)
        delete_group(rest, sock);
    else if (strcmp(cmd, "renameuser") == 0)
        rename_user(sock, rest);
    else if (strcmp(cmd, "renamegroup") == 0)
    {
        char o[256], n[256];
        words(rest, o, n);
        rename_group(o, n, sock);
    }
    else if (strcmp(cmd, "list") == 0)
    {
        char msg2[512] = "Users:\n";
        for (int i = 0; i < client_count; i++)
        {
            strcat(msg2, clients[i].name);
            strcat(msg2, "\n");
        }
        write(sock, msg2, strlen(msg2));
    }
    else if (strcmp(cmd, "listgroup") == 0)
    {
        char msg2[512] = "Groups:\n";
        for (int i = 0; i < group_count; i++)
        {
            strcat(msg2, groups[i].grp_name);
            strcat(msg2, "\n");
        }
        write(sock, msg2, strlen(msg2));
    }
    else if (strcmp(cmd, "exit") == 0)
        return 0;
    else
    {
        char msg3[256];
        sprintf(msg3, "[%s] Invalid request-->%s\n", timestamp(), rest);
        write(sock, msg3, strlen(msg3));
    }
    return 1;
}

// THREAD
void *client_handler(void *arg)
{
    int sock = *(int *)arg;
    free(arg);
    char buffer[256], username[256];
    // Accpect unique name of each client
    while (1)
    {
        bzero(buffer, 256);
        int n = read(sock, buffer, 255);
        if (n <= 0)
            return NULL;
        buffer[strcspn(buffer, "\n")] = 0;
        char cmd[256];
        words(buffer, cmd, username);
        if (strcmp(cmd, "join") != 0)
        {
            write(sock, "NO", 2);
            continue;
        }
        if (is_name_taken(username))
        {
            write(sock, "NO", 2);
        }
        else
        {
            if (client_count == 100)
            {
                char msg3[256];
                sprintf(msg3, "[%s] The server had reached it's max limits on clients!!!!\n", timestamp());
                write(sock, msg3, strlen(msg3));
                return NULL;
            }
            pthread_mutex_lock(&lock);
            strcpy(clients[client_count].name, username);
            clients[client_count].sock = sock;
            client_count++;
            pthread_mutex_unlock(&lock);
            write(sock, "OK", 2);
            break;
        }
    }

    char msg[256];
    sprintf(msg, "[%s] %s joined\n", timestamp(), username);
    broadcast(msg, sock);
    // accepts multiple requests of each client
    while (1)
    {
        bzero(buffer, 256);
        int n = read(sock, buffer, 255);
        if (n <= 0)
            break;
        buffer[strcspn(buffer, "\n")] = 0;
        if (!process_message(sock, buffer))
            break;
    }
     // after disconnection of client it sends a broadcast and also will be removed fromclient and all groups list
    char msg1[256];
    sprintf(msg1, "[%s] %s had disconnected\n", timestamp(), username);
    broadcast(msg1, sock);
    remove_socket(sock);
    close(sock);
    return NULL;
}

// MAIN function
int main(int argc, char *argv[])
{
    pthread_mutex_init(&lock, NULL);
    if (argc < 2)
    {
        printf("Port missing\n");
        exit(1);
    }
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("Socket error");
    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(atoi(argv[1]));
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)))
        error("Bind error");
    listen(sockfd, 5);
    printf("[%s] Server is Listening....!\n", timestamp());
    // accepts different clients and assign to newthreads
    while (1)
    {
        int newsockfd;
        struct sockaddr_in cli_addr;
        socklen_t clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0)
            continue;
        int *p = malloc(sizeof(int));
        *p = newsockfd;

        pthread_t t;
        pthread_create(&t, NULL, client_handler, p);
        pthread_detach(t);
    }
}