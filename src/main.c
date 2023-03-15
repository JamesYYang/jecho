#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/wait.h>
#include <threads.h>
#include <dirent.h>
#include <sys/sysinfo.h>
#include <sys/epoll.h>
#include <fcntl.h>

#define MAX_LISTEN_CONN 128
#define PORT 8080
#define HTTP_REQ_BUF 1024
#define HTTP_RES_BUF 1024
#define MAX_EVENT_NUM 1024
#define INFTIM -1

#define SERVER_STRING "Server: jecho/0.0.1\r\n"

void sigHandler(int signal);
void handle_subprocess_exit(int signal);
void read_til_crnl(FILE *fp);
void error_die(const char *sc);
void unimplemented(int client);
int not_exist(char *f);
int isadir(char *f);
void do_ls(char *dir, int fd);
void not_found(char *item, int fd);
void header(FILE *fp, char *content_type);
char *file_type(char *f);
void serve_file(int client, char *filename);
int process_request(void *fdptr);
void startServer();
void process();

int serverFd;

void sigHandler(int signal)
{
    printf("server closed: \n");
    close(serverFd);
    exit(EXIT_SUCCESS);
}

void handle_subprocess_exit(int signal)
{
    printf("clean subprocess.\n");
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0)
        ;
}

void read_til_crnl(FILE *fp)
{
    char buf[BUFSIZ];
    while (fgets(buf, BUFSIZ, fp) != NULL && strcmp(buf, "\r\n") != 0)
    {
        // printf("%s", buf);
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
    fprintf(fp, SERVER_STRING);
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
    header(fp, "text/html");
    fprintf(fp, "\r\n");
    fflush(fp);

    DIR *dir_ptr;           /* the directory */
    struct dirent *direntp; /* each entry	 */

    if ((dir_ptr = opendir(dir)) == NULL)
        fprintf(fp, "cannot open %s\n", dir);
    else
    {

        while ((direntp = readdir(dir_ptr)) != NULL)
        {
            if (strcmp(direntp->d_name, ".") == 0 || strcmp(direntp->d_name, "..") == 0)
                continue;

            if (dir[strlen(dir) - 1] == '/')
            {
                fprintf(fp, "<a href='%s%s'>%s</a><br />", dir + 1, direntp->d_name, direntp->d_name);
            }
            else
            {
                fprintf(fp, "<a href='%s/%s'>%s</a><br />", dir + 1, direntp->d_name, direntp->d_name);
            }
        }

        closedir(dir_ptr);
    }

    fclose(fp);
}

void not_found(char *item, int fd)
{
    FILE *fp = fdopen(fd, "w");

    fprintf(fp, "HTTP/1.0 404 Not Found\r\n");
    fprintf(fp, "Content-type: text/plain\r\n");
    fprintf(fp, SERVER_STRING);
    fprintf(fp, "\r\n");

    fprintf(fp, "The item you requested: %s\r\nis not found\r\n", item);
    fclose(fp);
}

void header(FILE *fp, char *content_type)
{
    fprintf(fp, "HTTP/1.0 200 OK\r\n");
    if (content_type)
        fprintf(fp, "Content-type: %s\r\n", content_type);
    fprintf(fp, SERVER_STRING);
    time_t thetime;
    thetime = time(NULL);
    fprintf(fp, "Date: %s", ctime(&thetime));
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

int process_request(void *fdptr)
{
    int fd;
    fd = *(int *)fdptr;
    free(fdptr);

    FILE *fpin;
    char request[BUFSIZ];
    fpin = fdopen(fd, "r");

    /* read request */
    fgets(request, BUFSIZ, fpin);
    read_til_crnl(fpin);

    char cmd[BUFSIZ], arg[BUFSIZ];

    strcpy(arg, "."); /* precede args with ./ */
    if (sscanf(request, "%s%s", cmd, arg + 1) != 2)
        return thrd_success;

    printf("[%d]cmd: %s, url: %s\n\n", getpid(), cmd, arg);

    if (strcmp(cmd, "GET") != 0)
        unimplemented(fd);
    else if (not_exist(arg))
        not_found(arg, fd);
    else if (isadir(arg))
        do_ls(arg, fd);
    else
        serve_file(fd, arg);

    fclose(fpin);
    return thrd_success;
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
    signal(SIGCHLD, handle_subprocess_exit);
    startServer();
    printf("\nServer is now listening at port %d:\n\n", PORT);

    int cpu_core_num = get_nprocs();
    printf("cpu core num: %d\n", cpu_core_num);
    // for (int i = 0; i < cpu_core_num * 2; i++)
    for (int i = 0; i < cpu_core_num; i++)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            process();
        }
    }

    while (1)
    {
        sleep(1);
    }

    return EXIT_SUCCESS;
}

void process()
{
    char buf[128];
    struct sockaddr_in address;
    int addrLen = sizeof(address);
    int acceptedSocket;
    int ready_fd_num;

    thrd_t thread;
    int *fdptr;

    int epoll_fd = epoll_create(MAX_EVENT_NUM);
    struct epoll_event ev;
    struct epoll_event events[MAX_EVENT_NUM];
    ev.data.fd = serverFd;
    ev.events = EPOLLIN;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, serverFd, &ev) == -1)
    {
        error_die("epoll_ctl error, message: ");
    }

    while (1)
    {
        ready_fd_num = epoll_wait(epoll_fd, events, MAX_EVENT_NUM, INFTIM);
        printf("[%d] Wake up, ready fd: %d \n", getpid(), ready_fd_num);
        if (ready_fd_num == -1)
        {
            perror("epoll_wait error, message: ");
            continue;
        }

        for (int i = 0; i < ready_fd_num; i++)
        {
            if (events[i].data.fd == serverFd)
            {
                if ((acceptedSocket = accept(serverFd, (struct sockaddr *)&address, (socklen_t *)&addrLen)) < 0)
                {
                    sprintf(buf, "[%d] In accept\n", getpid());
                    perror(buf);
                    continue;
                }

                if (fcntl(acceptedSocket, F_SETFL, fcntl(acceptedSocket, F_GETFD, 0) | O_NONBLOCK) == -1)
                {
                    printf("set nonblock faild \n");
                    continue;
                }

                ev.data.fd = acceptedSocket;
                ev.events = EPOLLIN;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, acceptedSocket, &ev) == -1)
                {
                    perror("epoll_ctl error, message: ");
                    close(acceptedSocket);
                }
            }
            else if (events[i].events & EPOLLIN)
            {
                printf("[%d] process request \n", getpid());
                acceptedSocket = events[i].data.fd;
                fdptr = malloc(sizeof(int));
                *fdptr = acceptedSocket;

                thrd_create(&thread, process_request, fdptr);
                thrd_detach(thread);
                // process_request(acceptedSocket);
                // close(acceptedSocket);
            }
            else if (events[i].events & EPOLLERR)
            {
                perror("epoll error\n");
                close(acceptedSocket);
            }
        }
    }
}