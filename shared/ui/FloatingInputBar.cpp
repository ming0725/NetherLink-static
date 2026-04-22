#include "FloatingInputBar.h"
#include <QEvent>
#include <QFileDialog>
#include <QGraphicsDropShadowEffect>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QStandardPaths>
#include <QTextCursor>
#include <QTextOption>
#include <QVBoxLayout>

#ifdef Q_OS_MACOS
#include "platform/macos/MacFloatingInputBarBridge_p.h"
#endif

namespace {

constexpr int kQtMode3ToolbarInputOverlap = -4;

} // namespace

const QString FloatingInputBar::RESOURCE_PATH = ":/resources/icon/";

FloatingInputBar::FloatingInputBar(QWidget *parent)
        : QWidget(parent)
{
    setAutoFillBackground(false);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::FramelessWindowHint);
    setFocusPolicy(Qt::StrongFocus);

#ifdef Q_OS_MACOS
    const auto nativeAppearance = MacFloatingInputBarBridge::appearance();
    m_usesNativeGlass = nativeAppearance != MacFloatingInputBarBridge::Appearance::Unsupported;
    m_usesNativeInput = m_usesNativeGlass;
#endif

    if (!m_usesNativeInput) {
        initQtFallbackUi();
    }

    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(30);
    shadow->setOffset(0, 0);
    shadow->setColor(QColor(150, 150, 150, 220));
    setGraphicsEffect(shadow);
}

FloatingInputBar::~FloatingInputBar()
{
#ifdef Q_OS_MACOS
    if (m_usesNativeGlass) {
        MacFloatingInputBarBridge::clearInputBar(this);
    }
#endif
}

void FloatingInputBar::focusInput()
{
    if (m_inputEdit) {
        m_inputEdit->setFocus();
        m_inputEdit->moveCursor(QTextCursor::End);
        return;
    }

#ifdef Q_OS_MACOS
    if (m_usesNativeInput) {
        MacFloatingInputBarBridge::focusInputBar(this);
    }
#endif
}

void FloatingInputBar::initQtFallbackUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, VERTICAL_MARGIN, 0, VERTICAL_MARGIN);
    mainLayout->setSpacing(TOOLBAR_INPUT_SPACING);

    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setContentsMargins(TOOLBAR_LEFT_MARGIN, 0, RIGHT_MARGIN + 4, 0);
    toolbarLayout->setSpacing(BUTTON_SPACING);

    m_emojiLabel = new QLabel(this);
    m_imageLabel = new QLabel(this);
    m_screenshotLabel = new QLabel(this);
    m_historyLabel = new QLabel(this);

    const QSize buttonSize(BUTTON_SIZE, BUTTON_SIZE);
    m_emojiLabel->setFixedSize(buttonSize);
    m_imageLabel->setFixedSize(buttonSize);
    m_screenshotLabel->setFixedSize(buttonSize);
    m_historyLabel->setFixedSize(buttonSize);

    const QSize iconSize(ICON_SIZE, ICON_SIZE);
    updateLabelIcon(m_emojiLabel, "skull.png", "hovered_skull.png", iconSize);
    updateLabelIcon(m_imageLabel, "painting.png", "hovered_painting.png", iconSize);
    updateLabelIcon(m_screenshotLabel, "shears.png", "hovered_shears.png", iconSize);
    updateLabelIcon(m_historyLabel, "clock.png", "hovered_clock.png", iconSize);

    const QList<QLabel*> toolbarLabels {m_emojiLabel, m_imageLabel, m_screenshotLabel, m_historyLabel};
    for (QLabel* label : toolbarLabels) {
        label->setCursor(Qt::PointingHandCursor);
        label->installEventFilter(this);
    }

    toolbarLayout->addWidget(m_emojiLabel);
    toolbarLayout->addWidget(m_imageLabel);
    toolbarLayout->addWidget(m_screenshotLabel);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(m_historyLabel);

    QHBoxLayout *inputLayout = new QHBoxLayout();
    inputLayout->setContentsMargins(INPUT_LEFT_MARGIN, kQtMode3ToolbarInputOverlap, 5, 0);
    inputLayout->setSpacing(0);

    m_inputEdit = new QTextEdit(this);
    m_inputEdit->setAcceptRichText(false);
    m_inputEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_inputEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_inputEdit->setMinimumHeight(60);
    m_inputEdit->setMaximumHeight(120);
    m_inputEdit->setStyleSheet(
            "QTextEdit {"
            "   background-color: transparent;"
            "   border: none;"
            "   color: #333333;"
            "   font-size: 15px;"
            "   padding: 5px;"
            "   selection-background-color: #3064CE;"
            "   selection-color: white;"
            "}"
            "QTextEdit::cursor {"
            "   border-left: 2px solid #3064CE;"
            "}"
            "QScrollBar:vertical {"
            "   border: none;"
            "   background: transparent;"
            "   width: 8px;"
            "   margin: 0px;"
            "}"
            "QScrollBar::handle:vertical {"
            "   background: rgba(0, 0, 0, 0.2);"
            "   border-radius: 4px;"
            "   min-height: 20px;"
            "}"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
            "   height: 0px;"
            "}"
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
            "   background: none;"
            "}"
    );
    m_inputEdit->installEventFilter(this);
    setFocusProxy(m_inputEdit);

    QFont font = m_inputEdit->font();
    font.setHintingPreference(QFont::PreferFullHinting);
    font.setStyleStrategy(QFont::PreferAntialias);
    m_inputEdit->setFont(font);

    inputLayout->addWidget(m_inputEdit);

    mainLayout->addLayout(toolbarLayout);
    mainLayout->addLayout(inputLayout);
    setLayout(mainLayout);

    m_sendLabel = new QLabel(this);
    m_sendLabel->setFixedSize(SEND_BUTTON_SIZE, SEND_BUTTON_SIZE);
    m_sendLabel->setCursor(Qt::PointingHandCursor);
    updateLabelIcon(m_sendLabel,
                    "book_and_pen.png",
                    "hovered_book_and_pen.png",
                    QSize(SEND_BUTTON_SIZE, SEND_BUTTON_SIZE));
    m_sendLabel->installEventFilter(this);
    m_sendLabel->raise();

    m_tooltip = new CustomTooltip(this);
    m_tooltip->hide();
}

void FloatingInputBar::updateLabelIcon(QLabel *label,
                                       const QString &normalIcon,
                                       const QString &hoveredIcon,
                                       const QSize &size)
{
    if (!label) {
        return;
    }

    QPixmap normalPixmap(RESOURCE_PATH + normalIcon);
    if (normalPixmap.isNull()) {
        label->clear();
        return;
    }

    const qreal ratio = qMin(size.width() / static_cast<qreal>(normalPixmap.width()),
                             size.height() / static_cast<qreal>(normalPixmap.height()));
    const QSize targetSize(normalPixmap.width() * ratio, normalPixmap.height() * ratio);
    QPixmap finalIcon(size);
    finalIcon.fill(Qt::transparent);

    QPainter painter(&finalIcon);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawPixmap((size.width() - targetSize.width()) / 2,
                       (size.height() - targetSize.height()) / 2,
                       normalPixmap.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    label->setProperty("hoveredIcon", RESOURCE_PATH + hoveredIcon);
    label->setProperty("normalIcon", RESOURCE_PATH + normalIcon);
    label->setProperty("iconSize", size);
    label->setPixmap(finalIcon);
}

void FloatingInputBar::handleLabelHover(QLabel *label,
                                        const QString &normalIcon,
                                        const QString &hoveredIcon,
                                        const QSize &size)
{
    if (!label) {
        return;
    }

    QPixmap pixmap(hoveredIcon);
    if (pixmap.isNull()) {
        return;
    }

    const qreal ratio = qMin(size.width() / static_cast<qreal>(pixmap.width()),
                             size.height() / static_cast<qreal>(pixmap.height()));
    const QSize targetSize(pixmap.width() * ratio, pixmap.height() * ratio);
    QPixmap finalIcon(size);
    finalIcon.fill(Qt::transparent);

    QPainter painter(&finalIcon);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawPixmap((size.width() - targetSize.width()) / 2,
                       (size.height() - targetSize.height()) / 2,
                       pixmap.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    label->setPixmap(finalIcon);
}

void FloatingInputBar::showTooltip(QLabel *label, const QString &text)
{
    if (!m_tooltip || !label) {
        return;
    }

    m_tooltip->setText(text);
    const QPoint labelPos = label->mapToGlobal(QPoint(0, 0));
    const QPoint tooltipPos(labelPos.x() + label->width() / 2 - m_tooltip->width() / 2,
                            labelPos.y());
    m_tooltip->showTooltip(tooltipPos);
}

void FloatingInputBar::hideTooltip()
{
    if (m_tooltip) {
        m_tooltip->hide();
    }
}

bool FloatingInputBar::submitNativeText(const QString& text)
{
    const QString trimmedText = text.trimmed();
    if (trimmedText.isEmpty()) {
        return false;
    }

    emit sendText(trimmedText);
    return true;
}

void FloatingInputBar::triggerNativeHelloShortcut()
{
    sendHelloWorld();
}

void FloatingInputBar::triggerNativeImageShortcut()
{
    chooseAndSendImage();
}

bool FloatingInputBar::event(QEvent *event)
{
    const bool handled = QWidget::event(event);

#ifdef Q_OS_MACOS
    if (!m_usesNativeGlass) {
        return handled;
    }

    switch (event->type()) {
    case QEvent::Show:
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::WinIdChange:
    case QEvent::ParentChange:
    case QEvent::ZOrderChange:
        syncPlatformInput();
        break;
    case QEvent::Hide:
        MacFloatingInputBarBridge::clearInputBar(this);
        break;
    default:
        break;
    }
#endif

    return handled;
}

void FloatingInputBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    if (m_usesNativeGlass) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setOpacity(m_visualOpacity);
    QPainterPath path;
    path.addRoundedRect(rect(), CORNER_RADIUS, CORNER_RADIUS);
    painter.fillPath(path, QColor(255, 255, 255, 100));
}

void FloatingInputBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    if (m_sendLabel) {
        m_sendLabel->move(width() - SEND_BUTTON_SIZE - RIGHT_MARGIN,
                          height() - SEND_BUTTON_SIZE - VERTICAL_MARGIN);
        m_sendLabel->raise();
    }
}

bool FloatingInputBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_inputEdit) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_BracketLeft && keyEvent->modifiers() == Qt::NoModifier) {
                sendCurrentMessage(SendMode::Peer);
                return true;
            }
            if (keyEvent->key() == Qt::Key_BracketRight && keyEvent->modifiers() == Qt::NoModifier) {
                sendHelloWorld();
                return true;
            }
            if (keyEvent->key() == Qt::Key_Backslash && keyEvent->modifiers() == Qt::NoModifier) {
                chooseAndSendImage();
                return true;
            }
            if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
                && keyEvent->modifiers() == Qt::NoModifier) {
                sendCurrentMessage();
                return true;
            }
        }
    }

    QLabel* label = qobject_cast<QLabel*>(watched);
    if (label) {
        if (event->type() == QEvent::Enter) {
            const QString hoveredIcon = label->property("hoveredIcon").toString();
            const QString normalIcon = label->property("normalIcon").toString();
            const QSize iconSize = label->property("iconSize").toSize();
            handleLabelHover(label, normalIcon, hoveredIcon, iconSize);

            if (label == m_emojiLabel) {
                showTooltip(label, QStringLiteral("表情"));
            } else if (label == m_imageLabel) {
                showTooltip(label, QStringLiteral("图片"));
            } else if (label == m_screenshotLabel) {
                showTooltip(label, QStringLiteral("截图"));
            } else if (label == m_historyLabel) {
                showTooltip(label, QStringLiteral("历史"));
            } else if (label == m_sendLabel) {
                showTooltip(label, QStringLiteral("发送"));
            }
        } else if (event->type() == QEvent::Leave) {
            const QString hoveredIcon = label->property("hoveredIcon").toString();
            const QString normalIcon = label->property("normalIcon").toString();
            const QSize iconSize = label->property("iconSize").toSize();
            handleLabelHover(label, hoveredIcon, normalIcon, iconSize);
            hideTooltip();
        }
    }

    return QWidget::eventFilter(watched, event);
}

void FloatingInputBar::mousePressEvent(QMouseEvent *event)
{
    if (m_sendLabel && m_sendLabel->geometry().contains(event->pos())) {
        sendCurrentMessage();
    } else if (m_imageLabel && m_imageLabel->geometry().contains(event->pos())) {
        chooseAndSendImage();
    }

    focusInput();
    QWidget::mousePressEvent(event);
}


void FloatingInputBar::sendCurrentMessage(SendMode mode)
{
    if (!m_inputEdit) {
        return;
    }

    QString text = m_inputEdit->toPlainText().trimmed();
    if (!text.isEmpty()) {
        if (mode == SendMode::Peer) {
            emit sendTextAsPeer(text);
        } else {
            emit sendText(text);
        }
        m_inputEdit->clear();
    }
}

void FloatingInputBar::chooseAndSendImage()
{
    const QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    const QString filePath = QFileDialog::getOpenFileName(this,
                                                          tr("选择图片"),
                                                          desktopPath,
                                                          tr("图片文件 (*.png *.jpg *.jpeg *.bmp)"));
    if (!filePath.isEmpty()) {
        emit sendImage(filePath);
    }
}

void FloatingInputBar::sendHelloWorld()
{
    emit sendText(QStringLiteral("Hello World"));
}

void FloatingInputBar::syncPlatformInput()
{
#ifdef Q_OS_MACOS
    if (m_usesNativeGlass) {
        if (m_usesNativeInput) {
            MacFloatingInputBarBridge::syncInputBar(this, QString(), m_visualOpacity);
        } else {
            MacFloatingInputBarBridge::syncGlass(this, m_visualOpacity);
        }
    }
#endif
}
