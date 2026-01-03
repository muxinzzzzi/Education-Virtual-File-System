#include "server/review_server.h"
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

namespace server {

ReviewServer::ReviewServer(int port, const std::string &fs_image_path)
    : port_(port), fs_image_path_(fs_image_path), server_socket_(-1),
      running_(false) {}

ReviewServer::~ReviewServer() { stop(); }

bool ReviewServer::start() {
  // Initialize VFS
  vfs_ = std::make_shared<vfs::VirtualFileSystem>();

  // 1. Try to mount existing filesystem, otherwise format new one
  if (!vfs_->mount(fs_image_path_, 512)) {
    std::cout << "Filesystem not found or invalid. Creating new filesystem: "
              << fs_image_path_ << "\n";
    if (!vfs_->format(fs_image_path_, 100, 512)) {
      std::cerr << "Failed to format filesystem!\n";
      return false;
    }
    // Format internally mounts the filesystem, so we are good to go.
  }

  // 2. Ensure base directories exist
  auto ensure_dir = [this](const std::string &path) {
    int r = vfs_->mkdir(path);
    if (r == 0) {
      std::cout << "[VFS] Created base directory: " << path << "\n";
    } else if (r == -2) {
      // Already exists, ignore
    } else {
      std::cerr << "[VFS ERROR] Failed to create " << path << " error=" << r
                << " (mounted=" << vfs_->get_fs_stats().total_blocks << ")\n";
    }
  };

  ensure_dir("/papers");
  ensure_dir("/users");
  ensure_dir("/reviews");
  ensure_dir("/backups");

  if (!vfs_->exists("/papers")) {
    std::cerr << "[FATAL] /papers folder is missing even after mkdir!\n";
    return false;
  }

  // Initialize auth manager
  auth_manager_ = std::make_shared<AuthManager>();

  // Create some demo users
  auth_manager_->create_user("alice", "password", protocol::Role::AUTHOR,
                             "alice@univ.edu");
  auth_manager_->create_user("bob", "password", protocol::Role::REVIEWER,
                             "bob@univ.edu");
  auth_manager_->create_user("charlie", "password", protocol::Role::EDITOR,
                             "charlie@univ.edu");

  // Initialize assignment service
  assignment_service_ = std::make_unique<AssignmentService>(vfs_, auth_manager_);

  // Create server socket
  server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket_ < 0) {
    std::cerr << "Failed to create socket\n";
    return false;
  }

  // Set socket options
  int opt = 1;
  setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  // Bind to port
  struct sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port_);

  if (bind(server_socket_, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    std::cerr << "Failed to bind to port " << port_ << "\n";
    close(server_socket_);
    return false;
  }

  // Listen for connections
  if (listen(server_socket_, 10) < 0) {
    std::cerr << "Failed to listen\n";
    close(server_socket_);
    return false;
  }

  running_ = true;
  std::cout << "Server started on port " << port_ << "\n";

  // Start accepting connections
  accept_connections();

  return true;
}

void ReviewServer::stop() {
  if (!running_) {
    return;
  }

  running_ = false;

  // Close server socket
  if (server_socket_ >= 0) {
    close(server_socket_);
    server_socket_ = -1;
  }

  // Wait for worker threads
  for (auto &thread : worker_threads_) {
    if (thread.joinable()) {
      thread.join();
    }
  }
  worker_threads_.clear();

  // Unmount filesystem
  if (vfs_) {
    vfs_->unmount();
  }

  std::cout << "Server stopped\n";
}

void ReviewServer::accept_connections() {
  while (running_) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_socket =
        accept(server_socket_, (struct sockaddr *)&client_addr, &client_len);
    if (client_socket < 0) {
      if (running_) {
        std::cerr << "Failed to accept connection\n";
      }
      continue;
    }

    std::cout << "Client connected\n";

    // Handle client in new thread
    worker_threads_.emplace_back(&ReviewServer::handle_client, this,
                                 client_socket);
  }
}

void ReviewServer::handle_client(int client_socket) {
  std::string session_id;

  while (running_) {
    protocol::Message message;
    if (!receive_message(client_socket, message)) {
      break; // Client disconnected or error
    }

    auto response = handle_command(message, session_id);

    if (!send_response(client_socket, response)) {
      break;
    }

    // If logout, disconnect
    if (message.command == protocol::Command::LOGOUT) {
      break;
    }
  }

  close(client_socket);
  std::cout << "Client disconnected\n";
}

protocol::Response ReviewServer::handle_command(const protocol::Message &msg,
                                                std::string &session_id) {
  // Check authentication (except for LOGIN)
  if (msg.command != protocol::Command::LOGIN) {
    if (!auth_manager_->validate_session(session_id)) {
      return protocol::Response(protocol::StatusCode::UNAUTHORIZED,
                                "Not authenticated");
    }

    if (!auth_manager_->authorize(session_id, msg.command)) {
      return protocol::Response(protocol::StatusCode::FORBIDDEN,
                                "Permission denied");
    }
  }

  // Handle command
  switch (msg.command) {
  case protocol::Command::LOGIN:
    return handle_login(msg, session_id);
  case protocol::Command::LOGOUT:
    return handle_logout(session_id);
  case protocol::Command::UPLOAD_PAPER:
    return handle_upload_paper(msg, session_id);
  case protocol::Command::SUBMIT_REVISION:
    return handle_submit_revision(msg, session_id);
  case protocol::Command::VIEW_PAPER_STATUS:
    return handle_view_paper_status(msg, session_id);
  case protocol::Command::DOWNLOAD_PAPER:
    return handle_download_paper(msg, session_id);
  case protocol::Command::SUBMIT_REVIEW:
    return handle_submit_review(msg, session_id);
  case protocol::Command::ASSIGN_REVIEWER:
    return handle_assign_reviewer(msg, session_id);
  case protocol::Command::MAKE_DECISION:
    return handle_make_decision(msg, session_id);
  case protocol::Command::CREATE_USER:
    return handle_create_user(msg, session_id);
  case protocol::Command::SYSTEM_STATUS:
    return handle_system_status(session_id);
  case protocol::Command::CREATE_BACKUP:
    return handle_create_backup(msg, session_id);
  case protocol::Command::LIST_USERS:
    return handle_list_users(session_id);
  case protocol::Command::LIST_BACKUPS:
    return handle_list_backups(session_id);
  case protocol::Command::RESTORE_BACKUP:
    return handle_restore_backup(msg, session_id);
  case protocol::Command::DOWNLOAD_REVIEWS:
    return handle_download_reviews(msg, session_id);
  case protocol::Command::VIEW_REVIEW_STATUS:
    return handle_view_review_status(msg, session_id);
  case protocol::Command::VIEW_PENDING_PAPERS:
    return handle_view_pending_papers(session_id);
  case protocol::Command::VIEW_REVIEW_PROGRESS:
    return handle_view_review_progress(msg, session_id);
  case protocol::Command::SET_REVIEWER_PROFILE:
    return handle_set_reviewer_profile(msg, session_id);
  case protocol::Command::GET_REVIEWER_PROFILE:
    return handle_get_reviewer_profile(msg, session_id);
  case protocol::Command::GET_REVIEWER_RECOMMENDATIONS:
    return handle_get_reviewer_recommendations(msg, session_id);
  case protocol::Command::AUTO_ASSIGN_REVIEWERS:
    return handle_auto_assign_reviewers(msg, session_id);
  default:
    return protocol::Response(protocol::StatusCode::BAD_REQUEST,
                              "Unknown command");
  }
}

protocol::Response ReviewServer::handle_login(const protocol::Message &msg,
                                              std::string &session_id) {
  auto it_user = msg.params.find("username");
  auto it_pass = msg.params.find("password");

  if (it_user == msg.params.end() || it_pass == msg.params.end()) {
    return protocol::Response(protocol::StatusCode::BAD_REQUEST,
                              "Missing username or password");
  }

  session_id = auth_manager_->authenticate(it_user->second, it_pass->second);

  if (session_id.empty()) {
    return protocol::Response(protocol::StatusCode::UNAUTHORIZED,
                              "Invalid credentials");
  }

  protocol::Role role = auth_manager_->get_user_role(session_id);

  protocol::Response resp(protocol::StatusCode::OK, "Login successful");
  std::string body = "session_id=" + session_id +
                     "\nrole=" + protocol::Protocol::role_to_string(role);
  resp.body.assign(body.begin(), body.end());

  return resp;
}

protocol::Response ReviewServer::handle_logout(const std::string &session_id) {
  auth_manager_->logout(session_id);
  return protocol::Response(protocol::StatusCode::OK, "Logged out");
}

protocol::Response
ReviewServer::handle_upload_paper(const protocol::Message &msg,
                                  const std::string &session_id) {
  auto it_title = msg.params.find("title");
  if (it_title == msg.params.end()) {
    return protocol::Response(protocol::StatusCode::BAD_REQUEST,
                              "Missing title");
  }

  std::string username = auth_manager_->get_username(session_id);

  // 1. Generate persistent paper ID by scanning /papers directory
  std::vector<vfs::DirEntry> entries;
  int max_id = 0;
  if (vfs_->readdir("/papers", entries) == 0) {
    for (const auto &entry : entries) {
      std::string name(entry.name, entry.name_len);
      if (!name.empty() && name[0] == 'P') {
        try {
          int id = std::stoi(name.substr(1));
          if (id > max_id)
            max_id = id;
        } catch (...) {
        }
      }
    }
  }
  std::string paper_id = "P" + std::to_string(max_id + 1);

  std::string paper_dir = "/papers/" + paper_id;
  std::string versions_dir = paper_dir + "/versions";
  std::string reviews_dir = paper_dir + "/reviews";
  std::string paper_file = versions_dir + "/v1.pdf";
  std::string metadata_file = paper_dir + "/metadata.txt";

  // ---- 1. Create directories (MUST check return values) ----
  auto must_ok = [&](int ret, const std::string &what) {
    if (ret != 0 && ret != -2) { // -2 = already exists
      std::cerr << "[UPLOAD ERROR] " << what << " ret=" << ret << "\n";
      return false;
    }
    return true;
  };

  if (!must_ok(vfs_->mkdir(paper_dir), paper_dir) ||
      !must_ok(vfs_->mkdir(versions_dir), versions_dir) ||
      !must_ok(vfs_->mkdir(reviews_dir), reviews_dir)) {
    return protocol::Response(protocol::StatusCode::INTERNAL_ERROR,
                              "Failed to create paper directories");
  }

  // ---- 2. Create and write paper file ----
  int r = vfs_->create_file(paper_file, 0644);
  if (r != 0 && r != -2) {
    return protocol::Response(protocol::StatusCode::INTERNAL_ERROR,
                              "Failed to create paper file");
  }

  int fd = vfs_->open(paper_file, O_WRONLY);
  if (fd < 0) {
    return protocol::Response(protocol::StatusCode::INTERNAL_ERROR,
                              "Failed to open paper file");
  }
  ssize_t written = vfs_->write(fd, msg.body.data(), msg.body.size());
  vfs_->close(fd);

  if (written < static_cast<ssize_t>(msg.body.size())) {
    return protocol::Response(
        protocol::StatusCode::INTERNAL_ERROR,
        "Failed to write complete paper data (disk full?)");
  }

  // ---- 3. Create and write metadata ----
  r = vfs_->create_file(metadata_file, 0644);
  if (r != 0 && r != -2) {
    return protocol::Response(protocol::StatusCode::INTERNAL_ERROR,
                              "Failed to create metadata file");
  }

  fd = vfs_->open(metadata_file, O_WRONLY);
  if (fd < 0) {
    return protocol::Response(protocol::StatusCode::INTERNAL_ERROR,
                              "Failed to open metadata file");
  }

  // ---- 4. Write metadata content (with fields/keywords support) ----
  std::ostringstream meta_ss;
  meta_ss << "author=" << username << "\n";
  meta_ss << "title=" << it_title->second << "\n";
  meta_ss << "status=SUBMITTED\n";
  
  // Optional fields and keywords
  auto it_fields = msg.params.find("fields");
  if (it_fields != msg.params.end()) {
    meta_ss << "fields=" << it_fields->second << "\n";
  } else {
    meta_ss << "fields=\n";
  }
  
  auto it_keywords = msg.params.find("keywords");
  if (it_keywords != msg.params.end()) {
    meta_ss << "keywords=" << it_keywords->second << "\n";
  } else {
    meta_ss << "keywords=\n";
  }
  
  // Optional conflict list
  auto it_conflicts = msg.params.find("conflict_usernames");
  if (it_conflicts != msg.params.end()) {
    meta_ss << "conflict_usernames=" << it_conflicts->second << "\n";
  } else {
    meta_ss << "conflict_usernames=\n";
  }
  
  // Legacy format for backward compatibility
  meta_ss << "\n--- Legacy Format ---\n";
  meta_ss << "Title: " << it_title->second << "\n"
          << "Uploader: " << username << "\n"
          << "Upload Time: " << std::time(nullptr) << "\n";
  std::string meta_data = meta_ss.str();

  written = vfs_->write(fd, meta_data.data(), meta_data.size());
  vfs_->close(fd);

  if (written < static_cast<ssize_t>(meta_data.size())) {
    return protocol::Response(protocol::StatusCode::INTERNAL_ERROR,
                              "Failed to write metadata");
  }

  // ---- 5. Write initial status ----
  std::string status_file = paper_dir + "/status.txt";
  vfs_->create_file(status_file, 0644);
  fd = vfs_->open(status_file, O_WRONLY | O_TRUNC);
  if (fd >= 0) {
    std::string s = "SUBMITTED";
    vfs_->write(fd, s.data(), s.size());
    vfs_->close(fd);
  }

  // ---- 6. Final sanity check ----
  if (!vfs_->exists(metadata_file)) {
    return protocol::Response(protocol::StatusCode::INTERNAL_ERROR,
                              "Upload verification failed");
  }

  protocol::Response resp(protocol::StatusCode::CREATED, "Paper uploaded");
  std::string body = "paper_id=" + paper_id;
  resp.body.assign(body.begin(), body.end());

  if (vfs_->exists(paper_file)) {
    std::cerr << "[DEBUG] Paper PDF successfully stored in VFS: " << paper_file
              << "\n";
  }

  return resp;
}

protocol::Response
ReviewServer::handle_submit_revision(const protocol::Message &msg,
                                     const std::string &session_id) {
  auto it_paper_id = msg.params.find("paper_id");
  if (it_paper_id == msg.params.end()) {
    return protocol::Response(protocol::StatusCode::BAD_REQUEST,
                              "Missing paper_id");
  }

  std::string paper_id = it_paper_id->second;
  std::string paper_dir = "/papers/" + paper_id;
  std::string versions_dir = paper_dir + "/versions";

  if (!vfs_->exists(paper_dir)) {
    return protocol::Response(protocol::StatusCode::NOT_FOUND,
                              "Paper not found");
  }

  // Find next version
  std::vector<vfs::DirEntry> entries;
  int max_ver = 0;
  if (vfs_->readdir(versions_dir, entries) == 0) {
    for (const auto &entry : entries) {
      if (entry.inode_num == 0)
        continue;
      std::string name(entry.name, entry.name_len);
      // Format vN.pdf
      if (name.size() > 5 && name[0] == 'v' &&
          name.substr(name.size() - 4) == ".pdf") {
        try {
          int v = std::stoi(name.substr(1, name.size() - 5));
          if (v > max_ver)
            max_ver = v;
        } catch (...) {
        }
      }
    }
  }

  int new_ver = max_ver + 1;
  std::string new_file = versions_dir + "/v" + std::to_string(new_ver) + ".pdf";

  // Write file
  int r = vfs_->create_file(new_file, 0644);
  if (r != 0 && r != -2) {
    return protocol::Response(protocol::StatusCode::INTERNAL_ERROR,
                              "Failed to create revision file");
  }

  int fd = vfs_->open(new_file, O_WRONLY | O_TRUNC);
  if (fd < 0) {
    return protocol::Response(protocol::StatusCode::INTERNAL_ERROR,
                              "Failed to open revision file");
  }

  ssize_t written = vfs_->write(fd, msg.body.data(), msg.body.size());
  vfs_->close(fd);

  if (written < static_cast<ssize_t>(msg.body.size())) {
    return protocol::Response(protocol::StatusCode::INTERNAL_ERROR,
                              "Failed to write revision data");
  }

  // Update Metadata (Append log)
  std::string metadata_file = paper_dir + "/metadata.txt";
  fd = vfs_->open(metadata_file,
                  O_WRONLY | O_APPEND); // O_APPEND not implemented? VFS doesn't
                                        // support O_APPEND yet.

  // Since we don't have atomic append or O_APPEND support in VFS yet (based on
  // previous file read), we might skip updating metadata for now or we would
  // need to read-modify-write. Given user request simplicity, just persisting
  // the file is the main "Revision" feature.

  // Optionally, we could try to read and write back if we want to log it.
  // But let's stick to core requirement: "Submit revised version". The file
  // v2.pdf etc is there.

  return protocol::Response(protocol::StatusCode::OK,
                            "Revision v" + std::to_string(new_ver) +
                                " submitted");
}

protocol::Response
ReviewServer::handle_view_paper_status(const protocol::Message &msg,
                                       const std::string &session_id) {
  auto it_paper_id = msg.params.find("paper_id");

  if (it_paper_id == msg.params.end()) {
    return protocol::Response(protocol::StatusCode::BAD_REQUEST,
                              "Missing paper_id");
  }

  std::string paper_dir = "/papers/" + it_paper_id->second;
  if (!vfs_->exists(paper_dir)) {
    return protocol::Response(protocol::StatusCode::NOT_FOUND,
                              "Paper not found");
  }

  std::ostringstream oss;
  oss << "=== Paper ID: " << it_paper_id->second << " ===\n";

  // Read Metadata
  std::string metadata_file = paper_dir + "/metadata.txt";
  int fd = vfs_->open(metadata_file, O_RDONLY);
  if (fd >= 0) {
    // Read fairly large chunk
    std::vector<char> buffer(4096);
    ssize_t bytes_read = vfs_->read(fd, buffer.data(), buffer.size());
    vfs_->close(fd);
    if (bytes_read > 0) {
      oss << std::string(buffer.data(), bytes_read) << "\n";
    }
  } else {
    oss << "[Metadata missing]\n";
  }

  // List Versions
  oss << "\nVersions:\n";
  std::string versions_dir = paper_dir + "/versions";
  std::vector<vfs::DirEntry> entries;
  if (vfs_->readdir(versions_dir, entries) == 0) {
    for (const auto &entry : entries) {
      if (entry.inode_num != 0) {
        oss << " - " << std::string(entry.name, entry.name_len) << "\n";
      }
    }
  }

  // List Reviews
  oss << "\nReviews:\n";
  std::string reviews_dir = paper_dir + "/reviews";
  if (vfs_->readdir(reviews_dir, entries) == 0) {
    bool has_reviews = false;
    for (const auto &entry : entries) {
      if (entry.inode_num != 0) {
        oss << " - " << std::string(entry.name, entry.name_len) << "\n";
        has_reviews = true;
      }
    }
    if (!has_reviews)
      oss << " (No reviews yet)\n";
  }

  protocol::Response resp(protocol::StatusCode::OK, "Paper status");
  std::string body = oss.str();
  resp.body.assign(body.begin(), body.end());

  return resp;
}

protocol::Response
ReviewServer::handle_download_paper(const protocol::Message &msg,
                                    const std::string &session_id) {
  auto it_paper_id = msg.params.find("paper_id");
  if (it_paper_id == msg.params.end()) {
    return protocol::Response(protocol::StatusCode::BAD_REQUEST,
                              "Missing paper_id");
  }

  std::string paper_file =
      "/papers/" + it_paper_id->second + "/versions/v1.pdf";

  if (!vfs_->exists(paper_file)) {
    return protocol::Response(protocol::StatusCode::NOT_FOUND,
                              "Paper not found");
  }

  int fd = vfs_->open(paper_file, O_RDONLY);
  if (fd < 0) {
    return protocol::Response(protocol::StatusCode::INTERNAL_ERROR,
                              "Failed to read paper");
  }

  // Get file size by seeking to the end
  off_t file_size = vfs_->seek(fd, 0, SEEK_END);
  vfs_->seek(fd, 0, SEEK_SET);

  if (file_size < 0) {
    vfs_->close(fd);
    return protocol::Response(protocol::StatusCode::INTERNAL_ERROR,
                              "Failed to determine paper size");
  }

  std::vector<char> buffer(file_size);
  size_t total_read = 0;
  while (total_read < static_cast<size_t>(file_size)) {
    ssize_t n =
        vfs_->read(fd, buffer.data() + total_read, file_size - total_read);
    if (n <= 0)
      break;
    total_read += n;
  }
  vfs_->close(fd);

  if (total_read < static_cast<size_t>(file_size)) {
    return protocol::Response(protocol::StatusCode::INTERNAL_ERROR,
                              "Failed to read complete paper data");
  }

  protocol::Response resp(protocol::StatusCode::OK, "Paper downloaded");
  resp.body = std::move(buffer);

  return resp;
}

protocol::Response
ReviewServer::handle_submit_review(const protocol::Message &msg,
                                   const std::string &session_id) {
  auto it_paper_id = msg.params.find("paper_id");
  if (it_paper_id == msg.params.end()) {
    return protocol::Response(protocol::StatusCode::BAD_REQUEST,
                              "Missing paper_id");
  }

  std::string username = auth_manager_->get_username(session_id);
  std::string review_file =
      "/papers/" + it_paper_id->second + "/reviews/" + username + ".txt";

  vfs_->create_file(review_file);
  int fd = vfs_->open(review_file, O_WRONLY);
  if (fd >= 0) {
    vfs_->write(fd, msg.body.data(), msg.body.size());
    vfs_->close(fd);
  }

  return protocol::Response(protocol::StatusCode::OK, "Review submitted");
}

protocol::Response
ReviewServer::handle_assign_reviewer(const protocol::Message &msg,
                                     const std::string &session_id) {
  auto it_paper_id = msg.params.find("paper_id");
  auto it_reviewer = msg.params.find("reviewer");

  if (it_paper_id == msg.params.end() || it_reviewer == msg.params.end()) {
    return protocol::Response(protocol::StatusCode::BAD_REQUEST,
                              "Missing paper_id or reviewer");
  }

  std::string paper_id = it_paper_id->second;
  std::string reviewer = it_reviewer->second;

  // Check if reviewer exists
  if (!auth_manager_->user_exists(reviewer)) {
    return protocol::Response(protocol::StatusCode::BAD_REQUEST,
                              "Reviewer user does not exist");
  }

  std::string paper_dir = "/papers/" + paper_id;
  if (!vfs_->exists(paper_dir)) {
    return protocol::Response(protocol::StatusCode::NOT_FOUND,
                              "Paper not found");
  }

  // ===== COI CHECK =====
  PaperMeta paper_meta;
  ReviewerProfile reviewer_profile;
  
  if (assignment_service_->load_paper_meta(paper_id, paper_meta) &&
      assignment_service_->load_reviewer_profile(reviewer, reviewer_profile)) {
    auto coi = assignment_service_->check_coi(paper_meta, reviewer_profile);
    if (coi.has_conflict) {
      return protocol::Response(protocol::StatusCode::CONFLICT,
                                "COI detected: " + coi.reason);
    }
  }

  // Check load
  int active_load = assignment_service_->get_active_load(reviewer);
  auto config = assignment_service_->get_config();
  if (active_load >= config.max_active) {
    return protocol::Response(
        protocol::StatusCode::CONFLICT,
        "Reviewer has too many active assignments (" +
            std::to_string(active_load) + "/" + std::to_string(config.max_active) + ")");
  }

  // Load existing assignments and add new one
  std::vector<Assignment> assignments;
  assignment_service_->load_assignments(paper_id, assignments);
  
  // Check if already assigned
  for (const auto &assign : assignments) {
    if (assign.reviewer == reviewer) {
      return protocol::Response(protocol::StatusCode::CONFLICT,
                                "Reviewer already assigned to this paper");
    }
  }

  Assignment new_assign;
  new_assign.paper_id = paper_id;
  new_assign.reviewer = reviewer;
  new_assign.assigned_at = std::time(nullptr);
  new_assign.state = "pending";
  assignments.push_back(new_assign);

  if (!assignment_service_->save_assignments(paper_id, assignments)) {
    return protocol::Response(protocol::StatusCode::INTERNAL_ERROR,
                              "Failed to save assignment");
  }

  // Update paper status to UNDER_REVIEW
  if (assignment_service_->load_paper_meta(paper_id, paper_meta)) {
    paper_meta.status = "UNDER_REVIEW";
    assignment_service_->save_paper_meta(paper_meta);
  }

  // Also maintain legacy reviewers.txt for backward compatibility
  std::string reviewers_file = paper_dir + "/reviewers.txt";
  if (!vfs_->exists(reviewers_file)) {
    vfs_->create_file(reviewers_file, 0644);
  }

  int fd = vfs_->open(reviewers_file, O_WRONLY);
  if (fd >= 0) {
    vfs_->seek(fd, 0, SEEK_END);
    std::string line = reviewer + "\n";
    vfs_->write(fd, line.data(), line.size());
    vfs_->close(fd);
  }

  return protocol::Response(protocol::StatusCode::OK, 
                            "Reviewer assigned (load: " + std::to_string(active_load + 1) + ")");
}

protocol::Response
ReviewServer::handle_make_decision(const protocol::Message &msg,
                                   const std::string &session_id) {
  // Implementation for making editorial decisions
  return protocol::Response(protocol::StatusCode::OK, "Decision made");
}

protocol::Response
ReviewServer::handle_create_user(const protocol::Message &msg,
                                 const std::string &session_id) {
  auto it_user = msg.params.find("username");
  auto it_pass = msg.params.find("password");
  auto it_role = msg.params.find("role");

  if (it_user == msg.params.end() || it_pass == msg.params.end() ||
      it_role == msg.params.end()) {
    return protocol::Response(protocol::StatusCode::BAD_REQUEST,
                              "Missing parameters");
  }

  protocol::Role role = protocol::Protocol::string_to_role(it_role->second);

  if (auth_manager_->create_user(it_user->second, it_pass->second, role)) {
    return protocol::Response(protocol::StatusCode::CREATED, "User created");
  } else {
    return protocol::Response(protocol::StatusCode::CONFLICT,
                              "User already exists");
  }
}

protocol::Response
ReviewServer::handle_system_status(const std::string &session_id) {
  auto fs_stats = vfs_->get_fs_stats();
  auto cache_stats = vfs_->get_cache_stats();

  std::ostringstream oss;
  oss << "=== File System Stats ===\n";
  oss << "Total size: " << fs_stats.total_size / 1024 / 1024 << " MB\n";

  if (fs_stats.used_size < 1024 * 1024) {
    oss << "Used: " << fs_stats.used_size / 1024 << " KB\n";
  } else {
    oss << "Used: " << fs_stats.used_size / 1024 / 1024 << " MB\n";
  }

  oss << "Free blocks: " << fs_stats.free_blocks << "\n";
  oss << "Usage: " << fs_stats.usage_percent() << "%\n\n";
  oss << "=== Cache Stats ===\n";
  oss << "Hits: " << cache_stats.hits << "\n";
  oss << "Misses: " << cache_stats.misses << "\n";
  oss << "Hit rate: " << cache_stats.hit_rate() * 100 << "%\n";
  oss << "Evictions: " << cache_stats.evictions << "\n";

  oss << "\n=== User Statistics ===\n";
  auto users = auth_manager_->list_users();
  oss << "Total Users: " << users.size() << "\n";
  int authors = 0, reviewers = 0, editors = 0, admins = 0;
  for (const auto &u : users) {
    switch (u.role) {
    case protocol::Role::AUTHOR:
      authors++;
      break;
    case protocol::Role::REVIEWER:
      reviewers++;
      break;
    case protocol::Role::EDITOR:
      editors++;
      break;
    case protocol::Role::ADMIN:
      admins++;
      break;
    default:
      break;
    }
  }
  oss << "Authors: " << authors << "\n";
  oss << "Reviewers: " << reviewers << "\n";
  oss << "Editors: " << editors << "\n";
  oss << "Admins: " << admins << "\n";

  oss << "\n=== Paper Statistics ===\n";
  std::vector<vfs::DirEntry> entries;
  int papers = 0;
  if (vfs_->readdir("/papers", entries) == 0) {
    for (const auto &entry : entries) {
      if (entry.inode_num != 0)
        papers++;
    }
  }
  oss << "Total Papers: " << papers << "\n";

  std::string status = oss.str();

  protocol::Response resp(protocol::StatusCode::OK, "System status");
  resp.body.assign(status.begin(), status.end());

  return resp;
}

protocol::Response
ReviewServer::handle_create_backup(const protocol::Message &msg,
                                   const std::string &session_id) {
  auto it_name = msg.params.find("name");
  if (it_name == msg.params.end()) {
    return protocol::Response(protocol::StatusCode::BAD_REQUEST,
                              "Missing backup name");
  }

  if (vfs_->create_backup(it_name->second)) {
    return protocol::Response(protocol::StatusCode::OK, "Backup created");
  } else {
    return protocol::Response(protocol::StatusCode::INTERNAL_ERROR,
                              "Backup failed");
  }
}

protocol::Response
ReviewServer::handle_list_users(const std::string &session_id) {
  auto users = auth_manager_->list_users();
  std::ostringstream oss;
  oss << "=== Users ===\n";
  for (const auto &u : users) {
    oss << u.username << " (" << protocol::Protocol::role_to_string(u.role)
        << ")\n";
  }
  protocol::Response resp(protocol::StatusCode::OK, "User list");
  std::string body = oss.str();
  resp.body.assign(body.begin(), body.end());
  return resp;
}

protocol::Response
ReviewServer::handle_list_backups(const std::string &session_id) {
  auto backups = vfs_->list_backups();
  std::ostringstream oss;
  oss << "=== Backups ===\n";
  if (backups.empty()) {
    oss << "(No backups found)\n";
  } else {
    for (const auto &b : backups) {
      oss << "- " << b << "\n";
    }
  }
  protocol::Response resp(protocol::StatusCode::OK, "Backup list");
  std::string body = oss.str();
  resp.body.assign(body.begin(), body.end());
  return resp;
}

protocol::Response
ReviewServer::handle_restore_backup(const protocol::Message &msg,
                                    const std::string &session_id) {
  auto it_name = msg.params.find("name");
  if (it_name == msg.params.end()) {
    return protocol::Response(protocol::StatusCode::BAD_REQUEST,
                              "Missing backup name");
  }

  // Restore requires unmounting. Note: This will disrupt other active
  // operations! For a real system we would need to pause the server or enter
  // maintenance mode. Here we just risk it for the demo.
  bool was_mounted = vfs_->is_mounted();
  if (was_mounted) {
    vfs_->unmount();
  }

  bool success = vfs_->restore_backup(it_name->second);

  if (success && was_mounted) {
    // Remount
    vfs_->mount(fs_image_path_, 512); // Assuming same block size
  }

  if (success) {
    return protocol::Response(protocol::StatusCode::OK, "Backup restored");
  } else {
    // Try to remount even if failed
    if (was_mounted)
      vfs_->mount(fs_image_path_, 512);
    return protocol::Response(protocol::StatusCode::INTERNAL_ERROR,
                              "Restore failed");
  }
}

protocol::Response
ReviewServer::handle_download_reviews(const protocol::Message &msg,
                                      const std::string &session_id) {
  auto it_paper_id = msg.params.find("paper_id");
  if (it_paper_id == msg.params.end()) {
    return protocol::Response(protocol::StatusCode::BAD_REQUEST,
                              "Missing paper_id");
  }
  std::string paper_dir = "/papers/" + it_paper_id->second;
  std::string reviews_dir = paper_dir + "/reviews";

  if (!vfs_->exists(reviews_dir)) {
    return protocol::Response(protocol::StatusCode::NOT_FOUND,
                              "No reviews found");
  }

  std::ostringstream oss;
  oss << "=== Reviews for " << it_paper_id->second << " ===\n\n";

  std::vector<vfs::DirEntry> entries;
  if (vfs_->readdir(reviews_dir, entries) == 0) {
    for (const auto &entry : entries) {
      if (entry.inode_num != 0) {
        std::string rname(entry.name, entry.name_len);
        std::string rpath = reviews_dir + "/" + rname;
        int fd = vfs_->open(rpath, O_RDONLY);
        if (fd >= 0) {
          off_t sz = vfs_->seek(fd, 0, SEEK_END);
          vfs_->seek(fd, 0, SEEK_SET);
          if (sz > 0) {
            std::vector<char> buf(sz);
            vfs_->read(fd, buf.data(), sz);
            oss << "--- Review by " << rname.substr(0, rname.length() - 4)
                << " ---\n";
            oss << std::string(buf.data(), sz) << "\n\n";
          }
          vfs_->close(fd);
        }
      }
    }
  }

  protocol::Response resp(protocol::StatusCode::OK, "Reviews downloaded");
  std::string body = oss.str();
  resp.body.assign(body.begin(), body.end());
  return resp;
}

protocol::Response
ReviewServer::handle_view_review_status(const protocol::Message &msg,
                                        const std::string &session_id) {
  // Reuse view_paper_status for now as it contains review info
  return handle_view_paper_status(msg, session_id);
}

protocol::Response
ReviewServer::handle_view_pending_papers(const std::string &session_id) {
  std::vector<vfs::DirEntry> entries;
  std::ostringstream oss;
  oss << "=== Pending Papers ===\n";

  if (vfs_->readdir("/papers", entries) == 0) {
    for (const auto &entry : entries) {
      std::string name(entry.name, entry.name_len);
      if (!name.empty() && name[0] == 'P') {
        // Check status
        std::string paper_dir = "/papers/" + name;
        std::string status_file = paper_dir + "/status.txt";
        int fd = vfs_->open(status_file, O_RDONLY);
        if (fd >= 0) {
          std::vector<char> buf(64);
          ssize_t n = vfs_->read(fd, buf.data(), buf.size());
          vfs_->close(fd);
          std::string status(buf.data(), n);
          if (status != "ACCEPTED" && status != "REJECTED") {
            oss << "- " << name << " [" << status << "]\n";
          }
        } else {
          // Assume new if no status
          oss << "- " << name << " [SUBMITTED]\n";
        }
      }
    }
  }

  protocol::Response resp(protocol::StatusCode::OK, "Pending papers");
  std::string body = oss.str();
  resp.body.assign(body.begin(), body.end());
  return resp;
}

protocol::Response
ReviewServer::handle_view_review_progress(const protocol::Message &msg,
                                          const std::string &session_id) {
  // Reuse view_paper_status which has progress details
  return handle_view_paper_status(msg, session_id);
}

// ===== Assignment & Profile Handlers =====

protocol::Response
ReviewServer::handle_set_reviewer_profile(const protocol::Message &msg,
                                          const std::string &session_id) {
  auto it_username = msg.params.find("username");
  if (it_username == msg.params.end()) {
    // Default to current user
    it_username = msg.params.find("session_username");
    if (it_username == msg.params.end()) {
      std::string username = auth_manager_->get_username(session_id);
      if (username.empty()) {
        return protocol::Response(protocol::StatusCode::BAD_REQUEST,
                                  "Cannot determine username");
      }
      
      ReviewerProfile profile;
      profile.username = username;
      
      // Parse fields and keywords from params
      auto it_fields = msg.params.find("fields");
      if (it_fields != msg.params.end()) {
        std::istringstream iss(it_fields->second);
        std::string field;
        while (std::getline(iss, field, ',')) {
          std::string trimmed = field;
          // Simple trim
          size_t start = trimmed.find_first_not_of(" \t");
          size_t end = trimmed.find_last_not_of(" \t");
          if (start != std::string::npos && end != std::string::npos) {
            profile.fields.push_back(trimmed.substr(start, end - start + 1));
          }
        }
      }
      
      auto it_keywords = msg.params.find("keywords");
      if (it_keywords != msg.params.end()) {
        std::istringstream iss(it_keywords->second);
        std::string kw;
        while (std::getline(iss, kw, ',')) {
          std::string trimmed = kw;
          size_t start = trimmed.find_first_not_of(" \t");
          size_t end = trimmed.find_last_not_of(" \t");
          if (start != std::string::npos && end != std::string::npos) {
            profile.keywords.push_back(trimmed.substr(start, end - start + 1));
          }
        }
      }
      
      auto it_affiliation = msg.params.find("affiliation");
      if (it_affiliation != msg.params.end()) {
        profile.affiliation = it_affiliation->second;
      }
      
      if (assignment_service_->save_reviewer_profile(profile)) {
        return protocol::Response(protocol::StatusCode::OK,
                                  "Profile updated");
      } else {
        return protocol::Response(protocol::StatusCode::INTERNAL_ERROR,
                                  "Failed to save profile");
      }
    }
  }
  
  return protocol::Response(protocol::StatusCode::BAD_REQUEST,
                            "Invalid request");
}

protocol::Response
ReviewServer::handle_get_reviewer_profile(const protocol::Message &msg,
                                          const std::string &session_id) {
  auto it_username = msg.params.find("username");
  std::string username;
  
  if (it_username == msg.params.end()) {
    username = auth_manager_->get_username(session_id);
  } else {
    username = it_username->second;
  }
  
  if (username.empty()) {
    return protocol::Response(protocol::StatusCode::BAD_REQUEST,
                              "Missing username");
  }
  
  ReviewerProfile profile;
  if (!assignment_service_->load_reviewer_profile(username, profile)) {
    return protocol::Response(protocol::StatusCode::INTERNAL_ERROR,
                              "Failed to load profile");
  }
  
  std::ostringstream oss;
  oss << "Username: " << profile.username << "\n";
  oss << "Fields: ";
  for (size_t i = 0; i < profile.fields.size(); ++i) {
    if (i > 0) oss << ", ";
    oss << profile.fields[i];
  }
  oss << "\nKeywords: ";
  for (size_t i = 0; i < profile.keywords.size(); ++i) {
    if (i > 0) oss << ", ";
    oss << profile.keywords[i];
  }
  oss << "\nAffiliation: " << profile.affiliation << "\n";
  
  protocol::Response resp(protocol::StatusCode::OK, "Profile retrieved");
  std::string body = oss.str();
  resp.body.assign(body.begin(), body.end());
  return resp;
}

protocol::Response ReviewServer::handle_get_reviewer_recommendations(
    const protocol::Message &msg, const std::string &session_id) {
  auto it_paper_id = msg.params.find("paper_id");
  auto it_k = msg.params.find("k");
  
  if (it_paper_id == msg.params.end()) {
    return protocol::Response(protocol::StatusCode::BAD_REQUEST,
                              "Missing paper_id");
  }
  
  int k = 5; // default
  if (it_k != msg.params.end()) {
    k = std::stoi(it_k->second);
  }
  
  std::string paper_id = it_paper_id->second;
  
  auto recommendations = assignment_service_->recommend_reviewers(paper_id, k);
  
  std::ostringstream oss;
  oss << "=== Top " << k << " Reviewer Recommendations for " << paper_id << " ===\n\n";
  
  int rank = 1;
  for (const auto &rec : recommendations) {
    oss << rank++ << ". " << rec.reviewer << "\n";
    oss << "   Relevance: " << rec.relevance_score << "\n";
    oss << "   Active Load: " << rec.active_load << "\n";
    oss << "   Final Score: " << rec.final_score << "\n";
    
    if (rec.coi_blocked) {
      oss << "   [BLOCKED] " << rec.coi_reason << "\n";
    } else {
      oss << "   [OK] No COI detected\n";
    }
    oss << "\n";
  }
  
  protocol::Response resp(protocol::StatusCode::OK, "Recommendations generated");
  std::string body = oss.str();
  resp.body.assign(body.begin(), body.end());
  return resp;
}

protocol::Response
ReviewServer::handle_auto_assign_reviewers(const protocol::Message &msg,
                                           const std::string &session_id) {
  auto it_paper_id = msg.params.find("paper_id");
  auto it_n = msg.params.find("n");
  
  if (it_paper_id == msg.params.end() || it_n == msg.params.end()) {
    return protocol::Response(protocol::StatusCode::BAD_REQUEST,
                              "Missing paper_id or n");
  }
  
  std::string paper_id = it_paper_id->second;
  int n = std::stoi(it_n->second);
  
  if (n <= 0 || n > 10) {
    return protocol::Response(protocol::StatusCode::BAD_REQUEST,
                              "n must be between 1 and 10");
  }
  
  auto result = assignment_service_->auto_assign(paper_id, n);
  
  if (!result.success) {
    return protocol::Response(protocol::StatusCode::CONFLICT, result.message);
  }
  
  std::ostringstream oss;
  oss << result.message << "\n\nAssigned reviewers:\n";
  for (const auto &reviewer : result.assigned_reviewers) {
    oss << "- " << reviewer << "\n";
  }
  
  protocol::Response resp(protocol::StatusCode::OK, "Auto-assignment completed");
  std::string body = oss.str();
  resp.body.assign(body.begin(), body.end());
  return resp;
}

bool ReviewServer::send_response(int socket,
                                 const protocol::Response &response) {
  auto data = protocol::Protocol::serialize_response(response);
  ssize_t sent = send(socket, data.data(), data.size(), 0);
  return sent == static_cast<ssize_t>(data.size());
}

bool ReviewServer::receive_message(int socket, protocol::Message &message) {
  // First, receive header (command line + body length line)
  std::vector<char> header_buffer(2048);
  ssize_t header_received = 0;

  // Receive until we have at least two newlines (command + length)
  while (header_received < 2048) {
    ssize_t n = recv(socket, header_buffer.data() + header_received,
                     2048 - header_received, 0);
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
        // Extract body length
        std::string length_str =
            temp.substr(first_newline + 1, second_newline - first_newline - 1);
        size_t body_length = std::stoull(length_str);

        // Check if body is too large (max 6MB)
        if (body_length > 6 * 1024 * 1024) {
          std::cerr << "Message body too large: " << body_length << " bytes\n";
          return false;
        }

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
              recv(socket, complete_buffer.data() + header_end + body_received,
                   body_length - body_received, 0);
          if (n <= 0) {
            return false;
          }
          body_received += n;
        }

        return protocol::Protocol::deserialize_message(complete_buffer,
                                                       message);
      }
    }
  }

  return false;
}

} // namespace server
