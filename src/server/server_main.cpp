#include "server/review_server.h"
#include <csignal>
#include <iostream>

std::unique_ptr<server::ReviewServer> g_server;

void signal_handler(int signal) {
  if (g_server) {
    std::cout << "\nShutting down server...\n";
    g_server->stop();
  }
}

int main(int argc, char *argv[]) {
  int port = 8080;
  std::string fs_image = "review_system.img";

  if (argc > 1) {
    port = std::stoi(argv[1]);
  }
  if (argc > 2) {
    fs_image = argv[2];
  }

  std::cout << "=== Peer Review System Server ===\n";
  std::cout << "Port: " << port << "\n";
  std::cout << "Filesystem: " << fs_image << "\n\n";

  g_server = std::make_unique<server::ReviewServer>(port, fs_image);

  // Setup signal handlers
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  if (!g_server->start()) {
    std::cerr << "Failed to start server\n";
    return 1;
  }

  std::cout << "\nPress Ctrl+C to stop the server\n";

  // Keep main thread alive
  while (g_server->is_running()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  return 0;
}
