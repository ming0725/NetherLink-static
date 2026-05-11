#include "FloatingInputBar.h"
#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/TransparentTextEdit.h"

#include <QEvent>
#include <QFileDialog>
#include <QGraphicsDropShadowEffect>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QScrollBar>
#include <QStandardPaths>
#include <QTextCursor>
#include <QTextOption>

#ifdef Q_OS_MACOS
#include "platform/macos/MacFloatingInputBarBridge_p.h"
#endif

namespace {

constexpr int kQtMode3ToolbarY = 12;
constexpr int kQtMode3ToolbarInputGap = 6;
constexpr int kQtMode3InputTextTopPadding = 1;
constexpr int kQtMode3InputTextHorizontalPadding = 5;
constexpr int kQtMode3InputTextBottomPadding = 5;
constexpr int kQtMode3InputRightPadding = 5;
constexpr int kQtMode3InputBottomMargin = 10;
constexpr int kQtMode3InputMaxHeight = 120;
constexpr auto kSystemFloatingBarsSuppressedProperty = "systemFloatingBarsSuppressed";
constexpr auto kNormalIconProperty = "normalIcon";
constexpr auto kHoveredIconProperty = "hoveredIcon";

} // namespace

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
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &FloatingInputBar::applyTheme);

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
    m_emojiLabel = new QLabel(this);
    m_imageLabel = new QLabel(this);
    m_screenshotLabel = new QLabel(this);
    m_historyLabel = new QLabel(this);

    const QSize buttonSize(BUTTON_SIZE, BUTTON_SIZE);
    m_emojiLabel->setFixedSize(buttonSize);
    m_imageLabel->setFixedSize(buttonSize);
    m_screenshotLabel->setFixedSize(buttonSize);
    m_historyLabel->setFixedSize(buttonSize);
    m_emojiLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_screenshotLabel->setAlignment(Qt::AlignCenter);
    m_historyLabel->setAlignment(Qt::AlignCenter);

    const QSize iconSize(ICON_SIZE, ICON_SIZE);
    updateLabelIcon(m_emojiLabel,
                    QStringLiteral(":/resources/icon/skull.png"),
                    QStringLiteral(":/resources/icon/hovered_skull.png"),
                    iconSize);
    updateLabelIcon(m_imageLabel,
                    QStringLiteral(":/resources/icon/painting.png"),
                    QStringLiteral(":/resources/icon/hovered_painting.png"),
                    iconSize);
    updateLabelIcon(m_screenshotLabel,
                    QStringLiteral(":/resources/icon/shears.png"),
                    QStringLiteral(":/resources/icon/hovered_shears.png"),
                    iconSize);
    updateLabelIcon(m_historyLabel,
                    QStringLiteral(":/resources/icon/clock.png"),
                    QStringLiteral(":/resources/icon/hovered_clock.png"),
                    iconSize);

    const QList<QLabel*> toolbarLabels {m_emojiLabel, m_imageLabel, m_screenshotLabel, m_historyLabel};
    for (QLabel* label : toolbarLabels) {
        label->setCursor(Qt::PointingHandCursor);
        label->installEventFilter(this);
    }

    m_inputEdit = new TransparentTextEdit(this);
    m_inputEdit->setAcceptRichText(false);
    m_inputEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_inputEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_inputEdit->setMinimumHeight(60);
    m_inputEdit->setMaximumHeight(120);
    m_inputEdit->setViewportPadding(kQtMode3InputTextHorizontalPadding,
                                    kQtMode3InputTextTopPadding,
                                    kQtMode3InputTextHorizontalPadding,
                                    kQtMode3InputTextBottomPadding);
    m_inputEdit->installEventFilter(this);
    setFocusProxy(m_inputEdit);

    QFont font = m_inputEdit->font();
    font.setHintingPreference(QFont::PreferFullHinting);
    font.setStyleStrategy(QFont::PreferAntialias);
    m_inputEdit->setFont(font);

    m_sendLabel = new QLabel(this);
    m_sendLabel->setFixedSize(SEND_BUTTON_SIZE, SEND_BUTTON_SIZE);
    m_sendLabel->setCursor(Qt::PointingHandCursor);
    m_sendLabel->setAlignment(Qt::AlignCenter);
    updateLabelIcon(m_sendLabel,
                    QStringLiteral(":/resources/icon/book_and_pen.png"),
                    QStringLiteral(":/resources/icon/hovered_book_and_pen.png"),
                    QSize(SEND_BUTTON_SIZE, SEND_BUTTON_SIZE));
    m_sendLabel->installEventFilter(this);
    m_sendLabel->raise();
    updateQtFallbackGeometry();

    m_tooltip = new CustomTooltip(this);
    m_tooltip->hide();
    applyTheme();
}

void FloatingInputBar::clearQtFallbackUi()
{
    hideTooltip();

    setFocusProxy(nullptr);

    delete m_inputEdit;
    delete m_emojiLabel;
    delete m_imageLabel;
    delete m_screenshotLabel;
    delete m_historyLabel;
    delete m_sendLabel;
    delete m_tooltip;

    m_inputEdit = nullptr;
    m_emojiLabel = nullptr;
    m_imageLabel = nullptr;
    m_screenshotLabel = nullptr;
    m_historyLabel = nullptr;
    m_sendLabel = nullptr;
    m_tooltip = nullptr;
}

void FloatingInputBar::refreshPlatformAppearance()
{
#ifdef Q_OS_MACOS
    const bool shouldUseNative = MacFloatingInputBarBridge::appearance()
            != MacFloatingInputBarBridge::Appearance::Unsupported;
    const bool systemSuppressed = property(kSystemFloatingBarsSuppressedProperty).toBool();

    if (m_usesNativeGlass && !shouldUseNative) {
        MacFloatingInputBarBridge::clearInputBar(this);
        m_usesNativeGlass = false;
        m_usesNativeInput = false;
        if (!m_inputEdit) {
            initQtFallbackUi();
        }
        updateQtFallbackGeometry();
        updateGeometry();
        update();
        return;
    }

    if (!m_usesNativeGlass && shouldUseNative && systemSuppressed) {
        updateQtFallbackGeometry();
        update();
        return;
    }

    if (!m_usesNativeGlass && shouldUseNative) {
        clearQtFallbackUi();
        m_usesNativeGlass = true;
        m_usesNativeInput = true;
        syncPlatformInput();
        updateGeometry();
        update();
        return;
    }

    if (m_usesNativeGlass) {
        syncPlatformInput();
    }
#endif

    updateQtFallbackGeometry();
    update();
}

void FloatingInputBar::applyTheme()
{
#ifdef Q_OS_MACOS
    if (m_usesNativeGlass) {
        syncPlatformInput();
    }
#endif

    if (m_inputEdit) {
        QPalette inputPalette = m_inputEdit->palette();
        inputPalette.setColor(QPalette::Base, Qt::transparent);
        inputPalette.setColor(QPalette::Window, Qt::transparent);
        inputPalette.setColor(QPalette::Text, ThemeManager::instance().color(ThemeColor::PrimaryText));
        inputPalette.setColor(QPalette::PlaceholderText,
                              ThemeManager::instance().color(ThemeColor::PlaceholderText));
        inputPalette.setColor(QPalette::Highlight, ThemeManager::instance().color(ThemeColor::Accent));
        inputPalette.setColor(QPalette::HighlightedText, Qt::white);
        m_inputEdit->setPalette(inputPalette);
        m_inputEdit->update();
    }

    update();
}

void FloatingInputBar::updateLabelIcon(QLabel *label,
                                       const QString &normalIcon,
                                       const QString &hoveredIcon,
                                       const QSize &size)
{
    if (!label) {
        return;
    }

    const qreal dpr = label->devicePixelRatioF();
    label->setProperty(kNormalIconProperty, normalIcon);
    label->setProperty(kHoveredIconProperty, hoveredIcon);
    label->setProperty("iconSize", size);
    label->setPixmap(ImageService::instance().scaled(normalIcon,
                                                     size,
                                                     Qt::KeepAspectRatio,
                                                     dpr));
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
    QColor background = ThemeManager::instance().color(ThemeColor::PanelBackground);
    background.setAlpha(ThemeManager::instance().isDark() ? 190 : 100);
    painter.fillPath(path, background);
}

void FloatingInputBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateQtFallbackGeometry();
}

void FloatingInputBar::updateQtFallbackGeometry()
{
    if (!m_inputEdit) {
        return;
    }

    const int toolbarY = kQtMode3ToolbarY;
    const int emojiX = TOOLBAR_LEFT_MARGIN;
    const int imageX = emojiX + BUTTON_SIZE + BUTTON_SPACING;
    const int screenshotX = imageX + BUTTON_SIZE + BUTTON_SPACING;
    const int historyCandidateX = width() - RIGHT_MARGIN - 4 - BUTTON_SIZE;
    const int historyX = historyCandidateX > TOOLBAR_LEFT_MARGIN
            ? historyCandidateX
            : TOOLBAR_LEFT_MARGIN;
    const int inputY = toolbarY + BUTTON_SIZE + kQtMode3ToolbarInputGap;
    const int rawInputHeight = height() - inputY - kQtMode3InputBottomMargin;
    const int availableInputHeight = rawInputHeight > 0 ? rawInputHeight : 0;
    const int inputHeight = availableInputHeight < kQtMode3InputMaxHeight
            ? availableInputHeight
            : kQtMode3InputMaxHeight;
    const int rawInputWidth = width() - INPUT_LEFT_MARGIN - kQtMode3InputRightPadding;
    const int inputWidth = rawInputWidth > 0 ? rawInputWidth : 0;

    m_emojiLabel->setGeometry(emojiX, toolbarY, BUTTON_SIZE, BUTTON_SIZE);
    m_imageLabel->setGeometry(imageX, toolbarY, BUTTON_SIZE, BUTTON_SIZE);
    m_screenshotLabel->setGeometry(screenshotX, toolbarY, BUTTON_SIZE, BUTTON_SIZE);
    m_historyLabel->setGeometry(historyX, toolbarY, BUTTON_SIZE, BUTTON_SIZE);
    m_inputEdit->setGeometry(INPUT_LEFT_MARGIN, inputY, inputWidth, inputHeight);
    updateSendButtonPosition();
}

void FloatingInputBar::updateSendButtonPosition()
{
    if (m_sendLabel) {
        m_sendLabel->setGeometry(width() - SEND_BUTTON_SIZE - RIGHT_MARGIN,
                                 height() - SEND_BUTTON_SIZE - VERTICAL_MARGIN,
                                 SEND_BUTTON_SIZE,
                                 SEND_BUTTON_SIZE);
        m_sendLabel->raise();
    }
}

bool FloatingInputBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_inputEdit) {
        if (event->type() == QEvent::FocusIn) {
            emit inputFocused();
        } else if (event->type() == QEvent::KeyPress) {
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
            const QString hoveredIcon = label->property(kHoveredIconProperty).toString();
            const QSize iconSize = label->property("iconSize").toSize();
            label->setPixmap(ImageService::instance().scaled(hoveredIcon,
                                                             iconSize,
                                                             Qt::KeepAspectRatio,
                                                             label->devicePixelRatioF()));

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
            const QString normalIcon = label->property(kNormalIconProperty).toString();
            const QSize iconSize = label->property("iconSize").toSize();
            label->setPixmap(ImageService::instance().scaled(normalIcon,
                                                             iconSize,
                                                             Qt::KeepAspectRatio,
                                                             label->devicePixelRatioF()));
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
        if (property(kSystemFloatingBarsSuppressedProperty).toBool()) {
            MacFloatingInputBarBridge::clearInputBar(this);
            return;
        }

        if (m_usesNativeInput) {
            MacFloatingInputBarBridge::syncInputBar(this, QString(), m_visualOpacity);
        } else {
            MacFloatingInputBarBridge::syncGlass(this, m_visualOpacity);
        }
    }
#endif
}
