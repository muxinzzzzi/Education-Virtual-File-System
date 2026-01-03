#include "server/assignment_service.h"
#include <algorithm>
#include <cctype>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <sstream>

namespace server {

AssignmentService::AssignmentService(
    std::shared_ptr<vfs::VirtualFileSystem> vfs,
    std::shared_ptr<AuthManager> auth_manager)
    : vfs_(vfs), auth_manager_(auth_manager) {
  load_config();
}

// ===== Configuration =====

bool AssignmentService::load_config() {
  std::string config_file = "/config/assignment.txt";
  
  if (!vfs_->exists(config_file)) {
    // Use defaults
    config_ = AssignmentConfig();
    return true;
  }

  int fd = vfs_->open(config_file, O_RDONLY);
  if (fd < 0) {
    return false;
  }

  char buffer[1024];
  ssize_t n = vfs_->read(fd, buffer, sizeof(buffer) - 1);
  vfs_->close(fd);

  if (n <= 0) {
    return false;
  }

  buffer[n] = '\0';
  std::istringstream iss(buffer);
  std::string line;

  while (std::getline(iss, line)) {
    size_t eq = line.find('=');
    if (eq != std::string::npos) {
      std::string key = trim(line.substr(0, eq));
      std::string value = trim(line.substr(eq + 1));

      if (key == "lambda") {
        config_.lambda = std::stod(value);
      } else if (key == "max_active") {
        config_.max_active = std::stoi(value);
      }
    }
  }

  return true;
}

bool AssignmentService::save_config() {
  // Ensure /config exists
  if (!vfs_->exists("/config")) {
    vfs_->mkdir("/config");
  }

  std::string config_file = "/config/assignment.txt";
  if (!vfs_->exists(config_file)) {
    vfs_->create_file(config_file);
  }

  std::ostringstream oss;
  oss << "lambda=" << config_.lambda << "\n";
  oss << "max_active=" << config_.max_active << "\n";

  std::string content = oss.str();
  int fd = vfs_->open(config_file, O_WRONLY | O_TRUNC);
  if (fd < 0) {
    return false;
  }

  ssize_t written = vfs_->write(fd, content.data(), content.size());
  vfs_->close(fd);

  return written == static_cast<ssize_t>(content.size());
}

// ===== Paper Metadata =====

bool AssignmentService::load_paper_meta(const std::string &paper_id,
                                        PaperMeta &meta) {
  std::string meta_file = "/papers/" + paper_id + "/metadata.txt";
  if (!vfs_->exists(meta_file)) {
    return false;
  }

  int fd = vfs_->open(meta_file, O_RDONLY);
  if (fd < 0) {
    return false;
  }

  char buffer[4096];
  ssize_t n = vfs_->read(fd, buffer, sizeof(buffer) - 1);
  vfs_->close(fd);

  if (n <= 0) {
    return false;
  }

  buffer[n] = '\0';
  std::istringstream iss(buffer);
  std::string line;

  meta.paper_id = paper_id;
  meta.fields.clear();
  meta.keywords.clear();
  meta.conflict_usernames.clear();

  while (std::getline(iss, line)) {
    size_t eq = line.find('=');
    if (eq != std::string::npos) {
      std::string key = trim(line.substr(0, eq));
      std::string value = trim(line.substr(eq + 1));

      if (key == "author") {
        meta.author = value;
      } else if (key == "title") {
        meta.title = value;
      } else if (key == "status") {
        meta.status = value;
      } else if (key == "fields") {
        meta.fields = split(value, ',');
      } else if (key == "keywords") {
        meta.keywords = split(value, ',');
      } else if (key == "conflict_usernames") {
        meta.conflict_usernames = split(value, ',');
      }
    }
  }

  return true;
}

bool AssignmentService::save_paper_meta(const PaperMeta &meta) {
  std::string paper_dir = "/papers/" + meta.paper_id;
  if (!vfs_->exists(paper_dir)) {
    return false;
  }

  std::string meta_file = paper_dir + "/metadata.txt";
  
  std::ostringstream oss;
  oss << "author=" << meta.author << "\n";
  oss << "title=" << meta.title << "\n";
  oss << "status=" << meta.status << "\n";
  oss << "fields=" << join(meta.fields, ",") << "\n";
  oss << "keywords=" << join(meta.keywords, ",") << "\n";
  oss << "conflict_usernames=" << join(meta.conflict_usernames, ",") << "\n";

  std::string content = oss.str();

  if (!vfs_->exists(meta_file)) {
    vfs_->create_file(meta_file);
  }

  int fd = vfs_->open(meta_file, O_WRONLY | O_TRUNC);
  if (fd < 0) {
    return false;
  }

  ssize_t written = vfs_->write(fd, content.data(), content.size());
  vfs_->close(fd);

  return written == static_cast<ssize_t>(content.size());
}

// ===== Reviewer Profile =====

bool AssignmentService::load_reviewer_profile(const std::string &username,
                                              ReviewerProfile &profile) {
  std::string profile_file = "/users/" + username + "/profile.txt";
  
  profile.username = username;
  profile.fields.clear();
  profile.keywords.clear();
  profile.affiliation.clear();
  profile.coauthors.clear();

  if (!vfs_->exists(profile_file)) {
    // Return empty profile (will have low relevance)
    return true;
  }

  int fd = vfs_->open(profile_file, O_RDONLY);
  if (fd < 0) {
    return false;
  }

  char buffer[4096];
  ssize_t n = vfs_->read(fd, buffer, sizeof(buffer) - 1);
  vfs_->close(fd);

  if (n <= 0) {
    return true; // Empty profile is OK
  }

  buffer[n] = '\0';
  std::istringstream iss(buffer);
  std::string line;

  while (std::getline(iss, line)) {
    size_t eq = line.find('=');
    if (eq != std::string::npos) {
      std::string key = trim(line.substr(0, eq));
      std::string value = trim(line.substr(eq + 1));

      if (key == "fields") {
        profile.fields = split(value, ',');
      } else if (key == "keywords") {
        profile.keywords = split(value, ',');
      } else if (key == "affiliation") {
        profile.affiliation = value;
      } else if (key == "coauthors") {
        profile.coauthors = split(value, ',');
      }
    }
  }

  return true;
}

bool AssignmentService::save_reviewer_profile(const ReviewerProfile &profile) {
  std::string user_dir = "/users/" + profile.username;
  if (!vfs_->exists(user_dir)) {
    vfs_->mkdir(user_dir);
  }

  std::string profile_file = user_dir + "/profile.txt";

  std::ostringstream oss;
  oss << "fields=" << join(profile.fields, ",") << "\n";
  oss << "keywords=" << join(profile.keywords, ",") << "\n";
  oss << "affiliation=" << profile.affiliation << "\n";
  oss << "coauthors=" << join(profile.coauthors, ",") << "\n";

  std::string content = oss.str();

  if (!vfs_->exists(profile_file)) {
    vfs_->create_file(profile_file);
  }

  int fd = vfs_->open(profile_file, O_WRONLY | O_TRUNC);
  if (fd < 0) {
    return false;
  }

  ssize_t written = vfs_->write(fd, content.data(), content.size());
  vfs_->close(fd);

  return written == static_cast<ssize_t>(content.size());
}

// ===== Assignment Management =====

bool AssignmentService::load_assignments(const std::string &paper_id,
                                         std::vector<Assignment> &assignments) {
  std::string assign_file = "/papers/" + paper_id + "/assignments.txt";
  assignments.clear();

  if (!vfs_->exists(assign_file)) {
    return true; // No assignments yet
  }

  int fd = vfs_->open(assign_file, O_RDONLY);
  if (fd < 0) {
    return false;
  }

  char buffer[4096];
  ssize_t n = vfs_->read(fd, buffer, sizeof(buffer) - 1);
  vfs_->close(fd);

  if (n <= 0) {
    return true;
  }

  buffer[n] = '\0';
  std::istringstream iss(buffer);
  std::string line;

  while (std::getline(iss, line)) {
    if (line.empty())
      continue;

    // Format: reviewer,timestamp,state
    auto parts = split(line, ',');
    if (parts.size() >= 3) {
      Assignment assign;
      assign.paper_id = paper_id;
      assign.reviewer = trim(parts[0]);
      assign.assigned_at = std::stoll(trim(parts[1]));
      assign.state = trim(parts[2]);
      assignments.push_back(assign);
    }
  }

  return true;
}

bool AssignmentService::save_assignments(
    const std::string &paper_id, const std::vector<Assignment> &assignments) {
  std::string paper_dir = "/papers/" + paper_id;
  if (!vfs_->exists(paper_dir)) {
    return false;
  }

  std::string assign_file = paper_dir + "/assignments.txt";

  std::ostringstream oss;
  for (const auto &assign : assignments) {
    oss << assign.reviewer << "," << assign.assigned_at << "," << assign.state
        << "\n";
  }

  std::string content = oss.str();

  if (!vfs_->exists(assign_file)) {
    vfs_->create_file(assign_file);
  }

  int fd = vfs_->open(assign_file, O_WRONLY | O_TRUNC);
  if (fd < 0) {
    return false;
  }

  ssize_t written = vfs_->write(fd, content.data(), content.size());
  vfs_->close(fd);

  return written == static_cast<ssize_t>(content.size());
}

int AssignmentService::get_active_load(const std::string &reviewer) {
  int load = 0;

  // Scan all papers
  std::vector<vfs::DirEntry> papers;
  if (vfs_->readdir("/papers", papers) != 0) {
    return 0;
  }

  for (const auto &entry : papers) {
    std::string paper_id(entry.name, entry.name_len);
    std::vector<Assignment> assignments;
    
    if (load_assignments(paper_id, assignments)) {
      for (const auto &assign : assignments) {
        if (assign.reviewer == reviewer && assign.state == "pending") {
          load++;
        }
      }
    }
  }

  return load;
}

// ===== COI Detection =====

AssignmentService::COIResult
AssignmentService::check_coi(const PaperMeta &paper,
                              const ReviewerProfile &reviewer) {
  COIResult result;
  result.has_conflict = false;

  // Rule 1: Reviewer == Author
  if (reviewer.username == paper.author) {
    result.has_conflict = true;
    result.reason = "Reviewer is the paper author";
    return result;
  }

  // Rule 2: Author-specified blacklist
  for (const auto &blocked : paper.conflict_usernames) {
    if (trim(blocked) == reviewer.username) {
      result.has_conflict = true;
      result.reason = "Reviewer in author's conflict list";
      return result;
    }
  }

  // Rule 3: Same affiliation (if both provided)
  if (!paper.author.empty() && !reviewer.affiliation.empty()) {
    // Load author's profile to get affiliation
    ReviewerProfile author_profile;
    if (load_reviewer_profile(paper.author, author_profile)) {
      if (!author_profile.affiliation.empty() &&
          to_lower(author_profile.affiliation) ==
              to_lower(reviewer.affiliation)) {
        result.has_conflict = true;
        result.reason = "Same affiliation as author";
        return result;
      }
    }
  }

  // Rule 4: Co-author relationship (future extension)
  // Check if paper.author is in reviewer.coauthors
  for (const auto &coauthor : reviewer.coauthors) {
    if (trim(coauthor) == paper.author) {
      result.has_conflict = true;
      result.reason = "Co-author relationship with paper author";
      return result;
    }
  }

  return result;
}

// ===== Relevance Scoring =====

double AssignmentService::compute_relevance(const PaperMeta &paper,
                                            const ReviewerProfile &reviewer) {
  // Normalize and tokenize fields/keywords
  std::set<std::string> paper_fields, reviewer_fields;
  std::set<std::string> paper_kw, reviewer_kw;

  for (const auto &f : paper.fields) {
    paper_fields.insert(to_lower(trim(f)));
  }
  for (const auto &f : reviewer.fields) {
    reviewer_fields.insert(to_lower(trim(f)));
  }
  for (const auto &k : paper.keywords) {
    paper_kw.insert(to_lower(trim(k)));
  }
  for (const auto &k : reviewer.keywords) {
    reviewer_kw.insert(to_lower(trim(k)));
  }

  // Compute intersection
  std::vector<std::string> field_intersection, kw_intersection;
  std::set_intersection(paper_fields.begin(), paper_fields.end(),
                        reviewer_fields.begin(), reviewer_fields.end(),
                        std::back_inserter(field_intersection));
  std::set_intersection(paper_kw.begin(), paper_kw.end(), reviewer_kw.begin(),
                        reviewer_kw.end(),
                        std::back_inserter(kw_intersection));

  // Scoring: 2.0 * |Field_intersect| + 1.0 * |Keyword_intersect|
  double relevance =
      2.0 * field_intersection.size() + 1.0 * kw_intersection.size();

  return relevance;
}

// ===== Auto Recommendation =====

std::vector<RecommendationResult>
AssignmentService::recommend_reviewers(const std::string &paper_id, int top_k) {
  std::vector<RecommendationResult> results;

  // Load paper metadata
  PaperMeta paper;
  if (!load_paper_meta(paper_id, paper)) {
    return results;
  }

  // Get all reviewers
  auto all_users = auth_manager_->list_users();

  for (const auto &user : all_users) {
    // Only consider reviewers
    if (user.role != protocol::Role::REVIEWER &&
        user.role != protocol::Role::ADMIN) {
      continue;
    }

    RecommendationResult rec;
    rec.reviewer = user.username;

    // Load reviewer profile
    ReviewerProfile profile;
    if (!load_reviewer_profile(user.username, profile)) {
      continue;
    }

    // Check COI
    auto coi = check_coi(paper, profile);
    rec.coi_blocked = coi.has_conflict;
    rec.coi_reason = coi.reason;

    // Compute relevance
    rec.relevance_score = compute_relevance(paper, profile);

    // Get active load
    rec.active_load = get_active_load(user.username);

    // Check max_active threshold
    if (rec.active_load >= config_.max_active) {
      rec.coi_blocked = true;
      rec.coi_reason = "Exceeds max active assignments (" +
                       std::to_string(config_.max_active) + ")";
    }

    // Compute final score (load penalty)
    rec.final_score = rec.relevance_score - config_.lambda * rec.active_load;

    results.push_back(rec);
  }

  // Sort by final_score descending
  std::sort(results.begin(), results.end(),
            [](const RecommendationResult &a, const RecommendationResult &b) {
              return a.final_score > b.final_score;
            });

  // Return top K
  if (static_cast<int>(results.size()) > top_k) {
    results.resize(top_k);
  }

  return results;
}

// ===== Auto Assignment =====

AssignmentService::AssignResult
AssignmentService::auto_assign(const std::string &paper_id, int num_reviewers) {
  AssignResult result;
  result.success = false;

  // Get recommendations
  auto recommendations = recommend_reviewers(paper_id, num_reviewers * 2);

  // Filter out COI-blocked reviewers
  std::vector<RecommendationResult> valid_candidates;
  for (const auto &rec : recommendations) {
    if (!rec.coi_blocked) {
      valid_candidates.push_back(rec);
    }
  }

  if (static_cast<int>(valid_candidates.size()) < num_reviewers) {
    result.message = "Not enough valid reviewers (need " +
                     std::to_string(num_reviewers) + ", found " +
                     std::to_string(valid_candidates.size()) + ")";
    return result;
  }

  // Load existing assignments
  std::vector<Assignment> assignments;
  load_assignments(paper_id, assignments);

  // Add new assignments
  time_t now = std::time(nullptr);
  for (int i = 0; i < num_reviewers && i < static_cast<int>(valid_candidates.size()); ++i) {
    Assignment assign;
    assign.paper_id = paper_id;
    assign.reviewer = valid_candidates[i].reviewer;
    assign.assigned_at = now;
    assign.state = "pending";
    assignments.push_back(assign);
    result.assigned_reviewers.push_back(assign.reviewer);
  }

  // Save assignments
  if (!save_assignments(paper_id, assignments)) {
    result.message = "Failed to save assignments";
    return result;
  }

  // Update paper status to UNDER_REVIEW
  PaperMeta paper;
  if (load_paper_meta(paper_id, paper)) {
    paper.status = "UNDER_REVIEW";
    save_paper_meta(paper);
  }

  result.success = true;
  result.message = "Successfully assigned " +
                   std::to_string(result.assigned_reviewers.size()) +
                   " reviewers";
  return result;
}

// ===== Helper Functions =====

std::set<std::string> AssignmentService::tokenize(const std::string &text) {
  std::set<std::string> tokens;
  std::istringstream iss(text);
  std::string word;
  while (iss >> word) {
    tokens.insert(to_lower(word));
  }
  return tokens;
}

std::vector<std::string> AssignmentService::split(const std::string &str,
                                                  char delim) {
  std::vector<std::string> result;
  std::istringstream iss(str);
  std::string token;
  while (std::getline(iss, token, delim)) {
    std::string trimmed = trim(token);
    if (!trimmed.empty()) {
      result.push_back(trimmed);
    }
  }
  return result;
}

std::string
AssignmentService::join(const std::vector<std::string> &vec,
                        const std::string &delim) {
  std::ostringstream oss;
  for (size_t i = 0; i < vec.size(); ++i) {
    if (i > 0)
      oss << delim;
    oss << vec[i];
  }
  return oss.str();
}

std::string AssignmentService::trim(const std::string &str) {
  size_t start = 0;
  size_t end = str.length();

  while (start < end && std::isspace(str[start])) {
    start++;
  }
  while (end > start && std::isspace(str[end - 1])) {
    end--;
  }

  return str.substr(start, end - start);
}

std::string AssignmentService::to_lower(const std::string &str) {
  std::string result = str;
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return result;
}

} // namespace server

