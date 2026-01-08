// 将以下代码替换到 review_server.cpp 中的对应函数

// 1. 添加到类的私有辅助函数部分
private:
  std::string get_review_file_path(const std::string &paper_dir, 
                                    const std::string &round, 
                                    const std::string &reviewer) {
    return round_dir(paper_dir, round) + "/reviews/" + reviewer + ".json";
  }
  
  bool save_structured_review(const StructuredReview &review) {
    std::string paper_dir = "/papers/" + review.paper_id;
    std::string review_file = get_review_file_path(paper_dir, review.round, review.reviewer);
    
    ensure_round_dirs(paper_dir, review.round);
    vfs_->create_file(review_file, 0644);
    
    int fd = vfs_->open(review_file, O_WRONLY | O_TRUNC);
    if (fd < 0) return false;
    
    std::string json_data = review.to_json();
    ssize_t written = vfs_->write(fd, json_data.data(), json_data.size());
    vfs_->close(fd);
    
    return written == static_cast<ssize_t>(json_data.size());
  }
  
  bool load_structured_review(const std::string &paper_id, 
                             const std::string &round,
                             const std::string &reviewer,
                             StructuredReview &review) {
    std::string paper_dir = "/papers/" + paper_id;
    std::string review_file = get_review_file_path(paper_dir, round, reviewer);
    
    if (!vfs_->exists(review_file)) return false;
    
    int fd = vfs_->open(review_file, O_RDONLY);
    if (fd < 0) return false;
    
    char buffer[8192];
    ssize_t bytes_read = vfs_->read(fd, buffer, sizeof(buffer) - 1);
    vfs_->close(fd);
    
    if (bytes_read <= 0) return false;
    
    buffer[bytes_read] = '\0';
    review = StructuredReview::from_json(std::string(buffer));
    return true;
  }

// 2. 重写 handle_submit_review
protocol::Response
ReviewServer::handle_submit_review(const protocol::Message &msg,
                                   const std::string &session_id) {
  auto it_paper_id = msg.params.find("paper_id");
  if (it_paper_id == msg.params.end()) {
    return protocol::Response(protocol::StatusCode::BAD_REQUEST,
                              "Missing paper_id");
  }

  std::string username = auth_manager_->get_username(session_id);
  std::string paper_dir = "/papers/" + it_paper_id->second;

  if (!vfs_->exists(paper_dir)) {
    return protocol::Response(protocol::StatusCode::NOT_FOUND,
                              "Paper not found");
  }

  PaperStatus status = load_paper_status(paper_dir);
  std::string round = status.current_round;

  if (!is_reviewer_assigned(paper_dir, round, username)) {
    return protocol::Response(protocol::StatusCode::FORBIDDEN,
                              "Not assigned to this paper/round");
  }

  // 构建结构化审稿意见
  StructuredReview review;
  review.paper_id = it_paper_id->second;
  review.reviewer = username;
  review.round = round;
  
  // 从参数中提取各字段
  auto it = msg.params.find("summary");
  if (it != msg.params.end()) review.summary = it->second;
  
  it = msg.params.find("strengths");
  if (it != msg.params.end()) review.strengths = it->second;
  
  it = msg.params.find("weaknesses");
  if (it != msg.params.end()) review.weaknesses = it->second;
  
  it = msg.params.find("questions");
  if (it != msg.params.end()) review.questions = it->second;
  
  it = msg.params.find("rating");
  if (it != msg.params.end()) {
    try {
      review.rating = std::stoi(it->second);
    } catch (...) {
      review.rating = 0;
    }
  }
  
  it = msg.params.find("confidence");
  if (it != msg.params.end()) {
    try {
      review.confidence = std::stoi(it->second);
    } catch (...) {
      review.confidence = 0;
    }
  }
  
  // 验证必填字段
  if (review.summary.empty() || review.rating == 0) {
    return protocol::Response(protocol::StatusCode::BAD_REQUEST,
                              "总评和评分为必填项");
  }
  
  // 标记为已提交
  review.status = "submitted";
  review.last_modified = std::time(nullptr);
  review.submitted_at = std::time(nullptr);
  
  if (!save_structured_review(review)) {
    return protocol::Response(protocol::StatusCode::INTERNAL_ERROR,
                              "Failed to save review");
  }
  
  std::cout << "[DEBUG] Review submitted: paper=" << review.paper_id 
            << ", reviewer=" << review.reviewer 
            << ", rating=" << review.rating << std::endl;

  return protocol::Response(protocol::StatusCode::OK, "审稿意见已提交");
}

// 3. 新增 handle_save_review_draft
protocol::Response
ReviewServer::handle_save_review_draft(const protocol::Message &msg,
                                      const std::string &session_id) {
  auto it_paper_id = msg.params.find("paper_id");
  if (it_paper_id == msg.params.end()) {
    return protocol::Response(protocol::StatusCode::BAD_REQUEST,
                              "Missing paper_id");
  }

  std::string username = auth_manager_->get_username(session_id);
  std::string paper_dir = "/papers/" + it_paper_id->second;

  if (!vfs_->exists(paper_dir)) {
    return protocol::Response(protocol::StatusCode::NOT_FOUND,
                              "Paper not found");
  }

  PaperStatus status = load_paper_status(paper_dir);
  std::string round = status.current_round;

  if (!is_reviewer_assigned(paper_dir, round, username)) {
    return protocol::Response(protocol::StatusCode::FORBIDDEN,
                              "Not assigned to this paper/round");
  }

  // 构建结构化审稿意见（草稿）
  StructuredReview review;
  review.paper_id = it_paper_id->second;
  review.reviewer = username;
  review.round = round;
  
  // 从参数中提取各字段（草稿可以不完整）
  auto it = msg.params.find("summary");
  if (it != msg.params.end()) review.summary = it->second;
  
  it = msg.params.find("strengths");
  if (it != msg.params.end()) review.strengths = it->second;
  
  it = msg.params.find("weaknesses");
  if (it != msg.params.end()) review.weaknesses = it->second;
  
  it = msg.params.find("questions");
  if (it != msg.params.end()) review.questions = it->second;
  
  it = msg.params.find("rating");
  if (it != msg.params.end()) {
    try {
      review.rating = std::stoi(it->second);
    } catch (...) {
      review.rating = 0;
    }
  }
  
  it = msg.params.find("confidence");
  if (it != msg.params.end()) {
    try {
      review.confidence = std::stoi(it->second);
    } catch (...) {
      review.confidence = 0;
    }
  }
  
  // 标记为草稿
  review.status = "draft";
  review.last_modified = std::time(nullptr);
  review.submitted_at = 0;
  
  if (!save_structured_review(review)) {
    return protocol::Response(protocol::StatusCode::INTERNAL_ERROR,
                              "Failed to save draft");
  }
  
  std::cout << "[DEBUG] Review draft saved: paper=" << review.paper_id 
            << ", reviewer=" << review.reviewer << std::endl;

  return protocol::Response(protocol::StatusCode::OK, "草稿已保存");
}

// 4. 新增 handle_get_review_draft
protocol::Response
ReviewServer::handle_get_review_draft(const protocol::Message &msg,
                                     const std::string &session_id) {
  auto it_paper_id = msg.params.find("paper_id");
  if (it_paper_id == msg.params.end()) {
    return protocol::Response(protocol::StatusCode::BAD_REQUEST,
                              "Missing paper_id");
  }

  std::string username = auth_manager_->get_username(session_id);
  std::string paper_dir = "/papers/" + it_paper_id->second;

  if (!vfs_->exists(paper_dir)) {
    return protocol::Response(protocol::StatusCode::NOT_FOUND,
                              "Paper not found");
  }

  PaperStatus status = load_paper_status(paper_dir);
  std::string round = status.current_round;

  if (!is_reviewer_assigned(paper_dir, round, username)) {
    return protocol::Response(protocol::StatusCode::FORBIDDEN,
                              "Not assigned to this paper/round");
  }

  // 尝试加载已保存的审稿意见
  StructuredReview review;
  bool exists = load_structured_review(it_paper_id->second, round, username, review);
  
  protocol::Response resp(protocol::StatusCode::OK, 
                         exists ? "Found existing review" : "No existing review");
  
  if (exists) {
    // 返回JSON数据
    std::string json_data = review.to_json();
    resp.body.assign(json_data.begin(), json_data.end());
  }
  
  return resp;
}

// 5. 在 dispatch_command 中添加新命令的路由
  case protocol::Command::SAVE_REVIEW_DRAFT:
    return handle_save_review_draft(msg, session_id);
  case protocol::Command::GET_REVIEW_DRAFT:
    return handle_get_review_draft(msg, session_id);

