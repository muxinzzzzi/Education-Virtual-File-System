#include "server/auth_manager.h"
#include <ctime>
#include <iomanip>
#include <openssl/sha.h>
#include <random>
#include <sstream>

namespace server {

AuthManager::AuthManager() {
  // Create default admin user
  create_user("admin", "admin123", protocol::Role::ADMIN, "admin@review.sys");
}

std::string AuthManager::hash_password(const std::string &password) const {
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char *>(password.c_str()),
         password.length(), hash);

  std::ostringstream oss;
  for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
    oss << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(hash[i]);
  }
  return oss.str();
}

std::string AuthManager::generate_session_id() const {
  std::random_device rd;
  std::mt19937_64 gen(rd());
  std::uniform_int_distribution<uint64_t> dis;

  uint64_t random_val = dis(gen);
  std::ostringstream oss;
  oss << std::hex << random_val << std::time(nullptr);
  return oss.str();
}

bool AuthManager::verify_password(const std::string &password,
                                  const std::string &hash) const {
  return hash_password(password) == hash;
}

bool AuthManager::create_user(const std::string &username,
                              const std::string &password, protocol::Role role,
                              const std::string &email,
                              const std::string &affiliation) {
  std::unique_lock<std::shared_mutex> lock(mutex_);

  if (users_.find(username) != users_.end()) {
    return false; // User already exists
  }

  User user;
  user.username = username;
  user.password_hash = hash_password(password);
  user.role = role;
  user.email = email;
  user.affiliation = affiliation;
  user.created_time = std::time(nullptr);
  user.active = true;

  users_[username] = user;
  return true;
}

bool AuthManager::delete_user(const std::string &username) {
  std::unique_lock<std::shared_mutex> lock(mutex_);

  auto it = users_.find(username);
  if (it == users_.end()) {
    return false;
  }

  users_.erase(it);

  // Invalidate all sessions for this user
  for (auto it = sessions_.begin(); it != sessions_.end();) {
    if (it->second.username == username) {
      it = sessions_.erase(it);
    } else {
      ++it;
    }
  }

  return true;
}

bool AuthManager::user_exists(const std::string &username) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  return users_.find(username) != users_.end();
}

std::vector<User> AuthManager::list_users() const {
  std::shared_lock<std::shared_mutex> lock(mutex_);

  std::vector<User> result;
  for (const auto &pair : users_) {
    result.push_back(pair.second);
  }
  return result;
}

std::string AuthManager::authenticate(const std::string &username,
                                      const std::string &password) {
  std::unique_lock<std::shared_mutex> lock(mutex_);

  auto it = users_.find(username);
  if (it == users_.end() || !it->second.active) {
    return ""; // User not found or inactive
  }

  if (!verify_password(password, it->second.password_hash)) {
    return ""; // Wrong password
  }

  // Create session
  Session session;
  session.session_id = generate_session_id();
  session.username = username;
  session.role = it->second.role;
  session.created_time = std::time(nullptr);
  session.last_access_time = session.created_time;

  sessions_[session.session_id] = session;

  return session.session_id;
}

bool AuthManager::validate_session(const std::string &session_id) {
  std::unique_lock<std::shared_mutex> lock(mutex_);

  auto it = sessions_.find(session_id);
  if (it == sessions_.end()) {
    return false;
  }

  // Update last access time
  it->second.last_access_time = std::time(nullptr);

  // Check if session is expired (24 hours)
  if (it->second.last_access_time - it->second.created_time > 24 * 3600) {
    sessions_.erase(it);
    return false;
  }

  return true;
}

void AuthManager::logout(const std::string &session_id) {
  std::unique_lock<std::shared_mutex> lock(mutex_);
  sessions_.erase(session_id);
}

bool AuthManager::authorize(const std::string &session_id,
                            protocol::Command command) {
  std::shared_lock<std::shared_mutex> lock(mutex_);

  auto it = sessions_.find(session_id);
  if (it == sessions_.end()) {
    return false;
  }

  protocol::Role role = it->second.role;

  // Define authorization rules
  switch (command) {
  case protocol::Command::UPLOAD_PAPER:
  case protocol::Command::SUBMIT_REVISION:
  case protocol::Command::VIEW_PAPER_STATUS:
  case protocol::Command::DOWNLOAD_REVIEWS:
    return role == protocol::Role::AUTHOR || role == protocol::Role::ADMIN;

  case protocol::Command::VIEW_ASSIGNED_PAPERS:
  case protocol::Command::DOWNLOAD_PAPER:
  case protocol::Command::SUBMIT_REVIEW:
  case protocol::Command::VIEW_REVIEW_STATUS:
    return role == protocol::Role::REVIEWER || role == protocol::Role::ADMIN;

  case protocol::Command::VIEW_PENDING_PAPERS:
  case protocol::Command::ASSIGN_REVIEWER:
  case protocol::Command::VIEW_REVIEW_PROGRESS:
  case protocol::Command::MAKE_DECISION:
    return role == protocol::Role::EDITOR || role == protocol::Role::ADMIN;

  case protocol::Command::CREATE_USER:
  case protocol::Command::DELETE_USER:
  case protocol::Command::LIST_USERS:
  case protocol::Command::CREATE_BACKUP:
  case protocol::Command::RESTORE_BACKUP:
  case protocol::Command::LIST_BACKUPS:
  case protocol::Command::SYSTEM_STATUS:
    return role == protocol::Role::ADMIN;

  case protocol::Command::SET_REVIEWER_PROFILE:
  case protocol::Command::GET_REVIEWER_PROFILE:
    // Reviewers and admins can manage profiles
    return role == protocol::Role::REVIEWER || role == protocol::Role::ADMIN;

  case protocol::Command::GET_REVIEWER_RECOMMENDATIONS:
  case protocol::Command::AUTO_ASSIGN_REVIEWERS:
    // Editors and admins can use auto-assignment
    return role == protocol::Role::EDITOR || role == protocol::Role::ADMIN;

  case protocol::Command::LOGIN:
  case protocol::Command::LOGOUT:
    return true; // Everyone can login/logout

  default:
    return false;
  }
}

protocol::Role AuthManager::get_user_role(const std::string &session_id) {
  std::shared_lock<std::shared_mutex> lock(mutex_);

  auto it = sessions_.find(session_id);
  if (it == sessions_.end()) {
    return protocol::Role::UNKNOWN;
  }

  return it->second.role;
}

std::string AuthManager::get_username(const std::string &session_id) {
  std::shared_lock<std::shared_mutex> lock(mutex_);

  auto it = sessions_.find(session_id);
  if (it == sessions_.end()) {
    return "";
  }

  return it->second.username;
}

bool AuthManager::load_from_filesystem(const std::string &fs_path) {
  // TODO: Implement loading users from VFS
  return true;
}

bool AuthManager::save_to_filesystem(const std::string &fs_path) {
  // TODO: Implement saving users to VFS
  return true;
}

} // namespace server
