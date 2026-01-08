#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

namespace protocol {

// Command types
enum class Command {
  // Authentication
  LOGIN,
  LOGOUT,

  // Author commands
  UPLOAD_PAPER,
  SUBMIT_REVISION,
  VIEW_PAPER_STATUS,
  DOWNLOAD_REVIEWS,
  LIST_MY_PAPERS,

  // Reviewer commands
  VIEW_ASSIGNED_PAPERS,
  DOWNLOAD_PAPER,
  SUBMIT_REVIEW,
  SAVE_REVIEW_DRAFT,
  GET_REVIEW_DRAFT,
  VIEW_REVIEW_STATUS,
  LIST_ASSIGNED_PAPERS,

  // Editor commands
  VIEW_PENDING_PAPERS,
  ASSIGN_REVIEWER,
  VIEW_REVIEW_PROGRESS,
  MAKE_DECISION,
  LIST_ALL_PAPERS,

  // Admin commands
  CREATE_USER,
  DELETE_USER,
  LIST_USERS,
  CREATE_BACKUP,
  RESTORE_BACKUP,
  LIST_BACKUPS,
  SYSTEM_STATUS,

  // Assignment & Profile commands
  SET_REVIEWER_PROFILE,
  GET_REVIEWER_PROFILE,
  GET_REVIEWER_RECOMMENDATIONS,
  AUTO_ASSIGN_REVIEWERS,

  UNKNOWN
};

// Status codes
enum class StatusCode {
  OK = 200,
  CREATED = 201,
  BAD_REQUEST = 400,
  UNAUTHORIZED = 401,
  FORBIDDEN = 403,
  NOT_FOUND = 404,
  CONFLICT = 409,
  INTERNAL_ERROR = 500
};

// User roles
enum class Role { AUTHOR, REVIEWER, EDITOR, ADMIN, UNKNOWN };

// Paper decision
enum class Decision { ACCEPT, REJECT, MAJOR_REVISION, MINOR_REVISION, PENDING };

// Paper Lifecycle State
enum class LifecycleState {
  SUBMITTED,
  UNDER_REVIEW,
  REBUTTAL,
  DECISION_PENDING,
  ACCEPTED,
  REJECTED
};

// Review round
enum class ReviewRound { ROUND1, ROUND2, REBUTTAL };

// Blind strategy
enum class BlindPolicy { SINGLE_BLIND, DOUBLE_BLIND };

// Message structure
struct Message {
  Command command;
  std::map<std::string, std::string> params;
  std::vector<char> body;

  Message() : command(Command::UNKNOWN) {}
};

// Response structure
struct Response {
  StatusCode status;
  std::string message;
  std::vector<char> body;

  Response() : status(StatusCode::OK) {}
  Response(StatusCode s, const std::string &msg) : status(s), message(msg) {}
};

// Serialization/Deserialization
class Protocol {
public:
  // Serialize message to bytes
  static std::vector<char> serialize_message(const Message &msg);

  // Deserialize bytes to message
  static bool deserialize_message(const std::vector<char> &data, Message &msg);

  // Serialize response to bytes
  static std::vector<char> serialize_response(const Response &resp);

  // Deserialize bytes to response
  static bool deserialize_response(const std::vector<char> &data,
                                   Response &resp);

  // Helper functions
  static std::string command_to_string(Command cmd);
  static Command string_to_command(const std::string &str);
  static std::string status_to_string(StatusCode status);
  static std::string role_to_string(Role role);
  static Role string_to_role(const std::string &str);
  static std::string decision_to_string(Decision decision);
  static Decision string_to_decision(const std::string &str);

  static std::string state_to_string(LifecycleState state);
  static LifecycleState string_to_state(const std::string &str);

  static std::string round_to_string(ReviewRound round);
  static ReviewRound string_to_round(const std::string &str);

  static std::string blind_to_string(BlindPolicy blind);
  static BlindPolicy string_to_blind(const std::string &str);
};

// Command string mapping
inline std::string Protocol::command_to_string(Command cmd) {
  switch (cmd) {
  case Command::LOGIN:
    return "LOGIN";
  case Command::LOGOUT:
    return "LOGOUT";
  case Command::UPLOAD_PAPER:
    return "UPLOAD_PAPER";
  case Command::SUBMIT_REVISION:
    return "SUBMIT_REVISION";
  case Command::VIEW_PAPER_STATUS:
    return "VIEW_PAPER_STATUS";
  case Command::DOWNLOAD_REVIEWS:
    return "DOWNLOAD_REVIEWS";
  case Command::LIST_MY_PAPERS:
    return "LIST_MY_PAPERS";
  case Command::VIEW_ASSIGNED_PAPERS:
    return "VIEW_ASSIGNED_PAPERS";
  case Command::DOWNLOAD_PAPER:
    return "DOWNLOAD_PAPER";
  case Command::SUBMIT_REVIEW:
    return "SUBMIT_REVIEW";
  case Command::SAVE_REVIEW_DRAFT:
    return "SAVE_REVIEW_DRAFT";
  case Command::GET_REVIEW_DRAFT:
    return "GET_REVIEW_DRAFT";
  case Command::VIEW_REVIEW_STATUS:
    return "VIEW_REVIEW_STATUS";
  case Command::LIST_ASSIGNED_PAPERS:
    return "LIST_ASSIGNED_PAPERS";
  case Command::VIEW_PENDING_PAPERS:
    return "VIEW_PENDING_PAPERS";
  case Command::ASSIGN_REVIEWER:
    return "ASSIGN_REVIEWER";
  case Command::VIEW_REVIEW_PROGRESS:
    return "VIEW_REVIEW_PROGRESS";
  case Command::MAKE_DECISION:
    return "MAKE_DECISION";
  case Command::LIST_ALL_PAPERS:
    return "LIST_ALL_PAPERS";
  case Command::CREATE_USER:
    return "CREATE_USER";
  case Command::DELETE_USER:
    return "DELETE_USER";
  case Command::LIST_USERS:
    return "LIST_USERS";
  case Command::CREATE_BACKUP:
    return "CREATE_BACKUP";
  case Command::RESTORE_BACKUP:
    return "RESTORE_BACKUP";
  case Command::LIST_BACKUPS:
    return "LIST_BACKUPS";
  case Command::SYSTEM_STATUS:
    return "SYSTEM_STATUS";
  case Command::SET_REVIEWER_PROFILE:
    return "SET_REVIEWER_PROFILE";
  case Command::GET_REVIEWER_PROFILE:
    return "GET_REVIEWER_PROFILE";
  case Command::GET_REVIEWER_RECOMMENDATIONS:
    return "GET_REVIEWER_RECOMMENDATIONS";
  case Command::AUTO_ASSIGN_REVIEWERS:
    return "AUTO_ASSIGN_REVIEWERS";
  default:
    return "UNKNOWN";
  }
}

inline Command Protocol::string_to_command(const std::string &str) {
  static std::map<std::string, Command> cmd_map = {
      {"LOGIN", Command::LOGIN},
      {"LOGOUT", Command::LOGOUT},
      {"UPLOAD_PAPER", Command::UPLOAD_PAPER},
      {"SUBMIT_REVISION", Command::SUBMIT_REVISION},
      {"VIEW_PAPER_STATUS", Command::VIEW_PAPER_STATUS},
      {"DOWNLOAD_REVIEWS", Command::DOWNLOAD_REVIEWS},
      {"LIST_MY_PAPERS", Command::LIST_MY_PAPERS},
      {"VIEW_ASSIGNED_PAPERS", Command::VIEW_ASSIGNED_PAPERS},
      {"DOWNLOAD_PAPER", Command::DOWNLOAD_PAPER},
      {"SUBMIT_REVIEW", Command::SUBMIT_REVIEW},
      {"SAVE_REVIEW_DRAFT", Command::SAVE_REVIEW_DRAFT},
      {"GET_REVIEW_DRAFT", Command::GET_REVIEW_DRAFT},
      {"VIEW_REVIEW_STATUS", Command::VIEW_REVIEW_STATUS},
      {"LIST_ASSIGNED_PAPERS", Command::LIST_ASSIGNED_PAPERS},
      {"VIEW_PENDING_PAPERS", Command::VIEW_PENDING_PAPERS},
      {"ASSIGN_REVIEWER", Command::ASSIGN_REVIEWER},
      {"VIEW_REVIEW_PROGRESS", Command::VIEW_REVIEW_PROGRESS},
      {"MAKE_DECISION", Command::MAKE_DECISION},
      {"LIST_ALL_PAPERS", Command::LIST_ALL_PAPERS},
      {"CREATE_USER", Command::CREATE_USER},
      {"DELETE_USER", Command::DELETE_USER},
      {"LIST_USERS", Command::LIST_USERS},
      {"CREATE_BACKUP", Command::CREATE_BACKUP},
      {"RESTORE_BACKUP", Command::RESTORE_BACKUP},
      {"LIST_BACKUPS", Command::LIST_BACKUPS},
      {"SYSTEM_STATUS", Command::SYSTEM_STATUS},
      {"SET_REVIEWER_PROFILE", Command::SET_REVIEWER_PROFILE},
      {"GET_REVIEWER_PROFILE", Command::GET_REVIEWER_PROFILE},
      {"GET_REVIEWER_RECOMMENDATIONS", Command::GET_REVIEWER_RECOMMENDATIONS},
      {"AUTO_ASSIGN_REVIEWERS", Command::AUTO_ASSIGN_REVIEWERS}};

  auto it = cmd_map.find(str);
  return it != cmd_map.end() ? it->second : Command::UNKNOWN;
}

inline std::string Protocol::status_to_string(StatusCode status) {
  switch (status) {
  case StatusCode::OK:
    return "200 OK";
  case StatusCode::CREATED:
    return "201 Created";
  case StatusCode::BAD_REQUEST:
    return "400 Bad Request";
  case StatusCode::UNAUTHORIZED:
    return "401 Unauthorized";
  case StatusCode::FORBIDDEN:
    return "403 Forbidden";
  case StatusCode::NOT_FOUND:
    return "404 Not Found";
  case StatusCode::CONFLICT:
    return "409 Conflict";
  case StatusCode::INTERNAL_ERROR:
    return "500 Internal Server Error";
  default:
    return "500 Internal Server Error";
  }
}

inline std::string Protocol::role_to_string(Role role) {
  switch (role) {
  case Role::AUTHOR:
    return "author";
  case Role::REVIEWER:
    return "reviewer";
  case Role::EDITOR:
    return "editor";
  case Role::ADMIN:
    return "admin";
  default:
    return "unknown";
  }
}

inline Role Protocol::string_to_role(const std::string &str) {
  if (str == "author")
    return Role::AUTHOR;
  if (str == "reviewer")
    return Role::REVIEWER;
  if (str == "editor")
    return Role::EDITOR;
  if (str == "admin")
    return Role::ADMIN;
  return Role::UNKNOWN;
}

inline std::string Protocol::decision_to_string(Decision decision) {
  switch (decision) {
  case Decision::ACCEPT:
    return "accept";
  case Decision::REJECT:
    return "reject";
  case Decision::MAJOR_REVISION:
    return "major_revision";
  case Decision::MINOR_REVISION:
    return "minor_revision";
  case Decision::PENDING:
    return "pending";
  default:
    return "pending";
  }
}

inline Decision Protocol::string_to_decision(const std::string &str) {
  // 支持数字输入
  if (str == "1" || str == "accept")
    return Decision::ACCEPT;
  if (str == "2" || str == "reject")
    return Decision::REJECT;
  if (str == "3" || str == "major_revision")
    return Decision::MAJOR_REVISION;
  if (str == "4" || str == "minor_revision")
    return Decision::MINOR_REVISION;
  return Decision::PENDING;
}

inline std::string Protocol::state_to_string(LifecycleState state) {
  switch (state) {
  case LifecycleState::SUBMITTED:
    return "SUBMITTED";
  case LifecycleState::UNDER_REVIEW:
    return "UNDER_REVIEW";
  case LifecycleState::REBUTTAL:
    return "REBUTTAL";
  case LifecycleState::DECISION_PENDING:
    return "DECISION_PENDING";
  case LifecycleState::ACCEPTED:
    return "ACCEPTED";
  case LifecycleState::REJECTED:
    return "REJECTED";
  default:
    return "UNKNOWN";
  }
}

inline LifecycleState Protocol::string_to_state(const std::string &str) {
  if (str == "SUBMITTED")
    return LifecycleState::SUBMITTED;
  if (str == "UNDER_REVIEW")
    return LifecycleState::UNDER_REVIEW;
  if (str == "REBUTTAL")
    return LifecycleState::REBUTTAL;
  if (str == "DECISION_PENDING")
    return LifecycleState::DECISION_PENDING;
  if (str == "ACCEPTED")
    return LifecycleState::ACCEPTED;
  if (str == "REJECTED")
    return LifecycleState::REJECTED;
  return LifecycleState::SUBMITTED;
}

inline std::string Protocol::round_to_string(ReviewRound round) {
  switch (round) {
  case ReviewRound::ROUND1:
    return "R1";
  case ReviewRound::ROUND2:
    return "R2";
  case ReviewRound::REBUTTAL:
    return "REBUTTAL";
  default:
    return "R1";
  }
}

inline ReviewRound Protocol::string_to_round(const std::string &str) {
  if (str == "R1")
    return ReviewRound::ROUND1;
  if (str == "R2")
    return ReviewRound::ROUND2;
  if (str == "REBUTTAL")
    return ReviewRound::REBUTTAL;
  return ReviewRound::ROUND1;
}

inline std::string Protocol::blind_to_string(BlindPolicy blind) {
  switch (blind) {
  case BlindPolicy::DOUBLE_BLIND:
    return "double";
  case BlindPolicy::SINGLE_BLIND:
  default:
    return "single";
  }
}

inline BlindPolicy Protocol::string_to_blind(const std::string &str) {
  if (str == "double")
    return BlindPolicy::DOUBLE_BLIND;
  return BlindPolicy::SINGLE_BLIND;
}

} // namespace protocol

#endif // PROTOCOL_H
