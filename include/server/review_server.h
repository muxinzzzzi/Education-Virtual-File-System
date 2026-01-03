#ifndef REVIEW_SERVER_H
#define REVIEW_SERVER_H

#include "common/protocol.h"
#include "filesystem/vfs.h"
#include "server/auth_manager.h"
#include "server/assignment_service.h"
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace server {

class ReviewServer {
public:
  ReviewServer(int port, const std::string &fs_image_path);
  ~ReviewServer();

  // Server lifecycle
  bool start();
  void stop();
  bool is_running() const { return running_; }

private:
  int port_;
  std::string fs_image_path_;
  int server_socket_;
  std::atomic<bool> running_;

  std::shared_ptr<vfs::VirtualFileSystem> vfs_;
  std::shared_ptr<AuthManager> auth_manager_;
  std::unique_ptr<AssignmentService> assignment_service_;

  std::vector<std::thread> worker_threads_;

  // Network handling
  void accept_connections();
  void handle_client(int client_socket);

  // Command handlers
  protocol::Response handle_command(const protocol::Message &msg,
                                    std::string &session_id);

  protocol::Response handle_login(const protocol::Message &msg,
                                  std::string &session_id);
  protocol::Response handle_logout(const std::string &session_id);

  // Author commands
  protocol::Response handle_upload_paper(const protocol::Message &msg,
                                         const std::string &session_id);
  protocol::Response handle_submit_revision(const protocol::Message &msg,
                                            const std::string &session_id);
  protocol::Response handle_view_paper_status(const protocol::Message &msg,
                                              const std::string &session_id);

  // Reviewer commands
  protocol::Response handle_download_paper(const protocol::Message &msg,
                                           const std::string &session_id);
  protocol::Response handle_submit_review(const protocol::Message &msg,
                                          const std::string &session_id);

  // Editor commands
  protocol::Response handle_assign_reviewer(const protocol::Message &msg,
                                            const std::string &session_id);
  protocol::Response handle_make_decision(const protocol::Message &msg,
                                          const std::string &session_id);

  // Admin commands
  protocol::Response handle_create_user(const protocol::Message &msg,
                                        const std::string &session_id);
  protocol::Response handle_system_status(const std::string &session_id);
  protocol::Response handle_create_backup(const protocol::Message &msg,
                                          const std::string &session_id);
  protocol::Response handle_list_users(const std::string &session_id);
  protocol::Response handle_list_backups(const std::string &session_id);
  protocol::Response handle_restore_backup(const protocol::Message &msg,
                                           const std::string &session_id);

  // Missing handlers
  protocol::Response handle_download_reviews(const protocol::Message &msg,
                                             const std::string &session_id);
  protocol::Response handle_view_review_status(const protocol::Message &msg,
                                               const std::string &session_id);
  protocol::Response handle_view_pending_papers(const std::string &session_id);
  protocol::Response handle_view_review_progress(const protocol::Message &msg,
                                                 const std::string &session_id);

  // Assignment & Profile commands
  protocol::Response handle_set_reviewer_profile(const protocol::Message &msg,
                                                 const std::string &session_id);
  protocol::Response handle_get_reviewer_profile(const protocol::Message &msg,
                                                 const std::string &session_id);
  protocol::Response handle_get_reviewer_recommendations(
      const protocol::Message &msg, const std::string &session_id);
  protocol::Response handle_auto_assign_reviewers(const protocol::Message &msg,
                                                  const std::string &session_id);

  // Helpers
  bool send_response(int socket, const protocol::Response &response);
  bool receive_message(int socket, protocol::Message &message);
};

} // namespace server

#endif // REVIEW_SERVER_H
