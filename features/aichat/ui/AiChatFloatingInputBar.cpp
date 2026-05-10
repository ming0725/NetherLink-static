#include "AiChatFloatingInputBar.h"

#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QResizeEvent>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextOption>

#include "shared/theme/ThemeManager.h"

namespace {

QString submittedText(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text.trimmed();
}

} // namespace

AiChatFloatingInputBar::AiChatFloatingInputBar(QWidget* parent)
    : QWidget(parent)
    , m_inputEdit(new QTextEdit(this))
{
    setAutoFillBackground(false);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::FramelessWindowHint);
    setFocusPolicy(Qt::StrongFocus);

    m_inputEdit->setFocusPolicy(Qt::StrongFocus);
    m_inputEdit->setAcceptRichText(false);
    m_inputEdit->setPlaceholderText(QStringLiteral("输入消息"));
    m_inputEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_inputEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_inputEdit->setLineWrapMode(QTextEdit::WidgetWidth);
    m_inputEdit->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_inputEdit->setMinimumHeight(60);
    m_inputEdit->document()->setDocumentMargin(0);
    m_inputEdit->viewport()->setCursor(Qt::IBeamCursor);
    m_inputEdit->installEventFilter(this);
    m_inputEdit->viewport()->installEventFilter(this);
    setFocusProxy(m_inputEdit);

    QFont font = m_inputEdit->font();
    font.setPixelSize(14);
    font.setHintingPreference(QFont::PreferFullHinting);
    font.setStyleStrategy(QFont::PreferAntialias);
    m_inputEdit->setFont(font);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(30);
    shadow->setOffset(0, 0);
    shadow->setColor(QColor(150, 150, 150, 220));
    setGraphicsEffect(shadow);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &AiChatFloatingInputBar::applyTheme);

    applyTheme();
    updateInputGeometry();
}

void AiChatFloatingInputBar::focusInput()
{
    m_inputEdit->setFocus();
    m_inputEdit->moveCursor(QTextCursor::End);
}

bool AiChatFloatingInputBar::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_inputEdit) {
        if (event->type() == QEvent::FocusIn) {
            emit inputFocused();
        } else if (event->type() == QEvent::KeyPress) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
                && keyEvent->modifiers() == Qt::NoModifier) {
                sendCurrentText();
                return true;
            }
        }
    }

    if (watched == m_inputEdit->viewport() && event->type() == QEvent::MouseButtonPress) {
        m_inputEdit->setFocus(Qt::MouseFocusReason);
    }

    return QWidget::eventFilter(watched, event);
}

void AiChatFloatingInputBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        focusInput();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void AiChatFloatingInputBar::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    path.addRoundedRect(rect(), kCornerRadius, kCornerRadius);

    QColor background = ThemeManager::instance().color(ThemeColor::PanelBackground);
    background.setAlpha(ThemeManager::instance().isDark() ? 190 : 100);
    painter.fillPath(path, background);
}

void AiChatFloatingInputBar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateInputGeometry();
}

void AiChatFloatingInputBar::applyTheme()
{
    QPalette palette = m_inputEdit->palette();
    palette.setColor(QPalette::Base, Qt::transparent);
    palette.setColor(QPalette::Window, Qt::transparent);
    palette.setColor(QPalette::Text, ThemeManager::instance().color(ThemeColor::PrimaryText));
    palette.setColor(QPalette::PlaceholderText, ThemeManager::instance().color(ThemeColor::PlaceholderText));
    palette.setColor(QPalette::Highlight, ThemeManager::instance().color(ThemeColor::Accent));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    m_inputEdit->setPalette(palette);

    m_inputEdit->setStyleSheet(QStringLiteral(
            "QTextEdit {"
            "   border: none;"
            "   background: transparent;"
            "   padding: %1px %2px %3px %2px;"
            "}"
            "QScrollBar:vertical {"
            "   border: none;"
            "   background: transparent;"
            "}"
    ).arg(kInputTextTopPadding)
     .arg(kInputTextHorizontalPadding)
     .arg(kInputTextBottomPadding));

    update();
}

void AiChatFloatingInputBar::sendCurrentText()
{
    const QString text = submittedText(m_inputEdit->toPlainText());
    if (text.isEmpty()) {
        return;
    }

    m_inputEdit->clear();
    emit sendText(text);
    focusInput();
}

void AiChatFloatingInputBar::updateInputGeometry()
{
    m_inputEdit->setGeometry(kInputLeftMargin,
                             kInputTopMargin,
                             qMax(0, width() - kInputLeftMargin - kInputRightPadding),
                             qMax(0, height() - kInputTopMargin - kInputBottomMargin));
}
