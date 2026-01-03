#include "gui/author_widget.h"
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>

namespace gui {

AuthorWidget::AuthorWidget(QWidget *parent) : QWidget(parent) { setup_ui(); }

void AuthorWidget::set_username(const QString &username) {
  username_ = username;
  welcome_label_->setText(QString("欢迎, %1 (作者)").arg(username));
}

void AuthorWidget::setup_ui() {
  auto *main_layout = new QVBoxLayout(this);
  main_layout->setSpacing(15);
  main_layout->setContentsMargins(20, 20, 20, 20);

  // Header
  auto *header_layout = new QHBoxLayout();
  welcome_label_ = new QLabel("欢迎, 作者", this);
  QFont header_font = welcome_label_->font();
  header_font.setPointSize(16);
  header_font.setBold(true);
  welcome_label_->setFont(header_font);
  welcome_label_->setStyleSheet("color: #2c3e50;");

  logout_button_ = new QPushButton("退出登录", this);
  logout_button_->setStyleSheet(
      "QPushButton {"
      "   background-color: #e74c3c;"
      "   color: white;"
      "   border: none;"
      "   border-radius: 4px;"
      "   padding: 8px 15px;"
      "}"
      "QPushButton:hover { background-color: #c0392b; }");

  header_layout->addWidget(welcome_label_);
  header_layout->addStretch();
  header_layout->addWidget(logout_button_);

  // Action buttons
  auto *button_layout = new QHBoxLayout();
  upload_button_ = new QPushButton("📄 上传论文", this);
  upload_button_->setMinimumHeight(40);
  upload_button_->setStyleSheet(
      "QPushButton {"
      "   background-color: #27ae60;"
      "   color: white;"
      "   border: none;"
      "   border-radius: 4px;"
      "   padding: 10px 20px;"
      "   font-size: 14px;"
      "   font-weight: bold;"
      "}"
      "QPushButton:hover { background-color: #229954; }");

  refresh_button_ = new QPushButton("🔄 刷新", this);
  refresh_button_->setMinimumHeight(40);
  refresh_button_->setStyleSheet(
      "QPushButton {"
      "   background-color: #3498db;"
      "   color: white;"
      "   border: none;"
      "   border-radius: 4px;"
      "   padding: 10px 20px;"
      "   font-size: 14px;"
      "}"
      "QPushButton:hover { background-color: #2980b9; }");

  button_layout->addWidget(upload_button_);
  button_layout->addWidget(refresh_button_);
  button_layout->addStretch();

  // Papers list
  auto *list_group = new QGroupBox("我的论文", this);
  auto *list_layout = new QVBoxLayout(list_group);

  papers_list_ = new QListWidget(this);
  papers_list_->setStyleSheet("QListWidget {"
                              "   border: 2px solid #bdc3c7;"
                              "   border-radius: 4px;"
                              "   background-color: white;"
                              "   padding: 5px;"
                              "}"
                              "QListWidget::item {"
                              "   padding: 10px;"
                              "   border-bottom: 1px solid #ecf0f1;"
                              "}"
                              "QListWidget::item:hover {"
                              "   background-color: #ecf0f1;"
                              "}"
                              "QListWidget::item:selected {"
                              "   background-color: #3498db;"
                              "   color: white;"
                              "}");

  list_layout->addWidget(papers_list_);

  // Assembly
  main_layout->addLayout(header_layout);
  main_layout->addLayout(button_layout);
  main_layout->addWidget(list_group, 1);

  // Connections
  connect(upload_button_, &QPushButton::clicked, this,
          &AuthorWidget::upload_paper_requested);
  connect(refresh_button_, &QPushButton::clicked, this,
          &AuthorWidget::refresh_papers);
  connect(logout_button_, &QPushButton::clicked, this,
          &AuthorWidget::logout_requested);

  setStyleSheet("QWidget { background-color: #ecf0f1; }");
}

void AuthorWidget::refresh_papers() {
  // TODO: Fetch papers from server
  papers_list_->clear();
  papers_list_->addItem("📄 P1: 我的第一篇论文 - 状态: 待审");
  papers_list_->addItem("📄 P2: 操作系统研究 - 状态: 审稿中");
}

} // namespace gui
