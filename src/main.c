#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>

#define MAX_LISTEN_CONN 128
#define PORT 8080
#define HTTP_REQ_BUF 1024
#define HTTP_RES_BUF 1024

#define SERVER_STRING "Server: jecho/0.0.1\r\n"

void sigHandler(int signal);
void read_til_crnl(FILE *fp);
void error_die(const char *sc);
void unimplemented(int client);
int not_exist(char *f);
int isadir(char *f);
void do_ls(char *dir, int fd);
void not_found(char *item, int fd);
void header(FILE *fp, char *content_type);
void cat(int client, FILE *resource);
char *file_type(char *f);
void serve_file(int client, char *filename);
void process_request(char *request, int acceptedSocket);
void startServer();

int serverFd;

void sigHandler(int signal)
{
    printf("server closed: \n");
    close(serverFd);
    exit(EXIT_SUCCESS);
}

void read_til_crnl(FILE *fp)
{
    char buf[BUFSIZ];
    while (fgets(buf, BUFSIZ, fp) != NULL && strcmp(buf, "\r\n") != 0)
    {
        printf("%s", buf);
    }
}

void error_die(const char *sc)
{
    perror(sc);
    exit(1);
}

void unimplemented(int fd)
{
    FILE *fp = fdopen(fd, "w");

    fprintf(fp, "HTTP/1.0 501 Not Implemented\r\n");
    fprintf(fp, "Content-type: text/plain\r\n");
    fprintf(fp, "\r\n");

    fprintf(fp, "That command is not yet implemented\r\n");
    fclose(fp);
}

int not_exist(char *f)
{
    struct stat info;
    return (stat(f, &info) == -1);
}

int isadir(char *f)
{
    struct stat info;
    return (stat(f, &info) != -1 && S_ISDIR(info.st_mode));
}

void do_ls(char *dir, int fd)
{
    FILE *fp;

    fp = fdopen(fd, "w");
    header(fp, "text/plain");
    fprintf(fp, "\r\n");
    fflush(fp);

    dup2(fd, 1);
    dup2(fd, 2);
    close(fd);
    execlp("ls", "ls", "-l", dir, NULL);

    // error_die(dir);
}

void not_found(char *item, int fd)
{
    FILE *fp = fdopen(fd, "w");

    fprintf(fp, "HTTP/1.0 404 Not Found\r\n");
    fprintf(fp, "Content-type: text/plain\r\n");
    fprintf(fp, "\r\n");

    fprintf(fp, "The item you requested: %s\r\nis not found\r\n", item);
    fclose(fp);
}

void header(FILE *fp, char *content_type)
{
    fprintf(fp, "HTTP/1.0 200 OK\r\n");
    if (content_type)
        fprintf(fp, "Content-type: %s\r\n", content_type);
    fprintf(fp, "Server: jecho/0.0.1\r\n");
}

void cat(int client, FILE *resource)
{
    char buf[1024];

    fgets(buf, sizeof(buf), resource);
    while (!feof(resource))
    {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

char *file_type(char *f)
/* returns 'extension' of file */
{
    char *cp;
    if ((cp = strrchr(f, '.')) != NULL)
        return cp + 1;
    return "";
}

void serve_file(int client, char *filename)
{
    char *extension = file_type(filename);
    char *content = "text/plain";
    FILE *fpsock, *fpfile;
    int c;

    if (strcmp(extension, "html") == 0)
        content = "text/html";
    else if (strcmp(extension, "gif") == 0)
        content = "image/gif";
    else if (strcmp(extension, "jpg") == 0)
        content = "image/jpeg";
    else if (strcmp(extension, "jpeg") == 0)
        content = "image/jpeg";

    fpsock = fdopen(client, "w");
    fpfile = fopen(filename, "r");
    if (fpsock != NULL && fpfile != NULL)
    {
        header(fpsock, content);
        fprintf(fpsock, "\r\n");
        while ((c = getc(fpfile)) != EOF)
            putc(c, fpsock);
        fclose(fpfile);
        fclose(fpsock);
    }
    // exit(0);
}

void process_request(char *request, int acceptedSocket)
{
    char cmd[BUFSIZ], arg[BUFSIZ];

    strcpy(arg, "./"); /* precede args with ./ */
    if (sscanf(request, "%s%s", cmd, arg + 2) != 2)
        return;

    printf("cmd: %s, url: %s\n", cmd, arg);

    if (strcmp(cmd, "GET") != 0)
        unimplemented(acceptedSocket);
    else if (not_exist(arg))
        not_found(arg, acceptedSocket);
    else if (isadir(arg))
        do_ls(arg, acceptedSocket);
    else
        serve_file(acceptedSocket, arg);
}

void startServer()
{
    // establish a socket.
    if ((serverFd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        error_die("In socket creation");
    }

    struct sockaddr_in address;
    int addrLen = sizeof(address);

    bzero(&address, addrLen);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // -> 0.0.0.0.
    address.sin_port = htons(PORT);

    // assigns specified address to the socket.
    if (bind(serverFd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        error_die("In bind");
    }

    // mark the socket as a passive socket.
    if (listen(serverFd, MAX_LISTEN_CONN) < 0)
    {
        error_die("In listen");
    }
}

int main(int argc, const char *argv[])
{
    signal(SIGINT, sigHandler);
    startServer();
    printf("\nServer is now listening at port %d:\n\n", PORT);

    struct sockaddr_in address;
    int addrLen = sizeof(address);
    int acceptedSocket;
    while (1)
    {
        if ((acceptedSocket = accept(serverFd, (struct sockaddr *)&address, (socklen_t *)&addrLen)) < 0)
        {
            error_die("In accept");
        }

        FILE *fpin;
        char request[BUFSIZ];
        fpin = fdopen(acceptedSocket, "r");

        /* read request */
        fgets(request, BUFSIZ, fpin);
        printf("got a call: request = %s", request);
        read_til_crnl(fpin);

        process_request(request, acceptedSocket);
        fclose(fpin);
    }

    return EXIT_SUCCESS;
}