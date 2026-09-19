#include "main_window.hpp"

#include <QPushButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {
constexpr int kMainWindowHeight = 400;
constexpr int kMainWindowWidth = 600;
}; // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);

    auto* tab = new QTabWidget(central);
    tab->addTab(new QWidget, "console");
    layout->addWidget(tab);

    setCentralWidget(central);
    setWindowTitle("kvdb gui");
    resize(kMainWindowWidth, kMainWindowHeight);
    // statusBar()->showMessage("ready");
}
