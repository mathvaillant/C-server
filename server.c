#include <stdio.h>  // console input/output, perror
#include <stdlib.h> // exit
#include <string.h> // string manipulation
#include <netdb.h>  // getnameinfo

#include <sys/socket.h> // socket APIs
#include <netinet/in.h> // sockaddr_in
#include <unistd.h>     // open, close

#include <signal.h> // signal handling
#include <time.h>   // time

#define SIZE 1024  // Buffer size
#define PORT 2728  // port number
#define BACKLOG 20 // number of pending connections

/**
 * @brief Generates file URL based on route
 * @param route requested route
 * @param fileurl generated url
 */
void getFileURL(char *route, char *fileURL);

/**
 * @brief Sets *MIME to the mime type of file
 * @param file file URL
 * @param mime mime type of file
 */
void getMimeType(char *file, char *mime);

/**
 * @brief Handles SIGINT signal
 * SIGNIG = Signal Interrupt
 */
void handleSignal(int signal);

/**
 * @brief Returns a string with the current time in HTTP response date format
 * @param buffer buffer to store the time string
 * https://stackoverflow.com/questions/7548759/generate-a-date-string-in-http-response-date-format-in-c
 */
void getTimeString(char *buffer);

int serverSocket;
int clientSocket;

char *request;

int main()
{
  // Signal handler
  signal(SIGINT, handleSignal);

  // Internet socket address
  struct sockaddr_in serverAddress;
  serverAddress.sin_family = AF_INET;                     // IPv4
  serverAddress.sin_port = htons(PORT);                   // port number in network byte order (host-to-network short)
  serverAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // localhost (host to network long)

  // Socket of type IPv4 using TCP protocol
  serverSocket = socket(AF_INET, SOCK_STREAM, 0);

  // Reuse address and port
  setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

  // Bind socket to address
  if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
  {
    printf("Error: The server is not bound to the address.\n");
    return 1;
  }

  // Listen for connections
  if (listen(serverSocket, BACKLOG) < 0)
  {
    printf("Error: The server is not listening.\n");
    return 1;
  }

  // Get server address information
  char hostBuffer[NI_MAXHOST];
  char serviceBuffer[NI_MAXSERV];

  int error = getnameinfo((struct sockaddr *)&serverAddress, sizeof(serverAddress), hostBuffer,
                          sizeof(hostBuffer), serviceBuffer, sizeof(serviceBuffer), 0);

  if (error != 0)
  {
    printf("Error: %s\n", gai_strerror(error));
    return 1;
  }

  // Directly uses the sin_port field from the serverAddress structure,
  // converts it from network byte order to host byte order using ntohs,
  // and prints it in the expected format.
  printf("\nServer is listening on http://%s:%d/\n\n", hostBuffer, ntohs(serverAddress.sin_port));

  while (1)
  {
    // Buffer to store data (request)
    request = (char *)malloc(SIZE * sizeof(char));
    char method[10];
    char route[100];

    // Accept connection and read data
    clientSocket = accept(serverSocket, NULL, NULL);
    read(clientSocket, request, SIZE);

    // Parse HTTP request
    sscanf(request, "%s %s", method, route);
    printf("%s %s", method, route);

    // Free up space in memory
    free(request);

    // Only support GET method
    if (strcmp(method, "GET") != 0)
    {
      const char response[] = "HTTP/1.1 400 Bad Request\r\n\n";
      send(clientSocket, response, sizeof(response), 0);
    }
    else
    {
      char fileURL[100];

      // generate file URL
      getFileURL(route, fileURL);

      // read file
      FILE *file = fopen(fileURL, "r");
      if (!file)
      {
        const char response[] = "HTTP/1.1 404 Not Found\r\n\n";
        send(clientSocket, response, sizeof(response), 0);
      }
      else
      {
        // generate HTTP response header
        char resHeader[SIZE];

        // get current time
        char timeBuf[100];
        getTimeString(timeBuf);

        // generate mime type from file URL
        char mimeType[32];
        getMimeType(fileURL, mimeType);

        sprintf(resHeader, "HTTP/1.1 200 OK\r\nDate: %s\r\nContent-Type: %s\r\n\n", timeBuf, mimeType);
        int headerSize = strlen(resHeader);

        printf(" %s", mimeType);

        // Calculate file size
        fseek(file, 0, SEEK_END);
        long fsize = ftell(file);
        fseek(file, 0, SEEK_SET);

        // Allocates memory for response buffer and copies response header and file contents to it
        char *resBuffer = (char *)malloc(fsize + headerSize);
        strcpy(resBuffer, resHeader);

        // Starting position of file contents in response buffer
        char *fileBuffer = resBuffer + headerSize;
        fread(fileBuffer, fsize, 1, file);

        send(clientSocket, resBuffer, fsize + headerSize, 0);
        free(resBuffer);
        fclose(file);
      }
    }
    close(clientSocket);
    printf("\n");
  }
}

void getFileURL(char *route, char *fileURL)
{
  // if route has parameters, remove them
  char *question = strrchr(route, '?');
  if (question)
  {
    *question = '\0';
  }

  // if route is empty, set it to index.html
  if (route[strlen(route) - 1] == '/')
  {
    strcat(route, "index.html");
  }

  // get filename from route
  strcpy(fileURL, "htdocs");
  strcat(fileURL, route);

  // if filename does not have an extension, set it to .html
  const char *dot = strrchr(fileURL, '.');
  if (!dot || dot == fileURL)
  {
    strcat(fileURL, ".html");
  }
}

void getMimeType(char *file, char *mime)
{
  // position in string with period character
  const char *dot = strrchr(file, '.');

  // if period not found, set mime type to text/html
  if (dot == NULL)
    strcpy(mime, "text/html");

  else if (strcmp(dot, ".html") == 0)
    strcpy(mime, "text/html");

  else if (strcmp(dot, ".css") == 0)
    strcpy(mime, "text/css");

  else if (strcmp(dot, ".js") == 0)
    strcpy(mime, "application/js");

  else if (strcmp(dot, ".jpg") == 0)
    strcpy(mime, "image/jpeg");

  else if (strcmp(dot, ".png") == 0)
    strcpy(mime, "image/png");

  else if (strcmp(dot, ".gif") == 0)
    strcpy(mime, "image/gif");

  else
    strcpy(mime, "text/html");
}

void handleSignal(int signal)
{
  if (signal == SIGINT)
  {
    printf("\nShutting down server...\n");

    close(clientSocket);
    close(serverSocket);

    if (request != NULL)
    {
      free(request);
    }

    exit(0);
  }
}

void getTimeString(char *buf)
{
  time_t now = time(0);
  struct tm tm = *gmtime(&now);
  strftime(buf, sizeof buf, "%a, %d %b %Y %H:%M:%S %Z", &tm);
}
