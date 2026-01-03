#ifndef LOGIN_WIDGET_H
#define LOGIN_WIDGET_H

#include "common/protocol.h"
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

namespace gui {

class LoginWidget : public QWidget {
  Q_OBJECT

public:
  explicit LoginWidget(QWidget *parent = nullptr);

  QString get_username() const { return username_edit_->text(); }
  QString get_password() const { return password_edit_->text(); }
  QString get_host() const { return host_edit_->text(); }
  int get_port() const { return port_edit_->text().toInt(); }

signals:
  void login_requested();

private:
  void setup_ui();

  QLineEdit *host_edit_;
  QLineEdit *port_edit_;
  QLineEdit *username_edit_;
  QLineEdit *password_edit_;
  QPushButton *login_button_;
  QLabel *status_label_;
};

} // namespace gui

#endif // LOGIN_WIDGET_H
