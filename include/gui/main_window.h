#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include "common/protocol.h"
#include <QMainWindow>
#include <QStackedWidget>
#include <QString>
#include <QTcpSocket>
#include <memory>

namespace gui {

class LoginWidget;
class AuthorWidget;
class ReviewerWidget;
class EditorWidget;
class AdminWidget;

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

private slots:
  void on_login_success(const QString &session_id, protocol::Role role,
                        const QString &username);
  void on_logout();
  void on_socket_error();
  void on_socket_connected();

private:
  void setup_ui();
  void connect_to_server(const QString &host, int port);
  bool send_message(const protocol::Message &msg);
  bool receive_response(protocol::Response &resp);

  QStackedWidget *stack_widget_;
  LoginWidget *login_widget_;
  AuthorWidget *author_widget_;
  ReviewerWidget *reviewer_widget_;
  EditorWidget *editor_widget_;
  AdminWidget *admin_widget_;

  QTcpSocket *socket_;
  QString session_id_;
  protocol::Role current_role_;
  QString username_;
  bool connected_;

  static constexpr int LOGIN_PAGE = 0;
  static constexpr int AUTHOR_PAGE = 1;
  static constexpr int REVIEWER_PAGE = 2;
  static constexpr int EDITOR_PAGE = 3;
  static constexpr int ADMIN_PAGE = 4;
};

} // namespace gui

#endif // MAIN_WINDOW_H
