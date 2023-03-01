#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <uriparser/Uri.h>

#define MAX_LISTEN_CONN 128
#define PORT 8080
#define HTTP_REQ_BUF 1024
#define HTTP_RES_BUF 1024

int serverFd; 

void sigHandler(int signal) {  
  printf("server closed: \n");
  close(serverFd);
}

void wrapStrFromPTR(char* str, size_t len, const char* head, const char* tail) {
  for (size_t i = 0; head != tail; head++)
    str[i++] = *head;
  str[len - 1] = '\0';
}

void tryParseUrl(char* req) {
  // extract uri;
  const char* uriHead = strchr(req, ' ') + 1;
  const char* uriTail = strchr(uriHead, ' ');
  size_t uriLen = uriTail - uriHead + 1;
  char strUri[uriLen];
  wrapStrFromPTR(strUri, uriLen, uriHead, uriTail);

  printf("raw url: %s\n", strUri);

  // parse uri;
  UriUriA uri;
  UriQueryListA* queryList;
  const char* errorPos;
  if (uriParseSingleUriA(&uri, strUri, &errorPos) == URI_SUCCESS) {
    printf("parse url success: %s\n", strUri);
  }
}

void error_die(const char *sc)
{
  perror(sc); 
  exit(1);
}

void startServer() {
    // establish a socket.
  if ((serverFd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    error_die("In socket creation");
  }

  struct sockaddr_in address;
  int addrLen = sizeof(address);

  bzero(&address, addrLen); 
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;  // -> 0.0.0.0.
  address.sin_port = htons(PORT);

  // assigns specified address to the socket.
  if (bind(serverFd, (struct sockaddr*) &address, sizeof(address)) < 0) {
    error_die("In bind");
  }

  // mark the socket as a passive socket.
  if (listen(serverFd, MAX_LISTEN_CONN) < 0) {
    error_die("In listen");
  }
  
}

int main(int argc, const char* argv[]) {

  signal(SIGINT, sigHandler);

  startServer();

  printf("\nServer is now listening at port %d:\n\n", PORT);

  struct sockaddr_in address;
  int addrLen = sizeof(address);

  int acceptedSocket;

  while (1) {

    if ((acceptedSocket = accept(serverFd, (struct sockaddr*) &address, (socklen_t*) &addrLen)) < 0) {
      error_die("In accept");
    }

    char reqBuf[HTTP_REQ_BUF];
    bzero(reqBuf, HTTP_REQ_BUF); 

    const size_t receivedBytes = read(acceptedSocket, reqBuf, HTTP_REQ_BUF);

    if (receivedBytes > 0) {
      printf("you send msg: \n%s", reqBuf);
      tryParseUrl(reqBuf);
    }

    char resBuf[HTTP_RES_BUF];

    time_t currTime = time(NULL);
    struct tm* tm = localtime(&currTime);

    sprintf(resBuf, "HTTP/1.1 200 OK\r\n"
                    "Content-type: text/plain\r\n\r\n"
                    "Hello, you are connected, this is welcome msg from Jecho\r\n"
                    "current time: %s", asctime(tm));

    write(acceptedSocket, resBuf, strlen(resBuf));

    close(acceptedSocket);

  }

  return EXIT_SUCCESS;

}