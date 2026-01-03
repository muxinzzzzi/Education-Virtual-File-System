#include "gui/main_window.h"
#include "gui/author_widget.h"
#include "gui/login_widget.h"
#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <cstring>
#include <sstream>

namespace gui {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), socket_(new QTcpSocket(this)), connected_(false),
      current_role_(protocol::Role::UNKNOWN) {
  setup_ui();

  connect(socket_, &QTcpSocket::connected, this,
          &MainWindow::on_socket_connected);
  connect(socket_, &QTcpSocket::errorOccurred, this,
          &MainWindow::on_socket_error);
}

MainWindow::~MainWindow() = default;

void MainWindow::setup_ui() {
  setWindowTitle("科研审稿系统");
  setMinimumSize(900, 600);

  // Central widget with stack
  auto *central = new QWidget(this);
  auto *layout = new QVBoxLayout(central);
  layout->setContentsMargins(0, 0, 0, 0);

  stack_widget_ = new QStackedWidget(this);

  // Create pages
  login_widget_ = new LoginWidget(this);
  author_widget_ = new AuthorWidget(this);
  // TODO: Create other role widgets

  stack_widget_->insertWidget(LOGIN_PAGE, login_widget_);
  stack_widget_->insertWidget(AUTHOR_PAGE, author_widget_);

  layout->addWidget(stack_widget_);
  setCentralWidget(central);

  // Connections
  connect(login_widget_, &LoginWidget::login_requested, [this]() {
    QString host = login_widget_->get_host();
    int port = login_widget_->get_port();
    connect_to_server(host, port);
  });

  connect(author_widget_, &AuthorWidget::logout_requested, this,
          &MainWindow::on_logout);
  connect(author_widget_, &AuthorWidget::upload_paper_requested, [this]() {
    QString file_path = QFileDialog::getOpenFileName(
        this, "选择论文文件", "", "PDF Files (*.pdf);;All Files (*)");

    if (!file_path.isEmpty()) {
      // TODO: Implement upload
      QMessageBox::information(this, "上传", "论文上传功能开发中...");
    }
  });

  stack_widget_->setCurrentIndex(LOGIN_PAGE);
}

void MainWindow::connect_to_server(const QString &host, int port) {
  socket_->connectToHost(host, port);
}

void MainWindow::on_socket_connected() {
  // Try login
  protocol::Message msg;
  msg.command = protocol::Command::LOGIN;
  msg.params["username"] = login_widget_->get_username().toStdString();
  msg.params["password"] = login_widget_->get_password().toStdString();

  if (!send_message(msg)) {
    QMessageBox::critical(this, "错误", "发送登录请求失败");
    return;
  }

  protocol::Response resp;
  if (!receive_response(resp)) {
    QMessageBox::critical(this, "错误", "接收登录响应失败");
    return;
  }

  if (resp.status != protocol::StatusCode::OK) {
    QMessageBox::warning(this, "登录失败",
                         QString::fromStdString(resp.message));
    return;
  }

  // Parse response
  std::string body_str(resp.body.begin(), resp.body.end());
  std::istringstream iss(body_str);
  std::string line;
  protocol::Role role = protocol::Role::UNKNOWN;

  while (std::getline(iss, line)) {
    size_t eq_pos = line.find('=');
    if (eq_pos != std::string::npos) {
      std::string key = line.substr(0, eq_pos);
      std::string value = line.substr(eq_pos + 1);

      if (key == "session_id") {
        session_id_ = QString::fromStdString(value);
      } else if (key == "role") {
        role = protocol::Protocol::string_to_role(value);
      }
    }
  }

  on_login_success(session_id_, role, login_widget_->get_username());
}

void MainWindow::on_login_success(const QString &session_id,
                                  protocol::Role role,
                                  const QString &username) {
  session_id_ = session_id;
  current_role_ = role;
  username_ = username;
  connected_ = true;

  // Switch to appropriate page
  switch (role) {
  case protocol::Role::AUTHOR:
    author_widget_->set_username(username);
    author_widget_->refresh_papers();
    stack_widget_->setCurrentIndex(AUTHOR_PAGE);
    break;
  // TODO: Handle other roles
  default:
    QMessageBox::information(
        this, "提示",
        QString("登录成功！角色: %1\n其他角色界面开发中...")
            .arg(QString::fromStdString(
                protocol::Protocol::role_to_string(role))));
    stack_widget_->setCurrentIndex(AUTHOR_PAGE); // Fallback
    break;
  }
}

void MainWindow::on_logout() {
  // Send logout message
  protocol::Message msg;
  msg.command = protocol::Command::LOGOUT;
  send_message(msg);

  session_id_.clear();
  current_role_ = protocol::Role::UNKNOWN;
  username_.clear();
  connected_ = false;

  socket_->disconnectFromHost();
  stack_widget_->setCurrentIndex(LOGIN_PAGE);
}

void MainWindow::on_socket_error() {
  QMessageBox::critical(
      this, "连接错误",
      QString("无法连接到服务器: %1").arg(socket_->errorString()));
}

bool MainWindow::send_message(const protocol::Message &msg) {
  auto data = protocol::Protocol::serialize_message(msg);
  qint64 sent = socket_->write(data.data(), data.size());
  socket_->flush();
  return sent == static_cast<qint64>(data.size());
}

bool MainWindow::receive_response(protocol::Response &resp) {
  // Read header
  while (!socket_->canReadLine() || socket_->bytesAvailable() < 2) {
    if (!socket_->waitForReadyRead(5000)) {
      return false;
    }
  }

  // Simplified receive implementation
  QByteArray data = socket_->readAll();
  std::vector<char> buffer(data.begin(), data.end());
  return protocol::Protocol::deserialize_response(buffer, resp);
}

} // namespace gui
