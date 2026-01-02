#ifndef AUTH_MANAGER_H
#define AUTH_MANAGER_H

#include "common/protocol.h"
#include <map>
#include <memory>
#include <shared_mutex>
#include <string>

namespace server {

struct User {
  std::string username;
  std::string password_hash;
  protocol::Role role;
  std::string email;
  std::string affiliation;
  std::vector<std::string> research_fields;
  uint64_t created_time;
  bool active;

  User() : role(protocol::Role::UNKNOWN), created_time(0), active(true) {}
};

struct Session {
  std::string session_id;
  std::string username;
  protocol::Role role;
  uint64_t created_time;
  uint64_t last_access_time;

  Session()
      : role(protocol::Role::UNKNOWN), created_time(0), last_access_time(0) {}
};

class AuthManager {
public:
  AuthManager();

  // User management
  bool create_user(const std::string &username, const std::string &password,
                   protocol::Role role, const std::string &email = "",
                   const std::string &affiliation = "");
  bool delete_user(const std::string &username);
  bool user_exists(const std::string &username) const;
  std::vector<User> list_users() const;

  // Authentication
  std::string authenticate(const std::string &username,
                           const std::string &password);
  bool validate_session(const std::string &session_id);
  void logout(const std::string &session_id);

  // Authorization
  bool authorize(const std::string &session_id, protocol::Command command);
  protocol::Role get_user_role(const std::string &session_id);
  std::string get_username(const std::string &session_id);

  // Persistence
  bool load_from_filesystem(const std::string &fs_path);
  bool save_to_filesystem(const std::string &fs_path);

private:
  std::map<std::string, User> users_;
  std::map<std::string, Session> sessions_;
  mutable std::shared_mutex mutex_;

  std::string hash_password(const std::string &password) const;
  std::string generate_session_id() const;
  bool verify_password(const std::string &password,
                       const std::string &hash) const;
};

} // namespace server

#endif // AUTH_MANAGER_H
