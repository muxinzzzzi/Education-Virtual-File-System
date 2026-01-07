#include "client/review_client.h"
#include <iostream>

int main(int argc, char *argv[]) {
  std::string host = "127.0.0.1";
  int port = 8080;

  if (argc > 1) {
    host = argv[1];
  }
  if (argc > 2) {
    port = std::stoi(argv[2]);
  }

  std::cout << "=== Peer Review System Client ===\n\n";

  client::ReviewClient client;

  if (!client.connect(host, port)) {
    return 1;
  }

  while (true) {
    std::cout << "\nUsername: ";
    std::string username;
    std::getline(std::cin, username);

    std::cout << "Password: ";
    std::string password;
    std::getline(std::cin, password);

    if (!client.login(username, password)) {
      std::cerr << "Login failed\n";
      continue;
    }

    client.run();

    std::cout << "\nLogged out. 是否重新登录？(y/n): ";
    std::string again;
    std::getline(std::cin, again);
    if (again != "y" && again != "Y") {
      break;
    }
  }

  return 0;
}
