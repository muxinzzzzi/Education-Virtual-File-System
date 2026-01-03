#include "gui/login_widget.h"
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace gui {

LoginWidget::LoginWidget(QWidget *parent) : QWidget(parent) { setup_ui(); }

void LoginWidget::setup_ui() {
  auto *main_layout = new QVBoxLayout(this);
  main_layout->setSpacing(20);
  main_layout->setContentsMargins(50, 50, 50, 50);

  // Title
  auto *title_label = new QLabel("科研审稿系统", this);
  QFont title_font = title_label->font();
  title_font.setPointSize(24);
  title_font.setBold(true);
  title_label->setFont(title_font);
  title_label->setAlignment(Qt::AlignCenter);
  title_label->setStyleSheet("color: #2c3e50; margin-bottom: 20px;");

  // Server settings group
  auto *server_group = new QGroupBox("服务器设置", this);
  auto *server_layout = new QFormLayout(server_group);

  host_edit_ = new QLineEdit("127.0.0.1", this);
  host_edit_->setPlaceholderText("服务器地址");
  host_edit_->setStyleSheet(
      "padding: 8px; border: 2px solid #bdc3c7; border-radius: 4px;");

  port_edit_ = new QLineEdit("8080", this);
  port_edit_->setPlaceholderText("端口");
  port_edit_->setStyleSheet(
      "padding: 8px; border: 2px solid #bdc3c7; border-radius: 4px;");

  server_layout->addRow("服务器:", host_edit_);
  server_layout->addRow("端口:", port_edit_);

  // Login group
  auto *login_group = new QGroupBox("登录信息", this);
  auto *login_layout = new QFormLayout(login_group);

  username_edit_ = new QLineEdit(this);
  username_edit_->setPlaceholderText("用户名");
  username_edit_->setStyleSheet(
      "padding: 8px; border: 2px solid #bdc3c7; border-radius: 4px;");

  password_edit_ = new QLineEdit(this);
  password_edit_->setEchoMode(QLineEdit::Password);
  password_edit_->setPlaceholderText("密码");
  password_edit_->setStyleSheet(
      "padding: 8px; border: 2px solid #bdc3c7; border-radius: 4px;");

  login_layout->addRow("用户名:", username_edit_);
  login_layout->addRow("密码:", password_edit_);

  // Login button
  login_button_ = new QPushButton("登录", this);
  login_button_->setMinimumHeight(40);
  login_button_->setStyleSheet("QPushButton {"
                               "   background-color: #3498db;"
                               "   color: white;"
                               "   border: none;"
                               "   border-radius: 4px;"
                               "   padding: 10px;"
                               "   font-size: 14px;"
                               "   font-weight: bold;"
                               "}"
                               "QPushButton:hover {"
                               "   background-color: #2980b9;"
                               "}"
                               "QPushButton:pressed {"
                               "   background-color: #21618c;"
                               "}");

  // Status label
  status_label_ = new QLabel(this);
  status_label_->setAlignment(Qt::AlignCenter);
  status_label_->setStyleSheet("color: #e74c3c; font-size: 12px;");
  status_label_->setWordWrap(true);

  // Demo accounts hint
  auto *hint_label = new QLabel("<p style='color: #7f8c8d; font-size: 11px;'>"
                                "<b>测试账户:</b><br>"
                                "管理员: admin / admin123<br>"
                                "作者: alice / password<br>"
                                "审稿人: bob / password<br>"
                                "编辑: charlie / password"
                                "</p>",
                                this);
  hint_label->setAlignment(Qt::AlignCenter);

  // Add to main layout
  main_layout->addStretch();
  main_layout->addWidget(title_label);
  main_layout->addWidget(server_group);
  main_layout->addWidget(login_group);
  main_layout->addWidget(login_button_);
  main_layout->addWidget(status_label_);
  main_layout->addWidget(hint_label);
  main_layout->addStretch();

  // Connections
  connect(login_button_, &QPushButton::clicked, this,
          &LoginWidget::login_requested);
  connect(password_edit_, &QLineEdit::returnPressed, this,
          &LoginWidget::login_requested);

  // Set background
  setStyleSheet("QWidget { background-color: #ecf0f1; }");
}

} // namespace gui
