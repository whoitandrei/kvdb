#pragma once

#include "kvdb_client.hpp"

#include <QLineEdit>
#include <QPlainTextEdit>
#include <QWidget>

class ConsoleWidget : public QWidget {
    Q_OBJECT
  public:
    explicit ConsoleWidget(KvdbClient& client, QWidget* parent = nullptr);

  public slots:
    void setConnected(bool connected);
    void appendNote(const QString& text);

  private slots:
    void sendCurrentCommand();

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    void appendColored(const QString& text, const QString& color);

    KvdbClient& client_;
    QPlainTextEdit* log_;
    QLineEdit* input_;
    QStringList history_;
    int history_pos_ = 0;
};
