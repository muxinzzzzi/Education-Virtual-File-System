#include "common/protocol.h"
#include <cstring>
#include <sstream>
#include <algorithm>

namespace protocol {

std::vector<char> Protocol::serialize_message(const Message &msg) {
  std::ostringstream oss;

  // Command line
  oss << command_to_string(msg.command);
  for (const auto &param : msg.params) {
    oss << " " << param.first << "=" << param.second;
  }
  oss << "\n";

  // Body length
  oss << msg.body.size() << "\n";

  // Convert to vector
  std::string header = oss.str();
  std::vector<char> result(header.begin(), header.end());

  // Append body
  result.insert(result.end(), msg.body.begin(), msg.body.end());

  return result;
}

bool Protocol::deserialize_message(const std::vector<char> &data,
                                   Message &msg) {
  if (data.empty()) {
    return false;
  }

  // Find first newline (end of command line)
  auto first_newline = std::find(data.begin(), data.end(), '\n');
  if (first_newline == data.end()) {
    return false;
  }

  // Parse command line
  std::string command_line(data.begin(), first_newline);
  std::istringstream iss(command_line);

  std::string cmd_str;
  iss >> cmd_str;
  msg.command = string_to_command(cmd_str);

  // Parse parameters
  std::string param;
  while (iss >> param) {
    size_t eq_pos = param.find('=');
    if (eq_pos != std::string::npos) {
      std::string key = param.substr(0, eq_pos);
      std::string value = param.substr(eq_pos + 1);
      msg.params[key] = value;
    }
  }

  // Find second newline (end of body length)
  auto second_newline = std::find(first_newline + 1, data.end(), '\n');
  if (second_newline == data.end()) {
    return false;
  }

  // Parse body length
  std::string length_str(first_newline + 1, second_newline);
  size_t body_length = std::stoull(length_str);

  // Extract body
  auto body_start = second_newline + 1;
  if (std::distance(body_start, data.end()) < static_cast<long>(body_length)) {
    return false;
  }

  msg.body.assign(body_start, body_start + body_length);

  return true;
}

std::vector<char> Protocol::serialize_response(const Response &resp) {
  std::ostringstream oss;

  // Status line
  oss << status_to_string(resp.status) << "\n";

  // Body length
  oss << resp.body.size() << "\n";

  // Convert to vector
  std::string header = oss.str();
  std::vector<char> result(header.begin(), header.end());

  // Append body
  result.insert(result.end(), resp.body.begin(), resp.body.end());

  return result;
}

bool Protocol::deserialize_response(const std::vector<char> &data,
                                    Response &resp) {
  if (data.empty()) {
    return false;
  }

  // Find first newline
  auto first_newline = std::find(data.begin(), data.end(), '\n');
  if (first_newline == data.end()) {
    return false;
  }

  // Parse status line
  std::string status_line(data.begin(), first_newline);
  resp.message = status_line;

  // Extract status code
  size_t space_pos = status_line.find(' ');
  if (space_pos != std::string::npos) {
    int code = std::stoi(status_line.substr(0, space_pos));
    resp.status = static_cast<StatusCode>(code);
  }

  // Find second newline
  auto second_newline = std::find(first_newline + 1, data.end(), '\n');
  if (second_newline == data.end()) {
    return false;
  }

  // Parse body length
  std::string length_str(first_newline + 1, second_newline);
  size_t body_length = std::stoull(length_str);

  // Extract body
  auto body_start = second_newline + 1;
  if (std::distance(body_start, data.end()) < static_cast<long>(body_length)) {
    return false;
  }

  resp.body.assign(body_start, body_start + body_length);

  return true;
}

} // namespace protocol
