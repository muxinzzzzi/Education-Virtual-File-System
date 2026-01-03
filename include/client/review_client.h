#ifndef REVIEW_CLIENT_H
#define REVIEW_CLIENT_H

#include "common/protocol.h"
#include <memory>
#include <string>

namespace client {

class ReviewClient {
public:
  ReviewClient();
  ~ReviewClient();

  // Connection management
  bool connect(const std::string &host, int port);
  void disconnect();
  bool is_connected() const { return connected_; }

  // Authentication
  bool login(const std::string &username, const std::string &password);
  void logout();

  // Run interactive CLI
  void run();

private:
  int socket_;
  bool connected_;
  std::string session_id_;
  protocol::Role current_role_;
  std::string username_;

  // Command sending/receiving
  bool send_message(const protocol::Message &msg);
  bool receive_response(protocol::Response &resp);

  // Menu displays
  void show_main_menu();
  void show_author_menu();
  void show_reviewer_menu();
  void show_editor_menu();
  void show_admin_menu();

  // Author commands
  void upload_paper();
  void view_paper_status();
  void submit_revision();
  void download_reviews();

  // Reviewer commands
  void download_paper();
  void submit_review();
  void view_review_status();

  // Editor commands
  void assign_reviewer();
  void make_decision();
  void view_pending_papers();
  void view_review_progress();

  // Admin commands
  void create_user();
  void view_system_status();
  void create_backup();
  void list_users();
  void list_backups();
  void restore_backup();
  
  // Profile & Assignment commands
  void set_reviewer_profile();
  void get_reviewer_profile();
  void get_reviewer_recommendations();
  void auto_assign_reviewers();

  // Helpers
  std::string read_line();
  std::vector<char> read_file(const std::string &path);
  void save_file(const std::string &path, const std::vector<char> &data);
};

} // namespace client

#endif // REVIEW_CLIENT_H
