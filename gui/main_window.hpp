#pragma once

#include "kvdb_client.hpp"

#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QSpinBox>


class MainWindow : public QMainWindow {
    Q_OBJECT
  public:
    explicit MainWindow(QWidget* parent = nullptr);

  private slots:
    void toggleConnection();
    void onConnectionLost();

  private:
    void applyConnectedState(bool connected);

  private:
    QLineEdit* host_ = nullptr;
    QSpinBox* port_ = nullptr;
    QPushButton* connect_button_ = nullptr;
    KvdbClient* client_ = nullptr;
    // QConsoleWidget 
};