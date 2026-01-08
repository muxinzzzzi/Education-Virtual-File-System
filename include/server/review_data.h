#ifndef REVIEW_DATA_H
#define REVIEW_DATA_H

#include <string>
#include <sstream>
#include <vector>

namespace server {

// 结构化审稿意见
struct StructuredReview {
  std::string paper_id;
  std::string reviewer;
  std::string round;
  
  // 核心内容
  std::string summary;              // 总评
  std::string strengths;            // 优点
  std::string weaknesses;           // 缺点
  std::string questions;            // 问题/建议
  
  // 评分
  int rating;                       // 1-5 评分（1=strong reject, 5=strong accept）
  int confidence;                   // 1-5 置信度
  
  // 状态
  std::string status;               // "draft" 或 "submitted"
  int64_t last_modified;            // Unix timestamp
  int64_t submitted_at;             // Unix timestamp (0 if not submitted)
  
  StructuredReview() 
    : rating(0), confidence(0), last_modified(0), submitted_at(0) {}
  
  // 转换为JSON字符串
  std::string to_json() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"paper_id\": \"" << paper_id << "\",\n";
    oss << "  \"reviewer\": \"" << reviewer << "\",\n";
    oss << "  \"round\": \"" << round << "\",\n";
    oss << "  \"summary\": \"" << json_escape(summary) << "\",\n";
    oss << "  \"strengths\": \"" << json_escape(strengths) << "\",\n";
    oss << "  \"weaknesses\": \"" << json_escape(weaknesses) << "\",\n";
    oss << "  \"questions\": \"" << json_escape(questions) << "\",\n";
    oss << "  \"rating\": " << rating << ",\n";
    oss << "  \"confidence\": " << confidence << ",\n";
    oss << "  \"status\": \"" << status << "\",\n";
    oss << "  \"last_modified\": " << last_modified << ",\n";
    oss << "  \"submitted_at\": " << submitted_at << "\n";
    oss << "}";
    return oss.str();
  }
  
  // 从JSON字符串解析（简单实现）
  static StructuredReview from_json(const std::string &json) {
    StructuredReview review;
    
    review.paper_id = extract_json_string(json, "paper_id");
    review.reviewer = extract_json_string(json, "reviewer");
    review.round = extract_json_string(json, "round");
    review.summary = extract_json_string(json, "summary");
    review.strengths = extract_json_string(json, "strengths");
    review.weaknesses = extract_json_string(json, "weaknesses");
    review.questions = extract_json_string(json, "questions");
    review.rating = extract_json_int(json, "rating");
    review.confidence = extract_json_int(json, "confidence");
    review.status = extract_json_string(json, "status");
    review.last_modified = extract_json_int(json, "last_modified");
    review.submitted_at = extract_json_int(json, "submitted_at");
    
    return review;
  }
  
  // 转换为可读的文本格式（给编辑查看）
  std::string to_readable() const {
    std::ostringstream oss;
    oss << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    oss << "审稿人: " << reviewer << "\n";
    oss << "评分: " << rating_to_string(rating) << " | 置信度: " << confidence << "/5\n";
    oss << "状态: " << (status == "submitted" ? "✓ 已提交" : "⚠ 草稿") << "\n";
    oss << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n";
    
    oss << "【总评】\n" << (summary.empty() ? "(未填写)" : summary) << "\n\n";
    oss << "【优点】\n" << (strengths.empty() ? "(未填写)" : strengths) << "\n\n";
    oss << "【缺点】\n" << (weaknesses.empty() ? "(未填写)" : weaknesses) << "\n\n";
    oss << "【问题/建议】\n" << (questions.empty() ? "(未填写)" : questions) << "\n\n";
    
    return oss.str();
  }
  
private:
  static std::string json_escape(const std::string &str) {
    std::string result;
    for (char c : str) {
      if (c == '"') result += "\\\"";
      else if (c == '\\') result += "\\\\";
      else if (c == '\n') result += "\\n";
      else if (c == '\r') result += "\\r";
      else if (c == '\t') result += "\\t";
      else result += c;
    }
    return result;
  }
  
  static std::string extract_json_string(const std::string &json, const std::string &key) {
    std::string search = "\"" + key + "\": \"";
    size_t start = json.find(search);
    if (start == std::string::npos) return "";
    start += search.length();
    
    size_t end = start;
    while (end < json.length()) {
      if (json[end] == '"' && (end == 0 || json[end-1] != '\\')) break;
      end++;
    }
    
    std::string result = json.substr(start, end - start);
    // 简单的反转义
    size_t pos = 0;
    while ((pos = result.find("\\n", pos)) != std::string::npos) {
      result.replace(pos, 2, "\n");
      pos++;
    }
    while ((pos = result.find("\\\"", pos)) != std::string::npos) {
      result.replace(pos, 2, "\"");
      pos++;
    }
    return result;
  }
  
  static int extract_json_int(const std::string &json, const std::string &key) {
    std::string search = "\"" + key + "\": ";
    size_t start = json.find(search);
    if (start == std::string::npos) return 0;
    start += search.length();
    
    size_t end = start;
    while (end < json.length() && (isdigit(json[end]) || json[end] == '-')) {
      end++;
    }
    
    std::string num_str = json.substr(start, end - start);
    try {
      return std::stoi(num_str);
    } catch (...) {
      return 0;
    }
  }
  
  static std::string rating_to_string(int rating) {
    switch (rating) {
      case 1: return "1 - Strong Reject";
      case 2: return "2 - Weak Reject";
      case 3: return "3 - Borderline";
      case 4: return "4 - Weak Accept";
      case 5: return "5 - Strong Accept";
      default: return "0 - Not Rated";
    }
  }
};

} // namespace server

#endif // REVIEW_DATA_H

