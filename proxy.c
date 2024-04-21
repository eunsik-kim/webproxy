#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";
int request_transfer(int clientfd);
int parse_header(rio_t *rio, int clientfd);
void response_transfer(int clientfd, int serverfd);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv) {
    
    int listenfd, clientfd, serverfd;
    struct sockaddr_storage clientaddr;
    char clienthost[MAXLINE], clientport[MAXLINE];
    socklen_t clientlen;
    
    /* checking proxy port args */
    if (argc != 2){
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(clientaddr);
        clientfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, clienthost, MAXLINE, clientport, MAXLINE, 0);
        printf("Accepted connection from client (%s, %s)\n", clienthost, clientport);
        if ((serverfd = request_transfer(clientfd)) != -1)
            response_transfer(clientfd, serverfd);
        Close(clientfd);
    }
    return 0;
}

int request_transfer(int clientfd)
{   
    char buf[MAXLINE];
    int serverfd, isUserAgent = 0, isCon = 0, isProxCon = 0;

    rio_t rio;
    Rio_readinitb(&rio, clientfd);
    if ((serverfd = parse_header(&rio, clientfd)) == -1)
        return -1;

    while(strcmp(buf, "\r\n")) {
        Rio_readlineb(&rio, buf, MAXLINE);
        // check existance of important request headers
        if (strstr(buf, "User-Agent"))
            isUserAgent = 1;
        if (strstr(buf, "Proxy-Connection"))
            isProxCon = 1;
        else if (strstr(buf, "Connection"))
            isCon = 1;

        if (strcmp(buf, "\r\n"))
        {
            printf("%s", buf);
            Rio_writen(serverfd, buf, strlen(buf));
        }
    }

    if (!isUserAgent)
        Rio_writen(serverfd, (void *)user_agent_hdr, strlen(user_agent_hdr));

    char con[20], proxcon[26];
    if (!isCon){
        strcpy(con, "Connection: close\r\n");
        printf("%s", con);
        Rio_writen(serverfd, con, strlen(con));
    }
    if (!isProxCon){
        strcpy(proxcon, "Proxy-Connection: close\r\n");
        printf("%s", proxcon);
        Rio_writen(serverfd, proxcon, strlen(proxcon));
    }
    Rio_writen(serverfd, buf, strlen(buf));
    return serverfd;
}

int parse_header(rio_t *rio, int clientfd)
{   
    int serverfd;
    char buf[MAXLINE], serverhost[MAXLINE], serverport[MAXLINE];
    char method[10], uri[MAXLINE], version[10], new_uri[MAXLINE];

    Rio_readlineb(rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);
    // only GET request
    if (strcasecmp(method, "GET")){ 
        clienterror(clientfd, "method", "501", "Not implemented", "Tiny does not implement this method");
        return -1;
    }

    // parsing header request
    char *hostptr, *portptr, *rootptr;
    hostptr = index(uri, ':') + 3;
    rootptr = index(hostptr, '/');
    *rootptr = '\0';
    portptr = index(hostptr, ':');
    
    if (portptr){
        strcpy(serverport, portptr + 1);
        *portptr = '\0';
    }
    else    // html default port
        strcpy(serverport, "80");
    strcpy(serverhost, hostptr);

    // concatenate header request
    strcpy(new_uri, method);
    strcat(new_uri, " /");
    if (!strcmp(rootptr+1, "\0"))
        strcat(new_uri, rootptr);
    strcat(new_uri, " ");
    strcat(new_uri, "HTML/1.0\r\n");   // modify version HTML/1.1 into HTML/1.0

    serverfd = Open_clientfd((char *)serverhost, (char *)serverport);
    Rio_writen(serverfd, new_uri, strlen(new_uri));
    return serverfd;
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];
    sprintf(body, "\r\n<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);
    
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

void response_transfer(int clientfd, int serverfd)
{   
    rio_t rio;
    char buf[MAXLINE];
    Rio_readinitb(&rio, serverfd);
    do {  
        Rio_readlineb(&rio, buf, MAXLINE);
        Rio_writen(clientfd, buf, strlen(buf));
    } while (strcmp(buf, "\r\n"));
    
    do {  
        Rio_readlineb(&rio, buf, MAXLINE);
        Rio_writen(clientfd, buf, strlen(buf));
    } while (strcmp(buf, "\r\n"));
    Close(serverfd);
}