#include "AiChatFloatingInputBar.h"

#include <QAbstractButton>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>
#include <QResizeEvent>
#include <QTextCursor>
#include <QTextOption>

#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/TransparentTextEdit.h"

namespace {

constexpr int kLightPanelAlpha = 100;
constexpr int kDarkPanelAlpha = 150;

class IconOnlyButton : public QAbstractButton
{
public:
    explicit IconOnlyButton(QWidget* parent = nullptr)
        : QAbstractButton(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
    }

    void setIconPixmap(const QPixmap& pixmap)
    {
        m_pixmap = pixmap;
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);
        if (m_pixmap.isNull()) {
            return;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        const int side = qMin(width(), height());
        const QRect target((width() - side) / 2, (height() - side) / 2, side, side);
        painter.drawPixmap(target, m_pixmap);
    }

private:
    QPixmap m_pixmap;
};

QString submittedText(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text.trimmed();
}

} // namespace

AiChatFloatingInputBar::AiChatFloatingInputBar(QWidget* parent)
    : QWidget(parent)
    , m_inputEdit(new TransparentTextEdit(this))
    , m_actionButton(new IconOnlyButton(this))
{
    setAutoFillBackground(false);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
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
    m_inputEdit->setViewportPadding(kInputTextHorizontalPadding,
                                    kInputTextTopPadding,
                                    kInputTextHorizontalPadding,
                                    kInputTextBottomPadding);
    m_inputEdit->viewport()->setCursor(Qt::IBeamCursor);
    m_inputEdit->installEventFilter(this);
    m_inputEdit->viewport()->installEventFilter(this);
    setFocusProxy(m_inputEdit);

    connect(m_actionButton, &QAbstractButton::clicked,
            this, &AiChatFloatingInputBar::onActionButtonClicked);

    QFont font = m_inputEdit->font();
    font.setPixelSize(14);
    font.setHintingPreference(QFont::PreferFullHinting);
    font.setStyleStrategy(QFont::PreferAntialias);
    m_inputEdit->setFont(font);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(30);
    shadow->setOffset(0, 0);
    shadow->setColor(ThemeManager::instance().color(ThemeColor::FloatingPanelShadow));
    setGraphicsEffect(shadow);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &AiChatFloatingInputBar::applyTheme);

    applyTheme();
    updateActionButtonIcon();
    updateInputGeometry();
}

void AiChatFloatingInputBar::focusInput()
{
    m_inputEdit->setFocus();
    m_inputEdit->moveCursor(QTextCursor::End);
}

void AiChatFloatingInputBar::setStreaming(bool streaming)
{
    if (m_streaming == streaming) {
        return;
    }

    m_streaming = streaming;
    updateActionButtonIcon();
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
    background.setAlpha(ThemeManager::instance().isDark() ? kDarkPanelAlpha : kLightPanelAlpha);
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
    palette.setColor(QPalette::HighlightedText, ThemeManager::instance().color(ThemeColor::TextOnAccent));
    m_inputEdit->setPalette(palette);

    update();
}

void AiChatFloatingInputBar::onActionButtonClicked()
{
    if (m_streaming) {
        emit stopStreamingRequested();
        return;
    }

    sendCurrentText();
}

void AiChatFloatingInputBar::sendCurrentText()
{
    if (m_streaming) {
        return;
    }

    const QString text = submittedText(m_inputEdit->toPlainText());
    if (text.isEmpty()) {
        return;
    }

    m_inputEdit->clear();
    emit sendText(text);
    focusInput();
}

void AiChatFloatingInputBar::updateActionButtonIcon()
{
    const QString iconPath = m_streaming
            ? QStringLiteral(":/resources/icon/offline.png")
            : QStringLiteral(":/resources/icon/book_and_pen.png");

    if (auto* iconButton = dynamic_cast<IconOnlyButton*>(m_actionButton)) {
        iconButton->setIconPixmap(ImageService::instance().scaled(iconPath,
                                                                   QSize(kActionButtonSize, kActionButtonSize),
                                                                   Qt::KeepAspectRatio,
                                                                   devicePixelRatioF()));
    }
}

void AiChatFloatingInputBar::updateInputGeometry()
{
    m_inputEdit->setGeometry(kInputLeftMargin,
                             kInputTopMargin,
                             qMax(0, width() - kInputLeftMargin - kInputRightPadding),
                             qMax(0, height() - kInputTopMargin - kInputBottomMargin));
    m_actionButton->setGeometry(qMax(0, width() - kActionButtonRightMargin - kActionButtonSize),
                                qMax(0, height() - kActionButtonBottomMargin - kActionButtonSize),
                                kActionButtonSize,
                                kActionButtonSize);
}
