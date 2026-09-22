#include "main_window.hpp"

#include <QStatusBar>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {
constexpr int kMainWindowHeight = 400;
constexpr int kMainWindowWidth = 600;
} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    client_ = new KvdbClient(this);
    auto* central = new QWidget(this);
    auto* layoutGeneral = new QVBoxLayout(central);

    auto* toolbarLayout = new QHBoxLayout();
    host_ = new QLineEdit("localhost");
    port_ = new QSpinBox();
    port_->setRange(1, 65535);
    port_->setValue(8888);
    connect_button_ = new QPushButton();

    toolbarLayout->addWidget(host_);
    toolbarLayout->addWidget(port_);
    toolbarLayout->addWidget(connect_button_);

    layoutGeneral->addLayout(toolbarLayout);

    auto* tab = new QTabWidget(central);
    console_ = new ConsoleWidget(*client_, this);
    tab->addTab(console_, "console");
    layoutGeneral->addWidget(tab);

    setCentralWidget(central);
    setWindowTitle("kvdb gui");
    resize(kMainWindowWidth, kMainWindowHeight);

    applyConnectedState(false);

    connect(connect_button_, &QPushButton::clicked, this, &MainWindow::toggleConnection);
    connect(client_, &KvdbClient::connectionLost, this, &MainWindow::onConnectionLost);
}

void MainWindow::toggleConnection() {
    if (client_->isConnected()) {
        client_->disconnectFromServer();
        applyConnectedState(false);
        return;
    }

    QString error;
    if (!client_->connectToServer(host_->text(), static_cast<quint16>(port_->value()), &error)) {
        statusBar()->showMessage("[ERROR] " + error);
        return;
    }
    applyConnectedState(true);
}

void MainWindow::onConnectionLost() {
    applyConnectedState(false);
    statusBar()->showMessage("[FATAL] connection lost");
}

void MainWindow::applyConnectedState(bool connected) {
    connect_button_->setText(connected ? "disconnect" : "connect");
    port_->setEnabled(!connected);
    host_->setEnabled(!connected);
    console_->setConnected(connected);
    statusBar()->showMessage(connected ? "[INFO] connected" : "[INFO] disconnected");
}