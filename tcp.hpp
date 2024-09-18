
#include "rx.hpp"
#include <arpa/inet.h>
#include <string>
#include <unistd.h>

static auto tcp_server(const std::string &bind_to, uint16_t port) {
  return rx::make_shared_observable<std::string>(
      [bind_to, port](rx::observer<std::string> on_next) {
        int sockfd, newsockfd;
        unsigned int clilen;
        char buffer[256];
        struct sockaddr_in serv_addr, cli_addr;

        // Create socket
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
          perror("ERROR opening socket");
          exit(1);
        }

        // Set  server address
        bzero((char *)&serv_addr, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(port);

        // Bind socket to port
        if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) <
            0) {
          perror("ERROR on binding");
        }
        // Listen for connections
        listen(sockfd, 5);
        clilen = sizeof(cli_addr);
        while (true) {
          newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr,
                             (socklen_t *)&clilen);
          if (newsockfd < 0) {
            perror("ERROR on accept");
          }
          // Process client request
          bzero(buffer, 256);
          int n = read(newsockfd, buffer, 255);
          if (n < 0) {
            perror("ERROR reading from socket");
            exit(1);
          }
          on_next(std::string(buffer, 255));
          close(newsockfd);
        }
        close(sockfd);
      });
}

static auto tcp_client(const std::string &host, uint16_t port) {
  // return rx::make_shared_observable<std::string>(
  return rx::observable<std::string>::template make_shared_observable<
      std::string>([host, port](rx::observer<std::string> on_next) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);

    inet_pton(AF_INET, host.c_str(), &saddr.sin_addr);
    int res = connect(fd, (struct sockaddr *)&saddr, sizeof(saddr));

    // std::thread reader([fd, on_next]() {
    thread_local char buffer[1024];
    while (true) {
      int rd = read(fd, buffer, sizeof(buffer));
      if (rd > 0) {
        on_next(std::string(buffer, rd));
      }
    }
    //});
    // reader.detach();
  });
}