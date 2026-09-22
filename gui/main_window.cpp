#include "main_window.hpp"

#include <QStatusBar>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {
constexpr int kMainWindowHeight = 400;
constexpr int kMainWindowWidth = 600;
} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
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
    tab->addTab(new QWidget, "console");
    layoutGeneral->addWidget(tab);

    setCentralWidget(central);
    setWindowTitle("kvdb gui");
    resize(kMainWindowWidth, kMainWindowHeight);

    applyConnectedState(connected_);

    connect(connect_button_, &QPushButton::clicked, this, &MainWindow::toggleConnection);
}

void MainWindow::toggleConnection() {
    connected_ = !connected_;
    applyConnectedState(connected_);
}

void MainWindow::applyConnectedState(bool connected) {
    connect_button_->setText(connected ? "disconnect" : "connect");
    port_->setEnabled(!connected);
    host_->setEnabled(!connected);
    statusBar()->showMessage(connected ? "[INFO] connected" : "[INFO] disconnected");
}
