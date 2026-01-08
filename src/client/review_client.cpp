#include "client/review_client.h"
#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <vector>

namespace client {

// ===== UI Enhancement Utilities =====

namespace Colors {
    const std::string RESET = "\033[0m";
    const std::string BOLD = "\033[1m";
    const std::string DIM = "\033[2m";
    const std::string RED = "\033[31m";
    const std::string GREEN = "\033[32m";
    const std::string YELLOW = "\033[33m";
    const std::string BLUE = "\033[34m";
    const std::string MAGENTA = "\033[35m";
    const std::string CYAN = "\033[36m";
    const std::string WHITE = "\033[37m";
    const std::string BG_BLUE = "\033[44m";
    const std::string BG_GREEN = "\033[42m";
}

namespace Icons {
    const std::string SUCCESS = "✓";
    const std::string ERROR = "✗";
    const std::string INFO = "ℹ";
    const std::string WARNING = "⚠";
    const std::string ARROW = "➜";
    const std::string PAPER = "📄";
    const std::string USER = "👤";
    const std::string UPLOAD = "⬆";
    const std::string DOWNLOAD = "⬇";
    const std::string EDIT = "✏";
    const std::string VIEW = "👁";
    const std::string ASSIGN = "📌";
    const std::string DECISION = "⚖";
    const std::string BACK = "↩";
}

// UI Helper Functions
class UIHelper {
public:
    static void clear_screen() {
        std::cout << "\033[2J\033[1;1H";
    }
    
    static void print_header(const std::string& title) {
        std::cout << "\n" << Colors::CYAN << Colors::BOLD;
        std::cout << "╔════════════════════════════════════════════════════════════╗\n";
        std::string padded = "  " + title;
        padded.resize(58, ' ');
        std::cout << "║" << padded << "║\n";
        std::cout << "╚════════════════════════════════════════════════════════════╝";
        std::cout << Colors::RESET << "\n";
    }
    
    static void print_section(const std::string& title) {
        std::cout << "\n" << Colors::YELLOW << Colors::BOLD 
                  << "▸ " << title << Colors::RESET << "\n";
    }
    
    static void print_success(const std::string& msg) {
        std::cout << Colors::GREEN << Icons::SUCCESS << " " 
                  << msg << Colors::RESET << "\n";
    }
    
    static void print_error(const std::string& msg) {
        std::cout << Colors::RED << Icons::ERROR << " " 
                  << msg << Colors::RESET << "\n";
    }
    
    static void print_info(const std::string& msg) {
        std::cout << Colors::BLUE << Icons::INFO << " " 
                  << msg << Colors::RESET << "\n";
    }
    
    static void print_warning(const std::string& msg) {
        std::cout << Colors::YELLOW << Icons::WARNING << " " 
                  << msg << Colors::RESET << "\n";
    }
    
    static void print_menu_item(int num, const std::string& icon, 
                               const std::string& text, bool highlight = false) {
        if (highlight) {
            std::cout << Colors::BG_BLUE << Colors::WHITE;
        }
        std::cout << "  " << Colors::BOLD << Colors::WHITE << "[" << num << "]" 
                  << Colors::RESET;
        if (highlight) std::cout << Colors::BG_BLUE;
        std::cout << " " << icon << "  " << text;
        if (highlight) std::cout << Colors::RESET;
        std::cout << "\n";
    }
    
    static void print_separator() {
        std::cout << Colors::DIM << "  ────────────────────────────────────────────────────" 
                  << Colors::RESET << "\n";
    }
    
    static std::string prompt(const std::string& text, const std::string& default_val = "") {
        std::cout << Colors::CYAN << Icons::ARROW << " " << Colors::RESET 
                  << Colors::BOLD << text << Colors::RESET;
        if (!default_val.empty()) {
            std::cout << Colors::DIM << " [" << default_val << "]" << Colors::RESET;
        }
        std::cout << ": ";
        return "";
    }
    
    static bool confirm(const std::string& message) {
        std::cout << Colors::YELLOW << Icons::WARNING << " " 
                  << message << " " << Colors::BOLD << "(y/n)" 
                  << Colors::RESET << ": ";
        std::string response;
        std::getline(std::cin, response);
        return response == "y" || response == "Y" || response == "yes";
    }
    
    static void press_enter_to_continue() {
        std::cout << "\n" << Colors::DIM << "按回车继续..." << Colors::RESET;
        std::cin.get();
    }
};

ReviewClient::ReviewClient()
    : socket_(-1), connected_(false), current_role_(protocol::Role::UNKNOWN) {
    context_ = std::make_shared<OperationContext>();
}

ReviewClient::~ReviewClient() { disconnect(); }

bool ReviewClient::connect(const std::string &host, int port) {
  std::cout << Colors::CYAN << "正在连接到服务器 " << host << ":" << port << "..." 
            << Colors::RESET << "\n";
  
  socket_ = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_ < 0) {
    UIHelper::print_error("创建socket失败");
    return false;
  }

  struct sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
    UIHelper::print_error("无效的服务器地址");
    close(socket_);
    return false;
  }

  if (::connect(socket_, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    UIHelper::print_error("连接服务器失败，请检查服务器是否启动");
    close(socket_);
    return false;
  }

  connected_ = true;
  UIHelper::print_success("成功连接到 " + host + ":" + std::to_string(port));

  return true;
}

void ReviewClient::disconnect() {
  if (connected_) {
    if (!session_id_.empty()) {
      logout();
    }
    close(socket_);
    connected_ = false;
  }
}

bool ReviewClient::login(const std::string &username,
                         const std::string &password) {
  std::cout << "\n" << Colors::CYAN << "正在登录..." << Colors::RESET << "\n";
  
  protocol::Message msg;
  msg.command = protocol::Command::LOGIN;
  msg.params["username"] = username;
  msg.params["password"] = password;

  if (!send_message(msg)) {
    UIHelper::print_error("发送登录请求失败");
    return false;
  }

  protocol::Response resp;
  if (!receive_response(resp)) {
    UIHelper::print_error("接收服务器响应失败");
    return false;
  }

  if (resp.status != protocol::StatusCode::OK) {
    UIHelper::print_error("登录失败: " + resp.message);
    return false;
  }

  // Parse response body
  std::string body_str(resp.body.begin(), resp.body.end());
  std::istringstream iss(body_str);
  std::string line;

  while (std::getline(iss, line)) {
    size_t eq_pos = line.find('=');
    if (eq_pos != std::string::npos) {
      std::string key = line.substr(0, eq_pos);
      std::string value = line.substr(eq_pos + 1);

      if (key == "session_id") {
        session_id_ = value;
      } else if (key == "role") {
        current_role_ = protocol::Protocol::string_to_role(value);
      }
    }
  }

  username_ = username;
  
  std::cout << "\n";
  UIHelper::print_success("登录成功！");
  
  std::string role_name;
  switch (current_role_) {
    case protocol::Role::AUTHOR:   role_name = "作者"; break;
    case protocol::Role::REVIEWER: role_name = "审稿人"; break;
    case protocol::Role::EDITOR:   role_name = "编辑"; break;
    case protocol::Role::ADMIN:    role_name = "管理员"; break;
    default: role_name = "未知"; break;
  }
  
  std::cout << Colors::CYAN << Icons::USER << " 用户: " << Colors::BOLD << username_ 
            << Colors::RESET << Colors::CYAN << " | 角色: " << Colors::BOLD << role_name 
            << Colors::RESET << "\n";

  return true;
}

void ReviewClient::logout() {
  protocol::Message msg;
  msg.command = protocol::Command::LOGOUT;

  send_message(msg);

  protocol::Response resp;
  receive_response(resp);

  session_id_.clear();
  current_role_ = protocol::Role::UNKNOWN;
  username_.clear();
}

void ReviewClient::run() {
  while (connected_ && !session_id_.empty()) {
    switch (current_role_) {
    case protocol::Role::AUTHOR:
      show_author_menu();
      break;
    case protocol::Role::REVIEWER:
      show_reviewer_menu();
      break;
    case protocol::Role::EDITOR:
      show_editor_menu();
      break;
    case protocol::Role::ADMIN:
      show_admin_menu();
      break;
    default:
      std::cout << "Unknown role\n";
      return;
    }
  }
}

void ReviewClient::show_author_menu() {
  UIHelper::clear_screen();
  UIHelper::print_header("作者面板 - " + username_);
  
  // Show recent context if available
  if (!context_->last_paper_id.empty()) {
    UIHelper::print_info("最近操作的论文: " + context_->last_paper_id);
  }
  
  UIHelper::print_section("论文管理");
  UIHelper::print_menu_item(1, Icons::UPLOAD, "上传新论文");
  UIHelper::print_menu_item(2, Icons::VIEW, "查看论文状态");
  UIHelper::print_menu_item(3, Icons::EDIT, "提交修订版");
  UIHelper::print_menu_item(4, Icons::DOWNLOAD, "下载审稿意见");
  
  UIHelper::print_separator();
  UIHelper::print_menu_item(5, Icons::BACK, "退出登录");
  
  std::cout << "\n";
  UIHelper::prompt("请选择操作");

  int choice;
  std::cin >> choice;
  std::cin.ignore();

  switch (choice) {
  case 1:
    std::cout << "\n";
    upload_paper();
    UIHelper::press_enter_to_continue();
    break;
  case 2:
    std::cout << "\n";
    view_paper_status();
    UIHelper::press_enter_to_continue();
    break;
  case 3:
    std::cout << "\n";
    submit_revision();
    UIHelper::press_enter_to_continue();
    break;
  case 4:
    std::cout << "\n";
    download_reviews();
    UIHelper::press_enter_to_continue();
    break;
  case 5:
    logout();
    break;
  default:
    UIHelper::print_error("无效的选择，请重试");
    UIHelper::press_enter_to_continue();
  }
}

void ReviewClient::show_reviewer_menu() {
  UIHelper::clear_screen();
  UIHelper::print_header("审稿人面板 - " + username_);
  
  if (!context_->last_paper_id.empty()) {
    UIHelper::print_info("最近操作的论文: " + context_->last_paper_id);
  }
  
  UIHelper::print_section("审稿任务");
  UIHelper::print_menu_item(1, Icons::DOWNLOAD, "下载待审论文");
  UIHelper::print_menu_item(2, Icons::UPLOAD, "提交审稿意见");
  UIHelper::print_menu_item(3, Icons::VIEW, "查看审稿状态");
  
  UIHelper::print_section("个人设置");
  UIHelper::print_menu_item(4, Icons::EDIT, "设置研究领域和关键词");
  UIHelper::print_menu_item(5, Icons::VIEW, "查看我的个人资料");
  
  UIHelper::print_separator();
  UIHelper::print_menu_item(6, Icons::BACK, "退出登录");
  
  std::cout << "\n";
  UIHelper::prompt("请选择操作");

  int choice;
  std::cin >> choice;
  std::cin.ignore();

  switch (choice) {
  case 1:
    std::cout << "\n";
    download_paper();
    UIHelper::press_enter_to_continue();
    break;
  case 2:
    std::cout << "\n";
    submit_review();
    UIHelper::press_enter_to_continue();
    break;
  case 3:
    std::cout << "\n";
    view_review_status();
    UIHelper::press_enter_to_continue();
    break;
  case 4:
    std::cout << "\n";
    set_reviewer_profile();
    UIHelper::press_enter_to_continue();
    break;
  case 5:
    std::cout << "\n";
    get_reviewer_profile();
    UIHelper::press_enter_to_continue();
    break;
  case 6:
    logout();
    break;
  default:
    UIHelper::print_error("无效的选择，请重试");
    UIHelper::press_enter_to_continue();
  }
}

void ReviewClient::show_editor_menu() {
  UIHelper::clear_screen();
  UIHelper::print_header("编辑面板 - " + username_);
  
  if (!context_->last_paper_id.empty()) {
    UIHelper::print_info("最近操作的论文: " + context_->last_paper_id);
  }
  if (!context_->last_reviewer.empty()) {
    UIHelper::print_info("最近分配的审稿人: " + context_->last_reviewer);
  }
  
  UIHelper::print_section("审稿人管理");
  UIHelper::print_menu_item(1, Icons::ASSIGN, "手动分配审稿人");
  UIHelper::print_menu_item(5, "🤖", "获取审稿人推荐 (智能匹配)");
  UIHelper::print_menu_item(6, "⚡", "自动分配审稿人");
  
  UIHelper::print_section("论文处理");
  UIHelper::print_menu_item(2, Icons::DECISION, "做出最终决定");
  UIHelper::print_menu_item(3, Icons::VIEW, "查看待处理论文");
  UIHelper::print_menu_item(4, "📊", "查看审稿进度");
  
  UIHelper::print_separator();
  UIHelper::print_menu_item(7, Icons::BACK, "退出登录");
  
  std::cout << "\n";
  UIHelper::prompt("请选择操作");

  int choice;
  std::cin >> choice;
  std::cin.ignore();

  switch (choice) {
  case 1:
    std::cout << "\n";
    assign_reviewer();
    UIHelper::press_enter_to_continue();
    break;
  case 2:
    std::cout << "\n";
    make_decision();
    UIHelper::press_enter_to_continue();
    break;
  case 3:
    std::cout << "\n";
    view_pending_papers();
    UIHelper::press_enter_to_continue();
    break;
  case 4:
    std::cout << "\n";
    view_review_progress();
    UIHelper::press_enter_to_continue();
    break;
  case 5:
    std::cout << "\n";
    get_reviewer_recommendations();
    UIHelper::press_enter_to_continue();
    break;
  case 6:
    std::cout << "\n";
    auto_assign_reviewers();
    UIHelper::press_enter_to_continue();
    break;
  case 7:
    logout();
    break;
  default:
    UIHelper::print_error("无效的选择，请重试");
    UIHelper::press_enter_to_continue();
  }
}

void ReviewClient::show_admin_menu() {
  UIHelper::clear_screen();
  UIHelper::print_header("管理员面板 - " + username_);
  
  UIHelper::print_section("用户管理");
  UIHelper::print_menu_item(1, "➕", "创建新用户");
  UIHelper::print_menu_item(4, Icons::USER, "查看所有用户");
  UIHelper::print_menu_item(8, "🗑️", "删除用户");
  
  UIHelper::print_section("系统管理");
  UIHelper::print_menu_item(2, "💻", "查看系统状态");
  UIHelper::print_menu_item(3, "💾", "创建系统备份");
  UIHelper::print_menu_item(5, "📋", "查看所有备份");
  UIHelper::print_menu_item(6, "♻️", "恢复系统备份");
  
  UIHelper::print_separator();
  UIHelper::print_menu_item(7, Icons::BACK, "退出登录");
  
  std::cout << "\n";
  UIHelper::prompt("请选择操作");

  int choice;
  std::cin >> choice;
  std::cin.ignore();

  switch (choice) {
  case 1:
    std::cout << "\n";
    create_user();
    UIHelper::press_enter_to_continue();
    break;
  case 2:
    std::cout << "\n";
    view_system_status();
    UIHelper::press_enter_to_continue();
    break;
  case 3:
    std::cout << "\n";
    create_backup();
    UIHelper::press_enter_to_continue();
    break;
  case 4:
    std::cout << "\n";
    list_users();
    UIHelper::press_enter_to_continue();
    break;
  case 5:
    std::cout << "\n";
    list_backups();
    UIHelper::press_enter_to_continue();
    break;
  case 6:
    std::cout << "\n";
    restore_backup();
    UIHelper::press_enter_to_continue();
    break;
  case 7:
    logout();
    break;
  case 8:
    std::cout << "\n";
    delete_user();
    UIHelper::press_enter_to_continue();
    break;
  default:
    UIHelper::print_error("无效的选择，请重试");
    UIHelper::press_enter_to_continue();
  }
}

void ReviewClient::upload_paper() {
  UIHelper::print_section("上传新论文");
  
  UIHelper::prompt("论文标题");
  std::string title = read_line();
  if (title.empty()) {
    UIHelper::print_warning("标题不能为空");
    return;
  }

  UIHelper::prompt("论文文件路径", context_->last_file_path);
  std::string file_path = read_line();
  if (file_path.empty() && !context_->last_file_path.empty()) {
    file_path = context_->last_file_path;
    UIHelper::print_info("使用上次的文件路径: " + file_path);
  }

  UIHelper::prompt("盲审策略 (single/double)", "single");
  std::string blind = read_line();
  if (blind.empty())
    blind = "single";

  UIHelper::prompt("研究领域 (逗号分隔，可选)");
  std::string fields = read_line();

  UIHelper::prompt("关键词 (逗号分隔，可选)");
  std::string keywords = read_line();

  UIHelper::prompt("冲突审稿人用户名 (逗号分隔，可选)");
  std::string conflicts = read_line();

  UIHelper::print_info("正在读取文件...");
  auto file_data = read_file(file_path);
  if (file_data.empty()) {
    UIHelper::print_error("无法读取文件: " + file_path);
    return;
  }
  
  context_->remember_file_path(file_path);
  UIHelper::print_success("文件读取成功 (" + std::to_string(file_data.size()) + " 字节)");

  protocol::Message msg;
  msg.command = protocol::Command::UPLOAD_PAPER;
  msg.params["title"] = title;
  msg.params["blind"] = blind;
  if (!fields.empty()) {
    msg.params["fields"] = fields;
  }
  if (!keywords.empty()) {
    msg.params["keywords"] = keywords;
  }
  if (!conflicts.empty()) {
    msg.params["conflict_usernames"] = conflicts;
  }
  msg.body = file_data;

  UIHelper::print_info("正在上传...");
  if (!send_message(msg)) {
    UIHelper::print_error("发送消息失败");
    return;
  }

  protocol::Response resp;
  if (!receive_response(resp)) {
    UIHelper::print_error("接收响应失败");
    return;
  }

  if (resp.status == protocol::StatusCode::OK) {
    UIHelper::print_success(resp.message);
  } else {
    UIHelper::print_error(resp.message);
  }
  
  if (!resp.body.empty()) {
    std::string body_str(resp.body.begin(), resp.body.end());
    std::cout << "\n" << Colors::CYAN << body_str << Colors::RESET << "\n";
    
    // Try to extract paper_id from response
    size_t id_pos = body_str.find("Paper ID:");
    if (id_pos != std::string::npos) {
      size_t start = id_pos + 9;
      size_t end = body_str.find_first_of(" \n\r", start);
      if (end != std::string::npos) {
        std::string paper_id = body_str.substr(start, end - start);
        context_->remember_paper(paper_id);
      }
    }
  }
}

void ReviewClient::view_paper_status() {
  UIHelper::print_section("查看论文状态");
  
  std::cout << Colors::DIM << "  提示: 输入 0 可列出您的所有论文" << Colors::RESET << "\n\n";
  
  UIHelper::prompt("论文ID (0=列表)", context_->last_paper_id);
  std::string paper_id = read_line();
  
  // 如果输入0，显示论文列表
  if (paper_id == "0") {
    protocol::Message list_msg;
    list_msg.command = protocol::Command::LIST_MY_PAPERS;
    
    UIHelper::print_info("正在获取论文列表...");
    send_message(list_msg);
    
    protocol::Response list_resp;
    receive_response(list_resp);
    
    if (!list_resp.body.empty()) {
      std::cout << Colors::CYAN << std::string(list_resp.body.begin(), list_resp.body.end()) 
                << Colors::RESET;
    }
    
    // 再次询问
    std::cout << "\n";
    UIHelper::prompt("请输入要查看的论文ID", context_->last_paper_id);
    paper_id = read_line();
  }
  
  if (paper_id.empty() && !context_->last_paper_id.empty()) {
    paper_id = context_->last_paper_id;
    UIHelper::print_info("使用最近的论文ID: " + paper_id);
  }
  
  if (paper_id.empty() || paper_id == "0") {
    return;
  }
  
  context_->remember_paper(paper_id);

  protocol::Message msg;
  msg.command = protocol::Command::VIEW_PAPER_STATUS;
  msg.params["paper_id"] = paper_id;

  UIHelper::print_info("正在查询详细信息...");
  send_message(msg);

  protocol::Response resp;
  receive_response(resp);

  std::cout << "\n";
  if (resp.status == protocol::StatusCode::OK) {
    UIHelper::print_success(resp.message);
  } else {
    UIHelper::print_error(resp.message);
  }
  
  if (!resp.body.empty()) {
    std::cout << "\n" << Colors::CYAN << std::string(resp.body.begin(), resp.body.end()) 
              << Colors::RESET << "\n";
  }
}

void ReviewClient::download_paper() {
  UIHelper::print_section("下载论文");
  
  UIHelper::prompt("论文ID", context_->last_paper_id);
  std::string paper_id = read_line();
  
  if (paper_id.empty() && !context_->last_paper_id.empty()) {
    paper_id = context_->last_paper_id;
    UIHelper::print_info("使用最近的论文ID: " + paper_id);
  }
  
  if (paper_id.empty()) {
    UIHelper::print_warning("论文ID不能为空");
    return;
  }
  
  context_->remember_paper(paper_id);

  UIHelper::prompt("保存路径", "paper_" + paper_id + ".pdf");
  std::string save_path = read_line();
  if (save_path.empty()) {
    save_path = "paper_" + paper_id + ".pdf";
  }

  protocol::Message msg;
  msg.command = protocol::Command::DOWNLOAD_PAPER;
  msg.params["paper_id"] = paper_id;

  UIHelper::print_info("正在下载...");
  send_message(msg);

  protocol::Response resp;
  receive_response(resp);

  if (resp.status == protocol::StatusCode::OK && !resp.body.empty()) {
    save_file(save_path, resp.body);
    UIHelper::print_success("论文已保存到: " + save_path + 
                           " (" + std::to_string(resp.body.size()) + " 字节)");
  } else {
    UIHelper::print_error(resp.message);
  }
}

// 辅助函数：读取多行文本输入（以空行结束）
std::string read_multiline(const std::string &prompt) {
  std::cout << Colors::CYAN << prompt << Colors::RESET << "\n";
  std::cout << Colors::DIM << "  (输入空行结束，或输入 /cancel 取消)" << Colors::RESET << "\n";
  
  std::ostringstream oss;
  std::string line;
  
  while (true) {
    std::cout << Colors::DIM << "  > " << Colors::RESET;
    if (!std::getline(std::cin, line)) break;
    
    if (line == "/cancel") {
      return "";
    }
    
    if (line.empty()) break;
    
    if (oss.tellp() > 0) oss << "\n";
    oss << line;
  }
  
  return oss.str();
}

void ReviewClient::submit_review() {
  UIHelper::print_section("📝 在线审稿表单");
  
  UIHelper::prompt("论文ID", context_->last_paper_id);
  std::string paper_id = read_line();
  
  if (paper_id.empty() && !context_->last_paper_id.empty()) {
    paper_id = context_->last_paper_id;
    UIHelper::print_info("使用最近的论文ID: " + paper_id);
  }
  
  if (paper_id.empty()) {
    UIHelper::print_warning("论文ID不能为空");
    return;
  }
  
  context_->remember_paper(paper_id);

  // 尝试加载已保存的草稿
  std::cout << "\n";
  UIHelper::print_info("正在检查是否有已保存的草稿...");
  
  protocol::Message draft_msg;
  draft_msg.command = protocol::Command::GET_REVIEW_DRAFT;
  draft_msg.params["paper_id"] = paper_id;
  send_message(draft_msg);
  
  protocol::Response draft_resp;
  receive_response(draft_resp);
  
  std::string existing_summary, existing_strengths, existing_weaknesses, existing_questions;
  std::string existing_rating = "0", existing_confidence = "0";
  
  if (draft_resp.status == protocol::StatusCode::OK && !draft_resp.body.empty()) {
    UIHelper::print_success("找到已保存的草稿！");
    std::string draft_json(draft_resp.body.begin(), draft_resp.body.end());
    
    // 简单解析JSON（提取已保存的内容）
    auto extract = [&](const std::string &key) -> std::string {
      std::string search = "\"" + key + "\": \"";
      size_t start = draft_json.find(search);
      if (start == std::string::npos) return "";
      start += search.length();
      size_t end = start;
      while (end < draft_json.length() && draft_json[end] != '"') {
        if (draft_json[end] == '\\' && end + 1 < draft_json.length()) {
          if (draft_json[end+1] == 'n') {
            end += 2;
            continue;
          }
        }
        end++;
      }
      std::string result = draft_json.substr(start, end - start);
      // 反转义 \n
      size_t pos = 0;
      while ((pos = result.find("\\n", pos)) != std::string::npos) {
        result.replace(pos, 2, "\n");
        pos++;
      }
      return result;
    };
    
    auto extract_int = [&](const std::string &key) -> std::string {
      std::string search = "\"" + key + "\": ";
      size_t start = draft_json.find(search);
      if (start == std::string::npos) return "0";
      start += search.length();
      size_t end = start;
      while (end < draft_json.length() && isdigit(draft_json[end])) end++;
      return draft_json.substr(start, end - start);
    };
    
    existing_summary = extract("summary");
    existing_strengths = extract("strengths");
    existing_weaknesses = extract("weaknesses");
    existing_questions = extract("questions");
    existing_rating = extract_int("rating");
    existing_confidence = extract_int("confidence");
    
    std::cout << Colors::DIM << "\n  草稿预览:\n";
    if (!existing_summary.empty()) {
      std::cout << "  总评: " << existing_summary.substr(0, 50) 
                << (existing_summary.length() > 50 ? "..." : "") << "\n";
    }
    if (existing_rating != "0") {
      std::cout << "  评分: " << existing_rating << "/5\n";
    }
    std::cout << Colors::RESET << "\n";
    
    std::cout << Colors::YELLOW << "  是否继续使用草稿内容？(y/n，n将清空重新输入): " << Colors::RESET;
    std::string use_draft = read_line();
    if (use_draft != "y" && use_draft != "Y") {
      existing_summary = existing_strengths = existing_weaknesses = existing_questions = "";
      existing_rating = existing_confidence = "0";
      UIHelper::print_info("已清空草稿，重新开始");
    }
  } else {
    UIHelper::print_info("没有已保存的草稿，开始新建审稿意见");
  }
  
  std::cout << "\n" << Colors::CYAN << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << Colors::RESET << "\n\n";

  // 表单输入
  std::string summary, strengths, weaknesses, questions, rating, confidence;
  
  // 1. 总评（必填）
  if (!existing_summary.empty()) {
    std::cout << Colors::GREEN << "[必填] 总评 (Summary)" << Colors::RESET 
              << Colors::DIM << " [已有内容，按回车保留或重新输入]" << Colors::RESET << "\n";
    std::cout << Colors::DIM << "当前: " << existing_summary.substr(0, 100) << "..." << Colors::RESET << "\n";
    summary = read_multiline("请输入总评");
    if (summary.empty()) summary = existing_summary;
  } else {
    summary = read_multiline(Colors::GREEN + "[必填] 总评 (Summary)" + Colors::RESET);
  }
  
  if (summary.empty() || summary == "/cancel") {
    UIHelper::print_warning("取消审稿");
    return;
  }
  
  // 2. 优点（选填）
  std::cout << "\n";
  if (!existing_strengths.empty()) {
    std::cout << Colors::CYAN << "[选填] 优点 (Strengths)" << Colors::RESET 
              << Colors::DIM << " [已有内容，按回车保留或重新输入]" << Colors::RESET << "\n";
    strengths = read_multiline("请输入优点");
    if (strengths.empty()) strengths = existing_strengths;
  } else {
    strengths = read_multiline(Colors::CYAN + "[选填] 优点 (Strengths)" + Colors::RESET);
  }
  
  // 3. 缺点（选填）
  std::cout << "\n";
  if (!existing_weaknesses.empty()) {
    std::cout << Colors::CYAN << "[选填] 缺点 (Weaknesses)" << Colors::RESET 
              << Colors::DIM << " [已有内容，按回车保留或重新输入]" << Colors::RESET << "\n";
    weaknesses = read_multiline("请输入缺点");
    if (weaknesses.empty()) weaknesses = existing_weaknesses;
  } else {
    weaknesses = read_multiline(Colors::CYAN + "[选填] 缺点 (Weaknesses)" + Colors::RESET);
  }
  
  // 4. 问题/建议（选填）
  std::cout << "\n";
  if (!existing_questions.empty()) {
    std::cout << Colors::CYAN << "[选填] 问题/建议 (Questions)" << Colors::RESET 
              << Colors::DIM << " [已有内容，按回车保留或重新输入]" << Colors::RESET << "\n";
    questions = read_multiline("请输入问题/建议");
    if (questions.empty()) questions = existing_questions;
  } else {
    questions = read_multiline(Colors::CYAN + "[选填] 问题/建议 (Questions)" + Colors::RESET);
  }
  
  // 5. 评分（必填）
  std::cout << "\n" << Colors::GREEN << "[必填] 评分 (Rating)" << Colors::RESET << "\n";
  std::cout << "  1 - Strong Reject\n";
  std::cout << "  2 - Weak Reject\n";
  std::cout << "  3 - Borderline\n";
  std::cout << "  4 - Weak Accept\n";
  std::cout << "  5 - Strong Accept\n";
  if (existing_rating != "0") {
    std::cout << Colors::DIM << "  [当前: " << existing_rating << "]" << Colors::RESET << "\n";
  }
  UIHelper::prompt("请输入评分 (1-5)", existing_rating != "0" ? existing_rating : "");
  rating = read_line();
  if (rating.empty() && existing_rating != "0") rating = existing_rating;
  
  if (rating.empty() || rating < "1" || rating > "5") {
    UIHelper::print_warning("评分必须是1-5之间的整数");
    return;
  }
  
  // 6. 置信度（必填）
  std::cout << "\n" << Colors::GREEN << "[必填] 置信度 (Confidence)" << Colors::RESET << "\n";
  std::cout << "  1 - Very Low\n";
  std::cout << "  2 - Low\n";
  std::cout << "  3 - Medium\n";
  std::cout << "  4 - High\n";
  std::cout << "  5 - Very High\n";
  if (existing_confidence != "0") {
    std::cout << Colors::DIM << "  [当前: " << existing_confidence << "]" << Colors::RESET << "\n";
  }
  UIHelper::prompt("请输入置信度 (1-5)", existing_confidence != "0" ? existing_confidence : "");
  confidence = read_line();
  if (confidence.empty() && existing_confidence != "0") confidence = existing_confidence;
  
  if (confidence.empty() || confidence < "1" || confidence > "5") {
    UIHelper::print_warning("置信度必须是1-5之间的整数");
    return;
  }
  
  // 选择操作
  std::cout << "\n" << Colors::CYAN << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << Colors::RESET << "\n\n";
  std::cout << Colors::YELLOW << "选择操作:\n" << Colors::RESET;
  std::cout << "  [1] 💾 保存草稿 (可稍后继续编辑)\n";
  std::cout << "  [2] ✅ 提交审稿意见 (不可再修改)\n";
  std::cout << "  [3] ❌ 取消\n\n";
  UIHelper::prompt("请选择");
  std::string choice = read_line();
  
  protocol::Message msg;
  msg.params["paper_id"] = paper_id;
  msg.params["summary"] = summary;
  msg.params["strengths"] = strengths;
  msg.params["weaknesses"] = weaknesses;
  msg.params["questions"] = questions;
  msg.params["rating"] = rating;
  msg.params["confidence"] = confidence;
  
  if (choice == "1") {
    // 保存草稿
    msg.command = protocol::Command::SAVE_REVIEW_DRAFT;
    UIHelper::print_info("正在保存草稿...");
  } else if (choice == "2") {
    // 提交最终版
    msg.command = protocol::Command::SUBMIT_REVIEW;
    UIHelper::print_info("正在提交审稿意见...");
  } else {
    UIHelper::print_warning("已取消");
    return;
  }
  
  send_message(msg);
  protocol::Response resp;
  receive_response(resp);

  if (resp.status == protocol::StatusCode::OK) {
    UIHelper::print_success(resp.message);
  } else {
    UIHelper::print_error(resp.message);
  }
}

void ReviewClient::assign_reviewer() {
  UIHelper::print_section("分配审稿人");
  
  UIHelper::prompt("论文ID", context_->last_paper_id);
  std::string paper_id = read_line();
  
  if (paper_id.empty() && !context_->last_paper_id.empty()) {
    paper_id = context_->last_paper_id;
    UIHelper::print_info("使用最近的论文ID: " + paper_id);
  }
  
  if (paper_id.empty()) {
    UIHelper::print_warning("论文ID不能为空");
    return;
  }
  
  context_->remember_paper(paper_id);

  UIHelper::prompt("审稿人用户名", context_->last_reviewer);
  std::string reviewer = read_line();
  
  if (reviewer.empty() && !context_->last_reviewer.empty()) {
    reviewer = context_->last_reviewer;
    UIHelper::print_info("使用最近的审稿人: " + reviewer);
  }
  
  if (reviewer.empty()) {
    UIHelper::print_warning("审稿人用户名不能为空");
    return;
  }
  
  context_->remember_reviewer(reviewer);

  UIHelper::prompt("审稿轮次 (R1/R2/REBUTTAL，可选)", "R1");
  std::string round = read_line();

  UIHelper::prompt("盲审策略 (single/double，可选)");
  std::string blind = read_line();

  protocol::Message msg;
  msg.command = protocol::Command::ASSIGN_REVIEWER;
  msg.params["paper_id"] = paper_id;
  msg.params["reviewer"] = reviewer;
  if (!round.empty()) {
    msg.params["round"] = round;
  }
  if (!blind.empty()) {
    msg.params["blind"] = blind;
  }

  UIHelper::print_info("正在分配...");
  send_message(msg);

  protocol::Response resp;
  receive_response(resp);

  if (resp.status == protocol::StatusCode::OK) {
    UIHelper::print_success(resp.message);
  } else {
    UIHelper::print_error(resp.message);
  }
}

void ReviewClient::make_decision() {
  UIHelper::print_section("做出最终决定");
  
  UIHelper::prompt("论文ID", context_->last_paper_id);
  std::string paper_id = read_line();
  
  if (paper_id.empty() && !context_->last_paper_id.empty()) {
    paper_id = context_->last_paper_id;
    UIHelper::print_info("使用最近的论文ID: " + paper_id);
  }
  
  if (paper_id.empty()) {
    UIHelper::print_warning("论文ID不能为空");
    return;
  }
  
  context_->remember_paper(paper_id);

  std::cout << "\n" << Colors::YELLOW << "可选决定:" << Colors::RESET << "\n";
  std::cout << "  1. accept          - 接受\n";
  std::cout << "  2. reject          - 拒绝\n";
  std::cout << "  3. major_revision  - 需大修\n";
  std::cout << "  4. minor_revision  - 需小修\n\n";
  
  UIHelper::prompt("请输入决定");
  std::string decision = read_line();
  
  if (decision.empty()) {
    UIHelper::print_warning("决定不能为空");
    return;
  }

  // Confirm important decision
  if (!UIHelper::confirm("确认要对论文 " + paper_id + " 做出决定: " + decision + " 吗？")) {
    UIHelper::print_info("操作已取消");
    return;
  }

  protocol::Message msg;
  msg.command = protocol::Command::MAKE_DECISION;
  msg.params["paper_id"] = paper_id;
  msg.params["decision"] = decision;

  UIHelper::print_info("正在提交决定...");
  send_message(msg);

  protocol::Response resp;
  receive_response(resp);

  if (resp.status == protocol::StatusCode::OK) {
    UIHelper::print_success(resp.message);
  } else {
    UIHelper::print_error(resp.message);
  }
}

void ReviewClient::create_user() {
  UIHelper::print_section("创建新用户");
  
  UIHelper::prompt("用户名");
  std::string username = read_line();
  
  if (username.empty()) {
    UIHelper::print_warning("用户名不能为空");
    return;
  }

  UIHelper::prompt("密码");
  std::string password = read_line();
  
  if (password.empty()) {
    UIHelper::print_warning("密码不能为空");
    return;
  }

  std::cout << "\n" << Colors::YELLOW << "可选角色:" << Colors::RESET << "\n";
  std::cout << "  1. author   - 作者\n";
  std::cout << "  2. reviewer - 审稿人\n";
  std::cout << "  3. editor   - 编辑\n";
  std::cout << "  4. admin    - 管理员\n\n";
  
  UIHelper::prompt("角色 (输入数字)");
  std::string role_input = read_line();
  
  if (role_input.empty()) {
    UIHelper::print_warning("角色不能为空");
    return;
  }

  // 将数字转换为角色字符串
  std::string role;
  if (role_input == "1") {
    role = "author";
  } else if (role_input == "2") {
    role = "reviewer";
  } else if (role_input == "3") {
    role = "editor";
  } else if (role_input == "4") {
    role = "admin";
  } else {
    // 也支持直接输入角色名称
    role = role_input;
  }

  protocol::Message msg;
  msg.command = protocol::Command::CREATE_USER;
  msg.params["username"] = username;
  msg.params["password"] = password;
  msg.params["role"] = role;

  UIHelper::print_info("正在创建用户 (" + username + " - " + role + ")...");
  send_message(msg);

  protocol::Response resp;
  receive_response(resp);

  if (resp.status == protocol::StatusCode::CREATED || resp.status == protocol::StatusCode::OK) {
    UIHelper::print_success("用户创建成功: " + username + " (" + role + ")");
  } else {
    UIHelper::print_error(resp.message);
  }
}

void ReviewClient::view_system_status() {
  UIHelper::print_section("系统状态");
  
  protocol::Message msg;
  msg.command = protocol::Command::SYSTEM_STATUS;

  UIHelper::print_info("正在获取系统状态...");
  send_message(msg);

  protocol::Response resp;
  receive_response(resp);

  std::cout << "\n" << Colors::CYAN << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" 
            << Colors::RESET << "\n";
  std::cout << std::string(resp.body.begin(), resp.body.end()) << "\n";
  std::cout << Colors::CYAN << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" 
            << Colors::RESET << "\n";
}

void ReviewClient::create_backup() {
  UIHelper::print_section("创建系统备份");
  
  UIHelper::prompt("备份名称");
  std::string name = read_line();
  
  if (name.empty()) {
    UIHelper::print_warning("备份名称不能为空");
    return;
  }

  if (!UIHelper::confirm("确认创建备份 '" + name + "'？")) {
    UIHelper::print_info("操作已取消");
    return;
  }

  protocol::Message msg;
  msg.command = protocol::Command::CREATE_BACKUP;
  msg.params["name"] = name;

  UIHelper::print_info("正在创建备份...");
  send_message(msg);

  protocol::Response resp;
  receive_response(resp);

  if (resp.status == protocol::StatusCode::OK) {
    UIHelper::print_success(resp.message);
  } else {
    UIHelper::print_error(resp.message);
  }
}

void ReviewClient::submit_revision() {
  UIHelper::print_section("提交修订版");
  
  UIHelper::prompt("论文ID", context_->last_paper_id);
  std::string paper_id = read_line();
  
  if (paper_id.empty() && !context_->last_paper_id.empty()) {
    paper_id = context_->last_paper_id;
    UIHelper::print_info("使用最近的论文ID: " + paper_id);
  }
  
  if (paper_id.empty()) {
    UIHelper::print_warning("论文ID不能为空");
    return;
  }
  
  context_->remember_paper(paper_id);

  UIHelper::prompt("修订版文件路径");
  std::string file_path = read_line();
  
  if (file_path.empty()) {
    UIHelper::print_warning("文件路径不能为空");
    return;
  }

  UIHelper::print_info("正在读取文件...");
  auto file_data = read_file(file_path);
  if (file_data.empty()) {
    UIHelper::print_error("无法读取文件: " + file_path);
    return;
  }
  
  UIHelper::print_success("文件读取成功 (" + std::to_string(file_data.size()) + " 字节)");

  protocol::Message msg;
  msg.command = protocol::Command::SUBMIT_REVISION;
  msg.params["paper_id"] = paper_id;
  msg.body = file_data;

  UIHelper::print_info("正在提交修订版...");
  send_message(msg);
  protocol::Response resp;
  receive_response(resp);
  
  if (resp.status == protocol::StatusCode::OK) {
    UIHelper::print_success(resp.message);
  } else {
    UIHelper::print_error(resp.message);
  }
}

void ReviewClient::download_reviews() {
  UIHelper::print_section("下载审稿意见");
  
  UIHelper::prompt("论文ID", context_->last_paper_id);
  std::string paper_id = read_line();
  
  if (paper_id.empty() && !context_->last_paper_id.empty()) {
    paper_id = context_->last_paper_id;
    UIHelper::print_info("使用最近的论文ID: " + paper_id);
  }
  
  if (paper_id.empty()) {
    UIHelper::print_warning("论文ID不能为空");
    return;
  }
  
  context_->remember_paper(paper_id);

  UIHelper::prompt("审稿轮次 (R1/R2/REBUTTAL，可选)");
  std::string round = read_line();

  protocol::Message msg;
  msg.command = protocol::Command::DOWNLOAD_REVIEWS;
  msg.params["paper_id"] = paper_id;
  if (!round.empty()) {
    msg.params["round"] = round;
  }

  UIHelper::print_info("正在获取审稿意见...");
  send_message(msg);
  protocol::Response resp;
  receive_response(resp);

  std::cout << "\n";
  if (resp.status == protocol::StatusCode::OK) {
    UIHelper::print_success(resp.message);
  } else {
    UIHelper::print_error(resp.message);
  }
  
  if (!resp.body.empty()) {
    std::cout << "\n" << Colors::CYAN << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" 
              << Colors::RESET << "\n";
    std::cout << std::string(resp.body.begin(), resp.body.end()) << "\n";
    std::cout << Colors::CYAN << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" 
              << Colors::RESET << "\n";
  }
}

void ReviewClient::view_review_status() {
  UIHelper::print_section("查看审稿状态");
  
  std::cout << Colors::DIM << "  提示: 输入 0 可列出分配给您的所有论文" << Colors::RESET << "\n\n";
  
  UIHelper::prompt("论文ID (0=列表)", context_->last_paper_id);
  std::string paper_id = read_line();
  
  // 如果输入0，显示待审论文列表
  if (paper_id == "0") {
    protocol::Message list_msg;
    list_msg.command = protocol::Command::LIST_ASSIGNED_PAPERS;
    
    UIHelper::print_info("正在获取待审论文列表...");
    send_message(list_msg);
    
    protocol::Response list_resp;
    receive_response(list_resp);
    
    if (!list_resp.body.empty()) {
      std::cout << Colors::CYAN << std::string(list_resp.body.begin(), list_resp.body.end()) 
                << Colors::RESET;
    }
    
    // 再次询问
    std::cout << "\n";
    UIHelper::prompt("请输入要查看的论文ID", context_->last_paper_id);
    paper_id = read_line();
  }
  
  if (paper_id.empty() && !context_->last_paper_id.empty()) {
    paper_id = context_->last_paper_id;
    UIHelper::print_info("使用最近的论文ID: " + paper_id);
  }
  
  if (paper_id.empty() || paper_id == "0") {
    return;
  }
  
  context_->remember_paper(paper_id);

  protocol::Message msg;
  msg.command = protocol::Command::VIEW_REVIEW_STATUS;
  msg.params["paper_id"] = paper_id;

  UIHelper::print_info("正在查询详细信息...");
  send_message(msg);
  protocol::Response resp;
  receive_response(resp);

  std::cout << "\n";
  if (resp.status == protocol::StatusCode::OK) {
    UIHelper::print_success(resp.message);
  } else {
    UIHelper::print_error(resp.message);
  }
  
  if (!resp.body.empty()) {
    std::cout << "\n" << Colors::CYAN << std::string(resp.body.begin(), resp.body.end()) 
              << Colors::RESET << "\n";
  }
}

void ReviewClient::view_pending_papers() {
  UIHelper::print_section("待处理论文列表");
  
  protocol::Message msg;
  msg.command = protocol::Command::VIEW_PENDING_PAPERS;
  
  UIHelper::print_info("正在获取列表...");
  send_message(msg);
  protocol::Response resp;
  receive_response(resp);
  
  std::cout << "\n";
  if (!resp.body.empty()) {
    std::cout << Colors::CYAN << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" 
              << Colors::RESET << "\n";
    std::cout << std::string(resp.body.begin(), resp.body.end()) << "\n";
    std::cout << Colors::CYAN << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" 
              << Colors::RESET << "\n";
  } else {
    UIHelper::print_info("没有待处理的论文");
  }
}

void ReviewClient::view_review_progress() {
  UIHelper::print_section("查看审稿进度");
  
  std::cout << Colors::DIM << "  提示: 输入 0 可列出系统中的所有论文" << Colors::RESET << "\n\n";
  
  UIHelper::prompt("论文ID (0=列表)", context_->last_paper_id);
  std::string paper_id = read_line();
  
  // 如果输入0，显示所有论文列表
  if (paper_id == "0") {
    protocol::Message list_msg;
    list_msg.command = protocol::Command::LIST_ALL_PAPERS;
    
    UIHelper::print_info("正在获取所有论文列表...");
    send_message(list_msg);
    
    protocol::Response list_resp;
    receive_response(list_resp);
    
    if (!list_resp.body.empty()) {
      std::cout << Colors::CYAN << std::string(list_resp.body.begin(), list_resp.body.end()) 
                << Colors::RESET;
    }
    
    // 再次询问
    std::cout << "\n";
    UIHelper::prompt("请输入要查看的论文ID", context_->last_paper_id);
    paper_id = read_line();
  }
  
  if (paper_id.empty() && !context_->last_paper_id.empty()) {
    paper_id = context_->last_paper_id;
    UIHelper::print_info("使用最近的论文ID: " + paper_id);
  }
  
  if (paper_id.empty() || paper_id == "0") {
    return;
  }
  
  context_->remember_paper(paper_id);

  protocol::Message msg;
  msg.command = protocol::Command::VIEW_REVIEW_PROGRESS;
  msg.params["paper_id"] = paper_id;

  UIHelper::print_info("正在查询详细进度...");
  send_message(msg);
  protocol::Response resp;
  receive_response(resp);

  std::cout << "\n";
  if (resp.status == protocol::StatusCode::OK) {
    UIHelper::print_success(resp.message);
  } else {
    UIHelper::print_error(resp.message);
  }
  
  if (!resp.body.empty()) {
    std::cout << "\n" << Colors::CYAN << std::string(resp.body.begin(), resp.body.end()) 
              << Colors::RESET << "\n";
  }
}

void ReviewClient::list_users() {
  UIHelper::print_section("用户列表");
  
  protocol::Message msg;
  msg.command = protocol::Command::LIST_USERS;
  
  UIHelper::print_info("正在获取用户列表...");
  send_message(msg);
  protocol::Response resp;
  receive_response(resp);
  
  std::cout << "\n" << Colors::CYAN << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" 
            << Colors::RESET << "\n";
  std::cout << std::string(resp.body.begin(), resp.body.end()) << "\n";
  std::cout << Colors::CYAN << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" 
            << Colors::RESET << "\n";
}

void ReviewClient::list_backups() {
  UIHelper::print_section("备份列表");
  
  protocol::Message msg;
  msg.command = protocol::Command::LIST_BACKUPS;
  
  UIHelper::print_info("正在获取备份列表...");
  send_message(msg);
  protocol::Response resp;
  receive_response(resp);
  
  std::cout << "\n" << Colors::CYAN << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" 
            << Colors::RESET << "\n";
  std::cout << std::string(resp.body.begin(), resp.body.end()) << "\n";
  std::cout << Colors::CYAN << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" 
            << Colors::RESET << "\n";
}

void ReviewClient::restore_backup() {
  UIHelper::print_section("恢复系统备份");
  
  UIHelper::print_warning("⚠️  恢复备份将覆盖当前系统数据！");
  
  UIHelper::prompt("备份名称");
  std::string name = read_line();
  
  if (name.empty()) {
    UIHelper::print_warning("备份名称不能为空");
    return;
  }

  if (!UIHelper::confirm("⚠️  确认要恢复备份 '" + name + "' 吗？这将覆盖当前数据！")) {
    UIHelper::print_info("操作已取消");
    return;
  }

  protocol::Message msg;
  msg.command = protocol::Command::RESTORE_BACKUP;
  msg.params["name"] = name;

  UIHelper::print_info("正在恢复备份...");
  send_message(msg);
  protocol::Response resp;
  receive_response(resp);
  
  if (resp.status == protocol::StatusCode::OK) {
    UIHelper::print_success(resp.message);
  } else {
    UIHelper::print_error(resp.message);
  }
}

void ReviewClient::delete_user() {
  UIHelper::print_section("删除用户");
  
  UIHelper::print_warning("⚠️  删除用户将永久移除该用户的账号！");
  
  UIHelper::prompt("要删除的用户名");
  std::string username = read_line();
  
  if (username.empty()) {
    UIHelper::print_warning("用户名不能为空");
    return;
  }
  
  // 防止删除admin账户
  if (username == "admin") {
    UIHelper::print_error("不能删除admin账户！");
    return;
  }
  
  // 防止删除自己
  if (username == username_) {
    UIHelper::print_error("不能删除当前登录的账户！");
    return;
  }

  if (!UIHelper::confirm("⚠️  确认要删除用户 '" + username + "' 吗？此操作不可恢复！")) {
    UIHelper::print_info("操作已取消");
    return;
  }

  protocol::Message msg;
  msg.command = protocol::Command::DELETE_USER;
  msg.params["username"] = username;

  UIHelper::print_info("正在删除用户...");
  send_message(msg);
  protocol::Response resp;
  receive_response(resp);
  
  if (resp.status == protocol::StatusCode::OK) {
    UIHelper::print_success("用户已删除: " + username);
  } else {
    UIHelper::print_error(resp.message);
  }
}

bool ReviewClient::send_message(const protocol::Message &msg) {
  auto data = protocol::Protocol::serialize_message(msg);
  ssize_t sent = send(socket_, data.data(), data.size(), 0);
  return sent == static_cast<ssize_t>(data.size());
}

bool ReviewClient::receive_response(protocol::Response &resp) {
  // First, receive header (status line + body length line)
  std::vector<char> header_buffer(1024);
  ssize_t header_received = 0;

  // Receive until we have at least two newlines (status + length)
  while (header_received < 1024) {
    ssize_t n = recv(socket_, header_buffer.data() + header_received,
                     1024 - header_received, 0);
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
        // We have the header, extract body length
        std::string length_str =
            temp.substr(first_newline + 1, second_newline - first_newline - 1);
        size_t body_length = std::stoull(length_str);

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
              recv(socket_, complete_buffer.data() + header_end + body_received,
                   body_length - body_received, 0);
          if (n <= 0) {
            return false;
          }
          body_received += n;
        }

        return protocol::Protocol::deserialize_response(complete_buffer, resp);
      }
    }
  }

  return false;
}

std::string ReviewClient::read_line() {
  std::string line;
  std::getline(std::cin, line);
  return line;
}

std::vector<char> ReviewClient::read_file(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return {};
  }

  return std::vector<char>((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
}

void ReviewClient::save_file(const std::string &path,
                             const std::vector<char> &data) {
  std::ofstream file(path, std::ios::binary);
  file.write(data.data(), data.size());
}

// ===== Profile & Assignment Methods =====

void ReviewClient::set_reviewer_profile() {
  UIHelper::print_section("设置审稿人资料");
  
  std::cout << Colors::DIM << "  提示: 设置您的研究领域和关键词可以帮助系统智能匹配论文\n" 
            << Colors::RESET << "\n";
  
  UIHelper::prompt("研究领域 (逗号分隔，例如: AI,ML,NLP)");
  std::string fields = read_line();
  
  UIHelper::prompt("关键词 (逗号分隔，例如: deep learning,transformer)");
  std::string keywords = read_line();
  
  UIHelper::prompt("所属机构 (例如: MIT, Stanford)");
  std::string affiliation = read_line();
  
  if (fields.empty() && keywords.empty() && affiliation.empty()) {
    UIHelper::print_warning("至少需要填写一项信息");
    return;
  }
  
  protocol::Message msg;
  msg.command = protocol::Command::SET_REVIEWER_PROFILE;
  msg.params["fields"] = fields;
  msg.params["keywords"] = keywords;
  msg.params["affiliation"] = affiliation;
  
  UIHelper::print_info("正在更新资料...");
  send_message(msg);
  protocol::Response resp;
  receive_response(resp);
  
  if (resp.status == protocol::StatusCode::OK) {
    UIHelper::print_success(resp.message);
  } else {
    UIHelper::print_error(resp.message);
  }
}

void ReviewClient::get_reviewer_profile() {
  UIHelper::print_section("我的审稿人资料");
  
  protocol::Message msg;
  msg.command = protocol::Command::GET_REVIEWER_PROFILE;
  
  UIHelper::print_info("正在获取资料...");
  send_message(msg);
  protocol::Response resp;
  receive_response(resp);
  
  std::cout << "\n";
  if (resp.status == protocol::StatusCode::OK) {
    UIHelper::print_success(resp.message);
  } else {
    UIHelper::print_error(resp.message);
  }
  
  if (!resp.body.empty()) {
    std::cout << "\n" << Colors::CYAN << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" 
              << Colors::RESET << "\n";
    std::cout << std::string(resp.body.begin(), resp.body.end()) << "\n";
    std::cout << Colors::CYAN << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" 
              << Colors::RESET << "\n";
  }
}

void ReviewClient::get_reviewer_recommendations() {
  UIHelper::print_section("获取审稿人推荐");
  
  UIHelper::prompt("论文ID", context_->last_paper_id);
  std::string paper_id = read_line();
  
  if (paper_id.empty() && !context_->last_paper_id.empty()) {
    paper_id = context_->last_paper_id;
    UIHelper::print_info("使用最近的论文ID: " + paper_id);
  }
  
  if (paper_id.empty()) {
    UIHelper::print_warning("论文ID不能为空");
    return;
  }
  
  context_->remember_paper(paper_id);
  
  UIHelper::prompt("推荐数量 (Top K)", "5");
  std::string k_str = read_line();
  int k = k_str.empty() ? 5 : std::stoi(k_str);
  
  protocol::Message msg;
  msg.command = protocol::Command::GET_REVIEWER_RECOMMENDATIONS;
  msg.params["paper_id"] = paper_id;
  msg.params["k"] = std::to_string(k);
  
  UIHelper::print_info("🤖 正在智能匹配审稿人...");
  send_message(msg);
  protocol::Response resp;
  receive_response(resp);
  
  std::cout << "\n";
  if (!resp.body.empty()) {
    std::cout << Colors::GREEN << "推荐结果:" << Colors::RESET << "\n";
    std::cout << Colors::CYAN << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" 
              << Colors::RESET << "\n";
    std::cout << std::string(resp.body.begin(), resp.body.end()) << "\n";
    std::cout << Colors::CYAN << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" 
              << Colors::RESET << "\n";
  } else {
    UIHelper::print_error(resp.message);
  }
}

void ReviewClient::auto_assign_reviewers() {
  UIHelper::print_section("自动分配审稿人");
  
  UIHelper::prompt("论文ID", context_->last_paper_id);
  std::string paper_id = read_line();
  
  if (paper_id.empty() && !context_->last_paper_id.empty()) {
    paper_id = context_->last_paper_id;
    UIHelper::print_info("使用最近的论文ID: " + paper_id);
  }
  
  if (paper_id.empty()) {
    UIHelper::print_warning("论文ID不能为空");
    return;
  }
  
  context_->remember_paper(paper_id);
  
  UIHelper::prompt("分配审稿人数量", "3");
  std::string n_str = read_line();
  if (n_str.empty()) n_str = "3";
  
  int n = std::stoi(n_str);
  
  if (!UIHelper::confirm("确认自动分配 " + std::to_string(n) + " 位审稿人给论文 " + paper_id + " 吗？")) {
    UIHelper::print_info("操作已取消");
    return;
  }
  
  protocol::Message msg;
  msg.command = protocol::Command::AUTO_ASSIGN_REVIEWERS;
  msg.params["paper_id"] = paper_id;
  msg.params["n"] = std::to_string(n);
  
  UIHelper::print_info("⚡ 正在自动分配审稿人...");
  send_message(msg);
  protocol::Response resp;
  receive_response(resp);
  
  std::cout << "\n";
  if (resp.status == protocol::StatusCode::OK) {
    UIHelper::print_success(resp.message);
  } else {
    UIHelper::print_error(resp.message);
  }
  
  if (!resp.body.empty()) {
    std::cout << "\n" << Colors::CYAN << std::string(resp.body.begin(), resp.body.end()) 
              << Colors::RESET << "\n";
  }
}

} // namespace client
