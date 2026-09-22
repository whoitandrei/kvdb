#include "console_widget.hpp"

#include "kvdb_client.hpp"

#include <QAbstractItemView>
#include <QCompleter>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QVBoxLayout>

namespace {

constexpr int kMaxLogLines = 5000;

const QString kColorRequest = QStringLiteral("#378ADD");
const QString kColorOk = QStringLiteral("#1D9E75");
const QString kColorServerError = QStringLiteral("#BA7517");
const QString kColorNetworkError = QStringLiteral("#E24B4A");
const QString kColorNote = QStringLiteral("#888780");

} // namespace

ConsoleWidget::ConsoleWidget(KvdbClient& client, QWidget* parent)
    : QWidget(parent), client_(client), log_(new QPlainTextEdit(this)),
      input_(new QLineEdit(this)) {

    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    log_->setReadOnly(true);
    log_->setMaximumBlockCount(kMaxLogLines);
    log_->setFont(mono);

    input_->setPlaceholderText(tr("command, e.g. SET key value"));
    input_->setFont(mono);
    input_->installEventFilter(this);

    auto* completer = new QCompleter(QStringList{QStringLiteral("SET "), QStringLiteral("GET "),
                                                 QStringLiteral("DEL "), QStringLiteral("PING"),
                                                 QStringLiteral("DBSIZE"), QStringLiteral("INFO"),
                                                 QStringLiteral("SCAN 0 100")},
                                     this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    input_->setCompleter(completer);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(log_);
    layout->addWidget(input_);

    connect(input_, &QLineEdit::returnPressed, this, &ConsoleWidget::sendCurrentCommand);

    setConnected(false);
}

void ConsoleWidget::setConnected(bool connected) {
    input_->setEnabled(connected);
    if (connected) {
        input_->setFocus();
    }
}

void ConsoleWidget::appendColored(const QString& text, const QString& color) {
    log_->appendHtml(
        QStringLiteral("<span style=\"color:%1\">%2</span>").arg(color, text.toHtmlEscaped()));
}

void ConsoleWidget::appendNote(const QString& text) {
    appendColored(text, kColorNote);
}

void ConsoleWidget::sendCurrentCommand() {
    const QString command = input_->text().trimmed();
    if (command.isEmpty()) {
        return;
    }

    if (history_.isEmpty() || history_.last() != command) {
        history_.append(command);
    }
    history_pos_ = history_.size();
    input_->clear();

    appendColored(QStringLiteral("> ") + command, kColorRequest);

    const Reply reply = client_.exec(command);

    if (!reply.ok) {
        appendColored(QStringLiteral("! ") + reply.error, kColorNetworkError);
        return;
    }

    const QString& color = reply.isServerError() ? kColorServerError : kColorOk;
    appendColored(reply.header, color);
    for (const QString& item : reply.items) {
        appendColored(QStringLiteral("  ") + item, color);
    }
    appendNote(tr("  %1 us").arg(reply.rtt_micros));
}

bool ConsoleWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched != input_ || event->type() != QEvent::KeyPress) {
        return QWidget::eventFilter(watched, event);
    }

    if (QCompleter* completer = input_->completer();
        completer != nullptr && completer->popup()->isVisible()) {
        return QWidget::eventFilter(watched, event);
    }

    const auto* key = static_cast<QKeyEvent*>(event);

    if (key->key() == Qt::Key_Up) {
        if (history_pos_ > 0) {
            input_->setText(history_.at(--history_pos_));
        }
        return true;
    }

    if (key->key() == Qt::Key_Down) {
        if (history_pos_ < history_.size() - 1) {
            input_->setText(history_.at(++history_pos_));
        } else {
            history_pos_ = history_.size();
            input_->clear();
        }
        return true;
    }

    return QWidget::eventFilter(watched, event);
}