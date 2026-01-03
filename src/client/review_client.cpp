#include "client/review_client.h"
#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

namespace client {

ReviewClient::ReviewClient()
    : socket_(-1), connected_(false), current_role_(protocol::Role::UNKNOWN) {}

ReviewClient::~ReviewClient() { disconnect(); }

bool ReviewClient::connect(const std::string &host, int port) {
  socket_ = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_ < 0) {
    std::cerr << "Failed to create socket\n";
    return false;
  }

  struct sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
    std::cerr << "Invalid address\n";
    close(socket_);
    return false;
  }

  if (::connect(socket_, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    std::cerr << "Connection failed\n";
    close(socket_);
    return false;
  }

  connected_ = true;
  std::cout << "Connected to " << host << ":" << port << "\n";

  return true;
}

void ReviewClient::disconnect() {
  if (connected_) {
    if (!session_id_.empty()) {
      logout();
    }
    close(socket_);
    connected_ = false;
  }
}

bool ReviewClient::login(const std::string &username,
                         const std::string &password) {
  protocol::Message msg;
  msg.command = protocol::Command::LOGIN;
  msg.params["username"] = username;
  msg.params["password"] = password;

  if (!send_message(msg)) {
    return false;
  }

  protocol::Response resp;
  if (!receive_response(resp)) {
    return false;
  }

  if (resp.status != protocol::StatusCode::OK) {
    std::cerr << "Login failed: " << resp.message << "\n";
    return false;
  }

  // Parse response body
  std::string body_str(resp.body.begin(), resp.body.end());
  std::istringstream iss(body_str);
  std::string line;

  while (std::getline(iss, line)) {
    size_t eq_pos = line.find('=');
    if (eq_pos != std::string::npos) {
      std::string key = line.substr(0, eq_pos);
      std::string value = line.substr(eq_pos + 1);

      if (key == "session_id") {
        session_id_ = value;
      } else if (key == "role") {
        current_role_ = protocol::Protocol::string_to_role(value);
      }
    }
  }

  username_ = username;
  std::cout << "Login successful as "
            << protocol::Protocol::role_to_string(current_role_) << "\n";

  return true;
}

void ReviewClient::logout() {
  protocol::Message msg;
  msg.command = protocol::Command::LOGOUT;

  send_message(msg);

  protocol::Response resp;
  receive_response(resp);

  session_id_.clear();
  current_role_ = protocol::Role::UNKNOWN;
  username_.clear();
}

void ReviewClient::run() {
  while (connected_ && !session_id_.empty()) {
    switch (current_role_) {
    case protocol::Role::AUTHOR:
      show_author_menu();
      break;
    case protocol::Role::REVIEWER:
      show_reviewer_menu();
      break;
    case protocol::Role::EDITOR:
      show_editor_menu();
      break;
    case protocol::Role::ADMIN:
      show_admin_menu();
      break;
    default:
      std::cout << "Unknown role\n";
      return;
    }
  }
}

void ReviewClient::show_author_menu() {
  std::cout << "\n=== Author Menu ===\n";
  std::cout << "1. Upload Paper\n";
  std::cout << "2. View Paper Status\n";
  std::cout << "3. Submit Revision\n";
  std::cout << "4. Download Reviews\n";
  std::cout << "5. Logout\n";
  std::cout << "Choice: ";

  int choice;
  std::cin >> choice;
  std::cin.ignore();

  switch (choice) {
  case 1:
    upload_paper();
    break;
  case 2:
    view_paper_status();
    break;
  case 3:
    submit_revision();
    break;
  case 4:
    download_reviews();
    break;
  case 5:
    logout();
    break;
  default:
    std::cout << "Invalid choice\n";
  }
}

void ReviewClient::show_reviewer_menu() {
  std::cout << "\n=== Reviewer Menu ===\n";
  std::cout << "1. Download Paper\n";
  std::cout << "2. Submit Review\n";
  std::cout << "3. View Review Status\n";
  std::cout << "4. Set My Profile (fields/keywords)\n";
  std::cout << "5. View My Profile\n";
  std::cout << "6. Logout\n";
  std::cout << "Choice: ";

  int choice;
  std::cin >> choice;
  std::cin.ignore();

  switch (choice) {
  case 1:
    download_paper();
    break;
  case 2:
    submit_review();
    break;
  case 3:
    view_review_status();
    break;
  case 4:
    set_reviewer_profile();
    break;
  case 5:
    get_reviewer_profile();
    break;
  case 6:
    logout();
    break;
  default:
    std::cout << "Invalid choice\n";
  }
}

void ReviewClient::show_editor_menu() {
  std::cout << "\n=== Editor Menu ===\n";
  std::cout << "1. Assign Reviewer (Manual)\n";
  std::cout << "2. Make Decision\n";
  std::cout << "3. View Pending Papers\n";
  std::cout << "4. View Review Progress\n";
  std::cout << "5. Get Reviewer Recommendations (Auto)\n";
  std::cout << "6. Auto Assign Reviewers\n";
  std::cout << "7. Logout\n";
  std::cout << "Choice: ";

  int choice;
  std::cin >> choice;
  std::cin.ignore();

  switch (choice) {
  case 1:
    assign_reviewer();
    break;
  case 2:
    make_decision();
    break;
  case 3:
    view_pending_papers();
    break;
  case 4:
    view_review_progress();
    break;
  case 5:
    get_reviewer_recommendations();
    break;
  case 6:
    auto_assign_reviewers();
    break;
  case 7:
    logout();
    break;
  default:
    std::cout << "Invalid choice\n";
  }
}

void ReviewClient::show_admin_menu() {
  std::cout << "\n=== Admin Menu ===\n";
  std::cout << "1. Create User\n";
  std::cout << "2. View System Status\n";
  std::cout << "3. Create Backup\n";
  std::cout << "4. List Users\n";
  std::cout << "5. List Backups\n";
  std::cout << "6. Restore Backup\n";
  std::cout << "7. Logout\n";
  std::cout << "Choice: ";

  int choice;
  std::cin >> choice;
  std::cin.ignore();

  switch (choice) {
  case 1:
    create_user();
    break;
  case 2:
    view_system_status();
    break;
  case 3:
    create_backup();
    break;
  case 4:
    list_users();
    break;
  case 5:
    list_backups();
    break;
  case 6:
    restore_backup();
    break;
  case 7:
    logout();
    break;
  default:
    std::cout << "Invalid choice\n";
  }
}

void ReviewClient::upload_paper() {
  std::cout << "Paper title: ";
  std::string title = read_line();

  std::cout << "Paper file path: ";
  std::string file_path = read_line();

  auto file_data = read_file(file_path);
  if (file_data.empty()) {
    std::cerr << "Failed to read file\n";
    return;
  }

  protocol::Message msg;
  msg.command = protocol::Command::UPLOAD_PAPER;
  msg.params["title"] = title;
  msg.body = file_data;

  if (!send_message(msg)) {
    std::cerr << "Failed to send message\n";
    return;
  }

  protocol::Response resp;
  if (!receive_response(resp)) {
    std::cerr << "Failed to receive response\n";
    return;
  }

  std::cout << resp.message << "\n";
  if (!resp.body.empty()) {
    std::cout << std::string(resp.body.begin(), resp.body.end()) << "\n";
  }
}

void ReviewClient::view_paper_status() {
  std::cout << "Paper ID: ";
  std::string paper_id = read_line();

  protocol::Message msg;
  msg.command = protocol::Command::VIEW_PAPER_STATUS;
  msg.params["paper_id"] = paper_id;

  send_message(msg);

  protocol::Response resp;
  receive_response(resp);

  std::cout << "\n" << resp.message << "\n";
  if (!resp.body.empty()) {
    std::cout << std::string(resp.body.begin(), resp.body.end()) << "\n";
  }
}

void ReviewClient::download_paper() {
  std::cout << "Paper ID: ";
  std::string paper_id = read_line();

  std::cout << "Save as: ";
  std::string save_path = read_line();

  protocol::Message msg;
  msg.command = protocol::Command::DOWNLOAD_PAPER;
  msg.params["paper_id"] = paper_id;

  send_message(msg);

  protocol::Response resp;
  receive_response(resp);

  if (resp.status == protocol::StatusCode::OK && !resp.body.empty()) {
    save_file(save_path, resp.body);
    std::cout << "Paper saved to " << save_path << "\n";
  } else {
    std::cout << resp.message << "\n";
  }
}

void ReviewClient::submit_review() {
  std::cout << "Paper ID: ";
  std::string paper_id = read_line();

  std::cout << "Review file path: ";
  std::string review_path = read_line();

  auto review_data = read_file(review_path);

  protocol::Message msg;
  msg.command = protocol::Command::SUBMIT_REVIEW;
  msg.params["paper_id"] = paper_id;
  msg.body = review_data;

  send_message(msg);

  protocol::Response resp;
  receive_response(resp);

  std::cout << resp.message << "\n";
}

void ReviewClient::assign_reviewer() {
  std::cout << "Paper ID: ";
  std::string paper_id = read_line();

  std::cout << "Reviewer username: ";
  std::string reviewer = read_line();

  protocol::Message msg;
  msg.command = protocol::Command::ASSIGN_REVIEWER;
  msg.params["paper_id"] = paper_id;
  msg.params["reviewer"] = reviewer;

  send_message(msg);

  protocol::Response resp;
  receive_response(resp);

  std::cout << resp.message << "\n";
}

void ReviewClient::make_decision() {
  std::cout << "Paper ID: ";
  std::string paper_id = read_line();

  std::cout << "Decision (accept/reject/major_revision/minor_revision): ";
  std::string decision = read_line();

  protocol::Message msg;
  msg.command = protocol::Command::MAKE_DECISION;
  msg.params["paper_id"] = paper_id;
  msg.params["decision"] = decision;

  send_message(msg);

  protocol::Response resp;
  receive_response(resp);

  std::cout << resp.message << "\n";
}

void ReviewClient::create_user() {
  std::cout << "Username: ";
  std::string username = read_line();

  std::cout << "Password: ";
  std::string password = read_line();

  std::cout << "Role (author/reviewer/editor/admin): ";
  std::string role = read_line();

  protocol::Message msg;
  msg.command = protocol::Command::CREATE_USER;
  msg.params["username"] = username;
  msg.params["password"] = password;
  msg.params["role"] = role;

  send_message(msg);

  protocol::Response resp;
  receive_response(resp);

  std::cout << resp.message << "\n";
}

void ReviewClient::view_system_status() {
  protocol::Message msg;
  msg.command = protocol::Command::SYSTEM_STATUS;

  send_message(msg);

  protocol::Response resp;
  receive_response(resp);

  std::cout << "\n" << std::string(resp.body.begin(), resp.body.end()) << "\n";
}

void ReviewClient::create_backup() {
  std::cout << "Backup name: ";
  std::string name = read_line();

  protocol::Message msg;
  msg.command = protocol::Command::CREATE_BACKUP;
  msg.params["name"] = name;

  send_message(msg);

  protocol::Response resp;
  receive_response(resp);

  std::cout << resp.message << "\n";
}

void ReviewClient::submit_revision() {
  std::cout << "Paper ID: ";
  std::string paper_id = read_line();

  std::cout << "Revision file path: ";
  std::string file_path = read_line();

  auto file_data = read_file(file_path);
  if (file_data.empty()) {
    std::cerr << "Failed to read file\n";
    return;
  }

  protocol::Message msg;
  msg.command = protocol::Command::SUBMIT_REVISION;
  msg.params["paper_id"] = paper_id;
  msg.body = file_data;

  send_message(msg);
  protocol::Response resp;
  receive_response(resp);
  std::cout << resp.message << "\n";
}

void ReviewClient::download_reviews() {
  std::cout << "Paper ID: ";
  std::string paper_id = read_line();

  protocol::Message msg;
  msg.command = protocol::Command::DOWNLOAD_REVIEWS;
  msg.params["paper_id"] = paper_id;

  send_message(msg);
  protocol::Response resp;
  receive_response(resp);

  std::cout << "\n" << resp.message << "\n";
  if (!resp.body.empty()) {
    std::cout << std::string(resp.body.begin(), resp.body.end()) << "\n";
  }
}

void ReviewClient::view_review_status() {
  // Essentially same as view_paper_status but with different access control
  // potentially Or tailored view. Reusing view_paper_status logic for
  // simplicity as client
  std::cout << "Paper ID: ";
  std::string paper_id = read_line();

  protocol::Message msg;
  msg.command = protocol::Command::VIEW_REVIEW_STATUS;
  msg.params["paper_id"] = paper_id;

  send_message(msg);
  protocol::Response resp;
  receive_response(resp);

  std::cout << "\n" << resp.message << "\n";
  if (!resp.body.empty()) {
    std::cout << std::string(resp.body.begin(), resp.body.end()) << "\n";
  }
}

void ReviewClient::view_pending_papers() {
  protocol::Message msg;
  msg.command = protocol::Command::VIEW_PENDING_PAPERS;
  send_message(msg);
  protocol::Response resp;
  receive_response(resp);
  std::cout << "\n" << std::string(resp.body.begin(), resp.body.end()) << "\n";
}

void ReviewClient::view_review_progress() {
  std::cout << "Paper ID: ";
  std::string paper_id = read_line();

  protocol::Message msg;
  msg.command = protocol::Command::VIEW_REVIEW_PROGRESS;
  msg.params["paper_id"] = paper_id;

  send_message(msg);
  protocol::Response resp;
  receive_response(resp);

  std::cout << "\n" << resp.message << "\n";
  if (!resp.body.empty()) {
    std::cout << std::string(resp.body.begin(), resp.body.end()) << "\n";
  }
}

void ReviewClient::list_users() {
  protocol::Message msg;
  msg.command = protocol::Command::LIST_USERS;
  send_message(msg);
  protocol::Response resp;
  receive_response(resp);
  std::cout << "\n" << std::string(resp.body.begin(), resp.body.end()) << "\n";
}

void ReviewClient::list_backups() {
  protocol::Message msg;
  msg.command = protocol::Command::LIST_BACKUPS;
  send_message(msg);
  protocol::Response resp;
  receive_response(resp);
  std::cout << "\n" << std::string(resp.body.begin(), resp.body.end()) << "\n";
}

void ReviewClient::restore_backup() {
  std::cout << "Backup name: ";
  std::string name = read_line();

  protocol::Message msg;
  msg.command = protocol::Command::RESTORE_BACKUP;
  msg.params["name"] = name;

  send_message(msg);
  protocol::Response resp;
  receive_response(resp);
  std::cout << resp.message << "\n";
}

bool ReviewClient::send_message(const protocol::Message &msg) {
  auto data = protocol::Protocol::serialize_message(msg);
  ssize_t sent = send(socket_, data.data(), data.size(), 0);
  return sent == static_cast<ssize_t>(data.size());
}

bool ReviewClient::receive_response(protocol::Response &resp) {
  // First, receive header (status line + body length line)
  std::vector<char> header_buffer(1024);
  ssize_t header_received = 0;

  // Receive until we have at least two newlines (status + length)
  while (header_received < 1024) {
    ssize_t n = recv(socket_, header_buffer.data() + header_received,
                     1024 - header_received, 0);
    if (n <= 0) {
      return false;
    }
    header_received += n;

    // Check if we have both newlines
    std::string temp(header_buffer.begin(),
                     header_buffer.begin() + header_received);
    size_t first_newline = temp.find('\n');
    if (first_newline != std::string::npos) {
      size_t second_newline = temp.find('\n', first_newline + 1);
      if (second_newline != std::string::npos) {
        // We have the header, extract body length
        std::string length_str =
            temp.substr(first_newline + 1, second_newline - first_newline - 1);
        size_t body_length = std::stoull(length_str);

        // Calculate how much body data we already received
        size_t header_end = second_newline + 1;
        size_t body_received = header_received - header_end;

        // Allocate buffer for complete message
        std::vector<char> complete_buffer(header_end + body_length);
        std::memcpy(complete_buffer.data(), header_buffer.data(),
                    header_received);

        // Receive remaining body data
        while (body_received < body_length) {
          ssize_t n =
              recv(socket_, complete_buffer.data() + header_end + body_received,
                   body_length - body_received, 0);
          if (n <= 0) {
            return false;
          }
          body_received += n;
        }

        return protocol::Protocol::deserialize_response(complete_buffer, resp);
      }
    }
  }

  return false;
}

std::string ReviewClient::read_line() {
  std::string line;
  std::getline(std::cin, line);
  return line;
}

std::vector<char> ReviewClient::read_file(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return {};
  }

  return std::vector<char>((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
}

void ReviewClient::save_file(const std::string &path,
                             const std::vector<char> &data) {
  std::ofstream file(path, std::ios::binary);
  file.write(data.data(), data.size());
}

// ===== Profile & Assignment Methods =====

void ReviewClient::set_reviewer_profile() {
  std::cout << "Enter your research fields (comma-separated, e.g., AI,ML,NLP): ";
  std::string fields = read_line();
  
  std::cout << "Enter your keywords (comma-separated, e.g., deep learning,transformer): ";
  std::string keywords = read_line();
  
  std::cout << "Enter your affiliation (e.g., MIT, Stanford): ";
  std::string affiliation = read_line();
  
  protocol::Message msg;
  msg.command = protocol::Command::SET_REVIEWER_PROFILE;
  msg.params["fields"] = fields;
  msg.params["keywords"] = keywords;
  msg.params["affiliation"] = affiliation;
  
  send_message(msg);
  protocol::Response resp;
  receive_response(resp);
  
  std::cout << resp.message << "\n";
}

void ReviewClient::get_reviewer_profile() {
  protocol::Message msg;
  msg.command = protocol::Command::GET_REVIEWER_PROFILE;
  
  send_message(msg);
  protocol::Response resp;
  receive_response(resp);
  
  std::cout << "\n" << resp.message << "\n";
  if (!resp.body.empty()) {
    std::cout << std::string(resp.body.begin(), resp.body.end()) << "\n";
  }
}

void ReviewClient::get_reviewer_recommendations() {
  std::cout << "Paper ID: ";
  std::string paper_id = read_line();
  
  std::cout << "Top K (default 5): ";
  std::string k_str = read_line();
  int k = k_str.empty() ? 5 : std::stoi(k_str);
  
  protocol::Message msg;
  msg.command = protocol::Command::GET_REVIEWER_RECOMMENDATIONS;
  msg.params["paper_id"] = paper_id;
  msg.params["k"] = std::to_string(k);
  
  send_message(msg);
  protocol::Response resp;
  receive_response(resp);
  
  std::cout << "\n";
  if (!resp.body.empty()) {
    std::cout << std::string(resp.body.begin(), resp.body.end()) << "\n";
  } else {
    std::cout << resp.message << "\n";
  }
}

void ReviewClient::auto_assign_reviewers() {
  std::cout << "Paper ID: ";
  std::string paper_id = read_line();
  
  std::cout << "Number of reviewers to assign: ";
  std::string n_str = read_line();
  int n = std::stoi(n_str);
  
  protocol::Message msg;
  msg.command = protocol::Command::AUTO_ASSIGN_REVIEWERS;
  msg.params["paper_id"] = paper_id;
  msg.params["n"] = std::to_string(n);
  
  send_message(msg);
  protocol::Response resp;
  receive_response(resp);
  
  std::cout << "\n" << resp.message << "\n";
  if (!resp.body.empty()) {
    std::cout << std::string(resp.body.begin(), resp.body.end()) << "\n";
  }
}

} // namespace client
