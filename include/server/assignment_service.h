#ifndef ASSIGNMENT_SERVICE_H
#define ASSIGNMENT_SERVICE_H

#include "auth_manager.h"
#include "filesystem/vfs.h"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace server {

// Paper metadata structure
struct PaperMeta {
  std::string paper_id;
  std::string author;
  std::string title;
  std::vector<std::string> fields;
  std::vector<std::string> keywords;
  std::vector<std::string> conflict_usernames; // Author-specified COI
  std::string status;
};

// Reviewer profile structure
struct ReviewerProfile {
  std::string username;
  std::vector<std::string> fields;
  std::vector<std::string> keywords;
  std::string affiliation;
  std::vector<std::string> coauthors; // For future COI detection
};

// Assignment record
struct Assignment {
  std::string paper_id;
  std::string reviewer;
  time_t assigned_at;
  std::string state; // "pending" or "submitted"
};

// Recommendation result
struct RecommendationResult {
  std::string reviewer;
  double relevance_score;
  double final_score;
  int active_load;
  bool coi_blocked;
  std::string coi_reason;
};

// Configuration
struct AssignmentConfig {
  double lambda;      // Load penalty weight
  int max_active;     // Max active assignments per reviewer
  
  AssignmentConfig() : lambda(0.5), max_active(5) {}
};

/**
 * @brief Assignment Service for automated reviewer assignment
 * Handles COI detection, relevance scoring, load balancing
 */
class AssignmentService {
public:
  AssignmentService(std::shared_ptr<vfs::VirtualFileSystem> vfs,
                    std::shared_ptr<AuthManager> auth_manager);

  // ===== Configuration =====
  bool load_config();
  bool save_config();
  AssignmentConfig get_config() const { return config_; }
  void set_config(const AssignmentConfig &config) { config_ = config; }

  // ===== Paper Metadata =====
  bool load_paper_meta(const std::string &paper_id, PaperMeta &meta);
  bool save_paper_meta(const PaperMeta &meta);

  // ===== Reviewer Profile =====
  bool load_reviewer_profile(const std::string &username,
                             ReviewerProfile &profile);
  bool save_reviewer_profile(const ReviewerProfile &profile);

  // ===== Assignment Management =====
  bool load_assignments(const std::string &paper_id,
                        std::vector<Assignment> &assignments);
  bool save_assignments(const std::string &paper_id,
                        const std::vector<Assignment> &assignments);
  int get_active_load(const std::string &reviewer);

  // ===== COI Detection =====
  struct COIResult {
    bool has_conflict;
    std::string reason;
  };
  COIResult check_coi(const PaperMeta &paper, const ReviewerProfile &reviewer);

  // ===== Relevance Scoring =====
  double compute_relevance(const PaperMeta &paper,
                           const ReviewerProfile &reviewer);

  // ===== Auto Recommendation =====
  std::vector<RecommendationResult>
  recommend_reviewers(const std::string &paper_id, int top_k);

  // ===== Auto Assignment =====
  struct AssignResult {
    bool success;
    std::string message;
    std::vector<std::string> assigned_reviewers;
  };
  AssignResult auto_assign(const std::string &paper_id, int num_reviewers);

private:
  std::shared_ptr<vfs::VirtualFileSystem> vfs_;
  std::shared_ptr<AuthManager> auth_manager_;
  AssignmentConfig config_;

  // Helper functions
  std::set<std::string> tokenize(const std::string &text);
  std::vector<std::string> split(const std::string &str, char delim);
  std::string join(const std::vector<std::string> &vec, const std::string &delim);
  std::string trim(const std::string &str);
  std::string to_lower(const std::string &str);
};

} // namespace server

#endif // ASSIGNMENT_SERVICE_H

