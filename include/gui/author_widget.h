#ifndef AUTHOR_WIDGET_H
#define AUTHOR_WIDGET_H

#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QString>
#include <QWidget>
#include <functional>

namespace gui {

class AuthorWidget : public QWidget {
  Q_OBJECT

public:
  explicit AuthorWidget(QWidget *parent = nullptr);

  void set_username(const QString &username);
  void refresh_papers();

signals:
  void upload_paper_requested();
  void view_status_requested(const QString &paper_id);
  void logout_requested();

private:
  void setup_ui();

  QLabel *welcome_label_;
  QPushButton *upload_button_;
  QPushButton *refresh_button_;
  QPushButton *logout_button_;
  QListWidget *papers_list_;
  QString username_;
};

} // namespace gui

#endif // AUTHOR_WIDGET_H
