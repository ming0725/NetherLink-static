#include "AiChatFloatingInputBar.h"

#include <QAbstractButton>
#include <QAction>
#include <QActionGroup>
#include <QEvent>
#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QImage>
#include <QKeyEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>
#include <QResizeEvent>
#include <QGuiApplication>
#include <QRegion>
#include <QScreen>
#include <QStringList>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextOption>
#include <QtMath>

#include "shared/services/AppFonts.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/StyledActionMenu.h"
#include "shared/ui/TransparentTextEdit.h"

namespace {

constexpr int kLightPanelAlpha = 246;
constexpr int kDarkPanelAlpha = 226;
const QColor kLightPanelBorderColor(0xc2, 0xc2, 0xc2);
const QColor kDarkPanelBorderColor(0x72, 0x76, 0x80);

qreal easeOutCubic(qreal progress)
{
    progress = qBound<qreal>(0.0, progress, 1.0);
    const qreal inverse = 1.0 - progress;
    return 1.0 - inverse * inverse * inverse;
}

qreal easeInOutCubic(qreal progress)
{
    progress = qBound<qreal>(0.0, progress, 1.0);
    if (progress < 0.5) {
        return 4.0 * progress * progress * progress;
    }

    const qreal factor = -2.0 * progress + 2.0;
    return 1.0 - factor * factor * factor / 2.0;
}

QString submittedText(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text.trimmed();
}

QPoint menuPopupPos(QWidget* anchor, StyledActionMenu* menu)
{
    menu->ensurePolished();
    const QSize hint = menu->sizeHint();
    const QRect anchorRect(anchor->mapToGlobal(QPoint(0, 0)), anchor->size());
    QPoint pos(anchorRect.left(), anchorRect.bottom() + 8);

    QScreen* screen = QGuiApplication::screenAt(anchorRect.center());
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) {
        return pos;
    }

    const QRect available = screen->availableGeometry().adjusted(8, 8, -8, -8);
    if (pos.y() + hint.height() > available.bottom()) {
        pos.setY(anchorRect.top() - hint.height() - 8);
    }
    if (pos.x() + hint.width() > available.right()) {
        pos.setX(available.right() - hint.width());
    }
    pos.setX(qMax(available.left(), pos.x()));
    pos.setY(qMax(available.top(), pos.y()));
    return pos;
}

class PlusButton final : public QAbstractButton
{
public:
    explicit PlusButton(QWidget* parent = nullptr)
        : QAbstractButton(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QColor color = ThemeManager::instance().color(ThemeColor::SecondaryText);
        painter.setPen(QPen(color, 1.45, Qt::SolidLine, Qt::RoundCap));
        const QPointF center = rect().center();
        const qreal arm = 7.0;
        painter.drawLine(QPointF(center.x() - arm, center.y()),
                         QPointF(center.x() + arm, center.y()));
        painter.drawLine(QPointF(center.x(), center.y() - arm),
                         QPointF(center.x(), center.y() + arm));
    }
};

class MenuTextButton final : public QAbstractButton
{
public:
    explicit MenuTextButton(const QString& text, QWidget* parent = nullptr)
        : QAbstractButton(parent)
        , m_text(text)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    QSize sizeHint() const override
    {
        QFont font = this->font();
        font.setPixelSize(14);
        const QFontMetrics metrics(font);
        int width = textPixelWidth(metrics, m_text) + kTrailingTextPadding;
        if (!m_secondaryText.isEmpty()) {
            width += kSecondaryTextGap + textPixelWidth(metrics, m_secondaryText);
        }
        if (m_showFastIcon) {
            width += kFastIconSide + kLeadingGap;
        }
        return QSize(width, 28);
    }

    void setText(const QString& text, const QString& secondaryText = QString())
    {
        if (m_text == text && m_secondaryText == secondaryText) {
            return;
        }

        m_text = text;
        m_secondaryText = secondaryText;
        updateGeometry();
        update();
    }

    void setFastIconVisible(bool visible)
    {
        if (m_showFastIcon == visible) {
            return;
        }

        m_showFastIcon = visible;
        updateGeometry();
        update();
    }

    void setMenuOpen(bool open)
    {
        if (m_menuOpen == open) {
            return;
        }

        m_menuOpen = open;
        if (!open) {
            m_suppressHoverUntilLeave = true;
        }
        update();
    }

protected:
    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::Enter || event->type() == QEvent::Leave) {
            m_suppressHoverUntilLeave = false;
            update();
        }
        return QAbstractButton::event(event);
    }

    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        AppFonts::configurePainterForText(painter);

        QColor hover = ThemeManager::instance().color(ThemeColor::ContextMenuHover);
        hover.setAlpha((m_menuOpen || (underMouse() && !m_suppressHoverUntilLeave)) ? 150 : 0);
        if (hover.alpha() > 0) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(hover);
            painter.drawRoundedRect(rect(), 8, 8);
        }

        QFont textFont = painter.font();
        textFont.setPixelSize(14);
        painter.setFont(textFont);

        int contentLeft = 0;
        if (m_showFastIcon) {
            const QRect iconRect(contentLeft,
                                 rect().center().y() - kFastIconSide / 2,
                                 kFastIconSide,
                                 kFastIconSide);
            paintFastIcon(painter, iconRect);
            contentLeft = iconRect.right() + 1 + kLeadingGap;
        }

        const QRect textRect = rect().adjusted(contentLeft, 0, -kTrailingTextPadding, 0);
        paintText(painter, textRect);

        const int cx = rect().right() - 8;
        const int cy = rect().center().y() + 1;
        QPainterPath chevron;
        chevron.moveTo(cx - 4, cy - 2);
        chevron.lineTo(cx, cy + 2);
        chevron.lineTo(cx + 4, cy - 2);
        painter.setPen(QPen(ThemeManager::instance().color(ThemeColor::SecondaryText), 1.55, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(chevron);
    }

private:
    QPixmap fastIconPixmap() const
    {
        QImage image(QStringLiteral(":/resources/icon/power.png"));
        if (image.isNull()) {
            return {};
        }

        image = image.convertToFormat(QImage::Format_ARGB32);
        if (ThemeManager::instance().isDark()) {
            for (int y = 0; y < image.height(); ++y) {
                QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
                for (int x = 0; x < image.width(); ++x) {
                    const QColor color = QColor::fromRgba(line[x]);
                    line[x] = qRgba(255 - color.red(),
                                    255 - color.green(),
                                    255 - color.blue(),
                                    color.alpha());
                }
            }
        }
        return QPixmap::fromImage(image);
    }

    void paintFastIcon(QPainter& painter, const QRect& iconRect) const
    {
        const QPixmap icon = fastIconPixmap();
        if (icon.isNull()) {
            return;
        }

        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawPixmap(iconRect, icon);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    }

    void paintText(QPainter& painter, const QRect& textRect) const
    {
        const QFontMetrics metrics = painter.fontMetrics();
        const int textWidth = textRect.width();
        if (textWidth <= 0) {
            return;
        }

        const QColor primaryColor = ThemeManager::instance().color(ThemeColor::SecondaryText);
        if (m_secondaryText.isEmpty()) {
            painter.setPen(primaryColor);
            painter.drawText(textRect,
                             Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                             metrics.elidedText(m_text, Qt::ElideRight, textWidth));
            return;
        }

        QColor secondaryColor = primaryColor;
        secondaryColor.setAlpha(ThemeManager::instance().isDark() ? 150 : 118);

        const int naturalSecondaryWidth = textPixelWidth(metrics, m_secondaryText);
        const int naturalPrimaryWidth = textPixelWidth(metrics, m_text);
        const int naturalTextWidth = naturalPrimaryWidth + kSecondaryTextGap + naturalSecondaryWidth;

        int primaryWidth = naturalPrimaryWidth;
        int secondaryWidth = naturalSecondaryWidth;
        if (textWidth < naturalTextWidth) {
            secondaryWidth = qMin(naturalSecondaryWidth, textWidth);
            primaryWidth = qMax(0, textWidth - secondaryWidth - kSecondaryTextGap);
        }

        const QRect primaryRect(textRect.left(), textRect.top(), primaryWidth, textRect.height());
        painter.setPen(primaryColor);
        painter.drawText(primaryRect,
                         Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                         metrics.elidedText(m_text, Qt::ElideRight, primaryWidth));

        const QRect secondaryRect(primaryRect.right() + 1 + kSecondaryTextGap,
                                  textRect.top(),
                                  secondaryWidth,
                                  textRect.height());
        painter.setPen(secondaryColor);
        painter.drawText(secondaryRect,
                         Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                         metrics.elidedText(m_secondaryText, Qt::ElideRight, secondaryWidth));
    }

    static constexpr int kFastIconSide = 15;
    static constexpr int kLeadingGap = 5;
    static constexpr int kSecondaryTextGap = 5;
    static constexpr int kTrailingTextPadding = 26;
    static constexpr int kTextElideSlack = 4;

    static int textPixelWidth(const QFontMetrics& metrics, const QString& text)
    {
        return text.isEmpty() ? 0 : metrics.horizontalAdvance(text) + kTextElideSlack;
    }

    QString m_text;
    QString m_secondaryText;
    bool m_showFastIcon = false;
    bool m_menuOpen = false;
    bool m_suppressHoverUntilLeave = false;
};

class SendButton final : public QAbstractButton
{
public:
    explicit SendButton(QWidget* parent = nullptr)
        : QAbstractButton(parent)
    {
        updateCursor();
        setFocusPolicy(Qt::NoFocus);
    }

    void setStreaming(bool streaming)
    {
        if (m_streaming == streaming) {
            return;
        }
        m_streaming = streaming;
        updateCursor();
        update();
    }

    void setHasText(bool hasText)
    {
        if (m_hasText == hasText) {
            return;
        }
        m_hasText = hasText;
        updateCursor();
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QString iconPath = m_streaming
                ? QStringLiteral(":/resources/icon/fail.png")
                : QStringLiteral(":/resources/icon/arrow.png");
        const QPixmap icon(iconPath);
        if (icon.isNull()) {
            return;
        }

        const int iconSide = qRound(qMin(width(), height()) * (m_streaming ? 0.58 : 0.68));
        QRect iconRect(0, 0, iconSide, iconSide);
        iconRect.moveCenter(rect().center());

        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter.drawPixmap(iconRect, icon);
    }

private:
    void updateCursor()
    {
        setCursor((m_streaming || m_hasText) ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }

    bool m_streaming = false;
    bool m_hasText = false;
};

QAction* addMenuAction(StyledActionMenu* menu, const QString& text, bool checked = false)
{
    QAction* action = menu->addAction(text);
    action->setCheckable(true);
    action->setChecked(checked);
    return action;
}

void setMenuButtonText(QAbstractButton* button,
                       const QString& text,
                       const QString& secondaryText = QString())
{
    if (auto* menuButton = dynamic_cast<MenuTextButton*>(button)) {
        menuButton->setText(text, secondaryText);
    }
}

} // namespace

AiChatFloatingInputBar::AiChatFloatingInputBar(QWidget* parent)
    : QWidget(parent)
    , m_inputEdit(new TransparentTextEdit(this))
    , m_addButton(new PlusButton(this))
    , m_permissionButton(new MenuTextButton(QStringLiteral("Default permissions"), this))
    , m_modelButton(new MenuTextButton(QString(), this))
    , m_actionButton(new SendButton(this))
    , m_borderGlowTimer(new QTimer(this))
{
    setAutoFillBackground(false);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setWindowFlags(Qt::FramelessWindowHint);
    setFocusPolicy(Qt::StrongFocus);
    m_borderGlowClock.start();
    m_borderGlowTimer->setInterval(kBorderGlowFrameMs);
    connect(m_borderGlowTimer, &QTimer::timeout,
            this, &AiChatFloatingInputBar::updateBorderGlowAnimation);

    m_inputEdit->setFocusPolicy(Qt::StrongFocus);
    m_inputEdit->setAcceptRichText(false);
    m_inputEdit->setPlaceholderText(QStringLiteral("输入你想提问的任何事情"));
    m_inputEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_inputEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_inputEdit->setLineWrapMode(QTextEdit::WidgetWidth);
    m_inputEdit->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_inputEdit->setMinimumHeight(kMinInputHeight);
    m_inputEdit->setViewportPadding(kInputTextHorizontalPadding,
                                    kInputTextTopPadding,
                                    kInputTextHorizontalPadding,
                                    kInputTextBottomPadding);
    m_inputEdit->viewport()->setCursor(Qt::IBeamCursor);
    m_inputEdit->installEventFilter(this);
    m_inputEdit->viewport()->installEventFilter(this);
    setFocusProxy(m_inputEdit);

    connect(m_permissionButton, &QAbstractButton::clicked,
            this, &AiChatFloatingInputBar::showPermissionMenu);
    connect(m_modelButton, &QAbstractButton::clicked,
            this, &AiChatFloatingInputBar::showModelMenu);
    connect(m_actionButton, &QAbstractButton::clicked,
            this, &AiChatFloatingInputBar::onActionButtonClicked);
    connect(m_inputEdit, &QTextEdit::textChanged, this, [this]() {
        updateActionButtonIcon();
        updatePreferredHeight();
    });

    QFont font = m_inputEdit->font();
    font.setPixelSize(15);
    font.setHintingPreference(QFont::PreferFullHinting);
    font.setStyleStrategy(QFont::PreferAntialias);
    m_inputEdit->setFont(font);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(18);
    shadow->setOffset(0, 4);
    QColor shadowColor = ThemeManager::instance().color(ThemeColor::FloatingPanelShadow);
    shadowColor.setAlpha(ThemeManager::instance().isDark() ? 70 : 42);
    shadow->setColor(shadowColor);
    setGraphicsEffect(shadow);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &AiChatFloatingInputBar::applyTheme);
    applyTheme();
    updateActionButtonIcon();
    updateModelButtonText();
    updatePreferredHeight();
    updateInputGeometry();
}

QSize AiChatFloatingInputBar::sizeHint() const
{
    return QSize(640, preferredHeightForWidth(width() > 0 ? width() : 640));
}

void AiChatFloatingInputBar::focusInput()
{
    m_inputEdit->setFocus();
    m_inputEdit->moveCursor(QTextCursor::End);
}

void AiChatFloatingInputBar::refocusInputAfterPositionChange()
{
    m_inputEdit->clearFocus();
    clearFocus();
    QTimer::singleShot(kInputRefocusAfterMoveDelayMs, this, [this]() {
        if (!isVisible() || !isEnabled()) {
            return;
        }
        focusInput();
    });
}

void AiChatFloatingInputBar::setStreaming(bool streaming, bool animateTransition)
{
    if (m_streaming == streaming) {
        if (!streaming && !animateTransition) {
            stopBorderGlow();
        }
        return;
    }

    m_streaming = streaming;
    if (m_streaming) {
        startBorderGlow();
    } else if (animateTransition) {
        finishBorderGlow();
    } else {
        stopBorderGlow();
    }
    updateActionButtonIcon();
}

QString AiChatFloatingInputBar::text() const
{
    return m_inputEdit->toPlainText();
}

void AiChatFloatingInputBar::setText(const QString& text)
{
    m_inputEdit->setPlainText(text);
    m_inputEdit->moveCursor(QTextCursor::End);
    updatePreferredHeight();
}

void AiChatFloatingInputBar::clearText()
{
    m_inputEdit->clear();
    updatePreferredHeight();
}

void AiChatFloatingInputBar::setContextUsage(const AiChatContextUsage& usage)
{
    Q_UNUSED(usage)
}

void AiChatFloatingInputBar::setContextUsageVisible(bool visible)
{
    Q_UNUSED(visible)
}

int AiChatFloatingInputBar::preferredHeightForWidth(int width) const
{
    const int inputWidth = qMax(1, width - kInputLeftMargin - kInputRightPadding -
                                      kInputTextHorizontalPadding * 2);

    QTextDocument document;
    document.setDocumentMargin(0);
    document.setDefaultFont(m_inputEdit->font());
    QTextOption option = m_inputEdit->document()->defaultTextOption();
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    document.setDefaultTextOption(option);
    const QString content = m_inputEdit->toPlainText().isEmpty()
            ? QStringLiteral(" ")
            : m_inputEdit->toPlainText();
    document.setPlainText(content);
    document.setTextWidth(inputWidth);

    const int documentHeight = qCeil(document.size().height());
    const int desiredInputHeight = documentHeight +
            kInputTextTopPadding + kInputTextBottomPadding + 2;
    const int inputHeight = qBound(kMinInputHeight, desiredInputHeight, kMaxInputHeight);
    return qBound(kMinHeight,
                  kInputTopMargin + inputHeight + kInputToolbarGap +
                          kToolbarHeight + kToolbarBottomMargin,
                  kMaxHeight);
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

    const QRectF panelRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath path;
    path.addRoundedRect(panelRect, kCornerRadius, kCornerRadius);

    QColor background = ThemeManager::instance().color(ThemeColor::PanelBackground);
    background.setAlpha(ThemeManager::instance().isDark() ? kDarkPanelAlpha : kLightPanelAlpha);
    painter.fillPath(path, background);

    const bool dark = ThemeManager::instance().isDark();
    QColor border = dark ? kDarkPanelBorderColor
                         : kLightPanelBorderColor;
    border.setAlpha(dark ? 180 : 230);
    painter.setPen(QPen(border, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);

    paintBorderGlow(painter, panelRect);
}

void AiChatFloatingInputBar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    invalidateBorderGlowCache();
    updatePreferredHeight();
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

    if (auto* shadow = qobject_cast<QGraphicsDropShadowEffect*>(graphicsEffect())) {
        QColor shadowColor = ThemeManager::instance().color(ThemeColor::FloatingPanelShadow);
        shadowColor.setAlpha(ThemeManager::instance().isDark() ? 70 : 42);
        shadow->setColor(shadowColor);
    }

    updateActionButtonIcon();
    m_addButton->update();
    m_permissionButton->update();
    m_modelButton->update();
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

void AiChatFloatingInputBar::showPermissionMenu()
{
    auto* menu = new StyledActionMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    if (auto* button = dynamic_cast<MenuTextButton*>(m_permissionButton)) {
        button->setMenuOpen(true);
    }
    addMenuAction(menu, QStringLiteral("Default permissions"), true);
    addMenuAction(menu, QStringLiteral("Ask every time"));
    addMenuAction(menu, QStringLiteral("Read-only"));
    connect(menu, &StyledActionMenu::triggered, this, [this](QAction* action) {
        if (!action || action->isSeparator()) {
            return;
        }
        setMenuButtonText(m_permissionButton, action->text());
        updateInputGeometry();
    });
    connect(menu, &StyledActionMenu::aboutToHide, this, [this, menu]() {
        if (auto* button = dynamic_cast<MenuTextButton*>(m_permissionButton)) {
            button->setMenuOpen(false);
        }
        menu->deleteLater();
    });
    menu->popup(menuPopupPos(m_permissionButton, menu));
}

void AiChatFloatingInputBar::showModelMenu()
{
    auto* menu = new StyledActionMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    if (auto* button = dynamic_cast<MenuTextButton*>(m_modelButton)) {
        button->setMenuOpen(true);
    }

    auto* thinkingGroup = new QActionGroup(menu);
    thinkingGroup->setExclusive(true);
    const QStringList thinkingLevels = {
        QStringLiteral("Low"),
        QStringLiteral("Medium"),
        QStringLiteral("High"),
        QStringLiteral("Max"),
    };
    for (const QString& level : thinkingLevels) {
        QAction* action = addMenuAction(menu, level, level == m_selectedThinkingLevel);
        thinkingGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, level]() {
            m_selectedThinkingLevel = level;
            updateModelButtonText();
            updateInputGeometry();
        });
    }

    menu->addSeparator();

    auto* modelGroup = new QActionGroup(menu);
    modelGroup->setExclusive(true);

    StyledActionMenu* deepSeekMenu = menu->addStyledMenu(QStringLiteral("DeepSeek"));
    const QStringList deepSeekModels = {
        QStringLiteral("DeepSeek V4 Pro"),
        QStringLiteral("DeepSeek V4 Flash"),
    };
    for (const QString& model : deepSeekModels) {
        QAction* action = addMenuAction(deepSeekMenu, model, model == m_selectedModelName);
        modelGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, model]() {
            m_selectedModelName = model;
            updateModelButtonText();
            updateInputGeometry();
        });
    }

    connect(menu, &StyledActionMenu::aboutToHide, this, [this, menu]() {
        if (auto* button = dynamic_cast<MenuTextButton*>(m_modelButton)) {
            button->setMenuOpen(false);
        }
        menu->deleteLater();
    });
    menu->popup(menuPopupPos(m_modelButton, menu));
}

void AiChatFloatingInputBar::updateModelButtonText()
{
    setMenuButtonText(m_modelButton, m_selectedModelName, m_selectedThinkingLevel);
    updateModelButtonState();
}

void AiChatFloatingInputBar::updateModelButtonState()
{
    if (auto* button = dynamic_cast<MenuTextButton*>(m_modelButton)) {
        button->setFastIconVisible(false);
    }
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
    if (auto* sendButton = dynamic_cast<SendButton*>(m_actionButton)) {
        sendButton->setStreaming(m_streaming);
        sendButton->setHasText(!submittedText(m_inputEdit->toPlainText()).isEmpty());
    }
}

void AiChatFloatingInputBar::updateInputGeometry()
{
    const int toolbarY = qMax(kInputTopMargin, height() - kToolbarBottomMargin - kToolbarHeight);
    const int inputHeight = qMax(0, toolbarY - kInputTopMargin - kInputToolbarGap);
    m_inputEdit->setGeometry(kInputLeftMargin,
                             kInputTopMargin,
                             qMax(0, width() - kInputLeftMargin - kInputRightPadding),
                             inputHeight);

    const int buttonY = toolbarY + (kToolbarHeight - kActionButtonSize) / 2;
    m_addButton->setGeometry(kToolbarSideMargin, buttonY, kActionButtonSize, kActionButtonSize);

    m_actionButton->setGeometry(qMax(0, width() - kActionButtonRightMargin - kActionButtonSize),
                                buttonY,
                                kActionButtonSize,
                                kActionButtonSize);

    int rightCursorX = m_actionButton->geometry().left() - kToolbarGap;

    const QSize modelSize = m_modelButton->sizeHint();
    const int modelAvailableWidth = qMax(28, rightCursorX - kToolbarSideMargin);
    const int modelWidth = qMin(modelSize.width(), modelAvailableWidth);
    const int modelX = rightCursorX - modelWidth;
    m_modelButton->setGeometry(modelX,
                               toolbarY + (kToolbarHeight - modelSize.height()) / 2,
                               modelWidth,
                               modelSize.height());

    int rightClusterLeft = m_modelButton->geometry().left();

    int cursorX = m_addButton->geometry().right() + 1 + kToolbarGap;
    const QSize permissionSize = m_permissionButton->sizeHint();
    const int permissionAvailableWidth = qMax(0,
                                              rightClusterLeft - kToolbarGap - cursorX);
    m_permissionButton->setGeometry(cursorX,
                                    toolbarY + (kToolbarHeight - permissionSize.height()) / 2,
                                    qMin(permissionSize.width(), permissionAvailableWidth),
                                    permissionSize.height());
}

void AiChatFloatingInputBar::updatePreferredHeight()
{
    const int newHeight = preferredHeightForWidth(width() > 0 ? width() : 640);
    if (m_preferredHeight == newHeight) {
        return;
    }

    m_preferredHeight = newHeight;
    updateGeometry();
    emit preferredHeightChanged(newHeight);
}

void AiChatFloatingInputBar::startBorderGlow()
{
    m_borderGlowState = BorderGlowState::Streaming;
    m_borderGlowStartMs = m_borderGlowClock.elapsed();
    m_borderGlowFinishStartMs = 0;
    if (m_borderGlowTimer && !m_borderGlowTimer->isActive()) {
        m_borderGlowTimer->start();
    }
    updateBorderGlowRegion();
}

void AiChatFloatingInputBar::finishBorderGlow()
{
    if (m_borderGlowState == BorderGlowState::Idle) {
        updateBorderGlowRegion();
        return;
    }

    if (m_borderGlowState == BorderGlowState::Streaming) {
        const qint64 revealElapsed = m_borderGlowClock.elapsed() - m_borderGlowStartMs;
        if (revealElapsed < kBorderGlowRevealDurationMs) {
            m_borderGlowState = BorderGlowState::PendingFinish;
            m_borderGlowFinishStartMs = 0;
            if (m_borderGlowTimer && !m_borderGlowTimer->isActive()) {
                m_borderGlowTimer->start();
            }
            updateBorderGlowRegion();
            return;
        }
    }

    m_borderGlowState = BorderGlowState::Finishing;
    m_borderGlowFinishStartMs = m_borderGlowClock.elapsed();
    if (m_borderGlowTimer && !m_borderGlowTimer->isActive()) {
        m_borderGlowTimer->start();
    }
    updateBorderGlowRegion();
}

void AiChatFloatingInputBar::stopBorderGlow()
{
    m_borderGlowState = BorderGlowState::Idle;
    m_borderGlowFinishStartMs = 0;
    if (m_borderGlowTimer) {
        m_borderGlowTimer->stop();
    }
    updateBorderGlowRegion();
}

void AiChatFloatingInputBar::updateBorderGlowAnimation()
{
    if (m_borderGlowState == BorderGlowState::Idle) {
        if (m_borderGlowTimer) {
            m_borderGlowTimer->stop();
        }
        return;
    }

    if (m_borderGlowState == BorderGlowState::PendingFinish) {
        const qint64 revealElapsed = m_borderGlowClock.elapsed() - m_borderGlowStartMs;
        if (revealElapsed >= kBorderGlowRevealDurationMs) {
            m_borderGlowState = BorderGlowState::Finishing;
            m_borderGlowFinishStartMs = m_borderGlowClock.elapsed();
        }
    }

    if (m_borderGlowState == BorderGlowState::Finishing) {
        const qint64 elapsed = m_borderGlowClock.elapsed() - m_borderGlowFinishStartMs;
        if (elapsed >= kBorderGlowFinishDurationMs) {
            m_borderGlowState = BorderGlowState::Idle;
            if (m_borderGlowTimer) {
                m_borderGlowTimer->stop();
            }
        }
    }

    updateBorderGlowRegion();
}

void AiChatFloatingInputBar::paintBorderGlow(QPainter& painter, const QRectF& panelRect)
{
    if (m_borderGlowState == BorderGlowState::Idle) {
        return;
    }

    const QRectF glowRect = panelRect.adjusted(kBorderGlowInset,
                                               kBorderGlowInset,
                                               -kBorderGlowInset,
                                               -kBorderGlowInset);
    if (glowRect.width() <= kCornerRadius || glowRect.height() <= kCornerRadius) {
        return;
    }

    if (!ensureBorderGlowCache(glowRect)) {
        return;
    }

    const qreal totalLength = m_borderGlowTotalLength;
    if (totalLength <= 0.0) {
        return;
    }

    const qint64 now = m_borderGlowClock.elapsed();
    const qreal phase = static_cast<qreal>(now - m_borderGlowStartMs) / 4200.0;
    const qreal halfLength = totalLength * 0.5;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (m_borderGlowState == BorderGlowState::Streaming
            || m_borderGlowState == BorderGlowState::PendingFinish) {
        const qreal revealElapsed = static_cast<qreal>(now - m_borderGlowStartMs);
        const qreal revealProgress = easeOutCubic(revealElapsed / kBorderGlowRevealDurationMs);
        const qreal visibleHalfLength = halfLength * revealProgress;
        paintBorderGlowRange(painter, totalLength, 0.0, visibleHalfLength, phase, 1.0);
        paintBorderGlowRange(painter,
                             totalLength,
                             totalLength - visibleHalfLength,
                             totalLength,
                             phase,
                             1.0);
    } else {
        const qreal finishElapsed = static_cast<qreal>(now - m_borderGlowFinishStartMs);
        const qreal finishProgress = easeInOutCubic(finishElapsed / kBorderGlowFinishDurationMs);
        const qreal clearedLength = halfLength * finishProgress;
        const qreal opacity = 1.0 - finishProgress * 0.22;
        paintBorderGlowRange(painter,
                             totalLength,
                             clearedLength,
                             halfLength,
                             phase,
                             opacity);
        paintBorderGlowRange(painter,
                             totalLength,
                             halfLength,
                             totalLength - clearedLength,
                             phase,
                             opacity);
    }

    painter.restore();
}

bool AiChatFloatingInputBar::ensureBorderGlowCache(const QRectF& rect)
{
    if (m_borderGlowCachedRect == rect && !m_borderGlowSamples.isEmpty()) {
        return true;
    }

    m_borderGlowCachedRect = rect;
    m_borderGlowSamples.clear();
    m_borderGlowTotalLength = 0.0;

    const qreal radius = qMin<qreal>(kCornerRadius - kBorderGlowInset,
                                     qMin(rect.width(), rect.height()) * 0.5);
    if (radius <= 0.0) {
        return false;
    }

    const qreal left = rect.left();
    const qreal right = rect.right();
    const qreal top = rect.top();
    const qreal bottom = rect.bottom();
    const qreal centerX = rect.center().x();

    qreal cursor = 0.0;
    auto appendPoint = [this](const QPointF& point, qreal length) {
        if (!m_borderGlowSamples.isEmpty()
            && QLineF(m_borderGlowSamples.constLast().point, point).length() < 0.001) {
            m_borderGlowSamples.last().length = length;
            return;
        }

        m_borderGlowSamples.append({point, length});
    };

    auto appendLine = [&appendPoint, &cursor](const QPointF& from, const QPointF& to) {
        const qreal segmentLength = QLineF(from, to).length();
        if (segmentLength <= 0.0) {
            return;
        }

        const int steps = qMax(1, qCeil(segmentLength / kBorderGlowSampleStep));
        for (int i = 1; i <= steps; ++i) {
            const qreal progress = static_cast<qreal>(i) / steps;
            appendPoint(from + (to - from) * progress, cursor + segmentLength * progress);
        }
        cursor += segmentLength;
    };

    auto appendArc = [&appendPoint, &cursor](const QPointF& center,
                                             qreal arcRadius,
                                             qreal startDegrees,
                                             qreal sweepDegrees) {
        const qreal segmentLength = qAbs(qDegreesToRadians(sweepDegrees)) * arcRadius;
        if (segmentLength <= 0.0) {
            return;
        }

        const int steps = qMax(1, qCeil(segmentLength / kBorderGlowSampleStep));
        for (int i = 1; i <= steps; ++i) {
            const qreal progress = static_cast<qreal>(i) / steps;
            const qreal angle = qDegreesToRadians(startDegrees + sweepDegrees * progress);
            appendPoint(QPointF(center.x() + qCos(angle) * arcRadius,
                                center.y() - qSin(angle) * arcRadius),
                        cursor + segmentLength * progress);
        }
        cursor += segmentLength;
    };

    const QPointF topCenter(centerX, top);
    appendPoint(topCenter, 0.0);
    appendLine(topCenter, QPointF(right - radius, top));
    appendArc(QPointF(right - radius, top + radius), radius, 90.0, -90.0);
    appendLine(QPointF(right, top + radius), QPointF(right, bottom - radius));
    appendArc(QPointF(right - radius, bottom - radius), radius, 0.0, -90.0);
    appendLine(QPointF(right - radius, bottom), QPointF(left + radius, bottom));
    appendArc(QPointF(left + radius, bottom - radius), radius, -90.0, -90.0);
    appendLine(QPointF(left, bottom - radius), QPointF(left, top + radius));
    appendArc(QPointF(left + radius, top + radius), radius, 180.0, -90.0);
    appendLine(QPointF(left + radius, top), topCenter);

    m_borderGlowTotalLength = cursor;
    return m_borderGlowSamples.size() >= 2 && m_borderGlowTotalLength > 0.0;
}

void AiChatFloatingInputBar::invalidateBorderGlowCache()
{
    m_borderGlowCachedRect = QRectF();
    m_borderGlowSamples.clear();
    m_borderGlowTotalLength = 0.0;
}

void AiChatFloatingInputBar::updateBorderGlowRegion()
{
    if (width() <= 0 || height() <= 0) {
        update();
        return;
    }

    const int band = qMin(qCeil(kBorderGlowInset + kBorderGlowMaxPenWidth + 3.0),
                          qMin(width(), height()));
    QRegion region(QRect(0, 0, width(), band));
    region += QRect(0, qMax(0, height() - band), width(), band);
    region += QRect(0, 0, band, height());
    region += QRect(qMax(0, width() - band), 0, band, height());
    update(region);
}

QColor AiChatFloatingInputBar::borderGlowColor(qreal position,
                                               qreal phase,
                                               qreal opacity,
                                               qreal alphaScale) const
{
    qreal hue = position + phase * 0.72;
    hue -= qFloor(hue);

    const qreal twoPi = 6.283185307179586;
    const qreal wave = 0.82 + 0.18 * ((qSin((position * 2.4 - phase * 1.8) * twoPi) + 1.0) * 0.5);
    QColor color = QColor::fromHsvF(hue, 0.82, 1.0);
    const qreal themeBoost = ThemeManager::instance().isDark() ? 1.0 : 0.82;
    color.setAlpha(qRound(qBound<qreal>(0.0,
                                        255.0 * opacity * alphaScale * wave * themeBoost,
                                        255.0)));
    return color;
}

void AiChatFloatingInputBar::paintBorderGlowRange(QPainter& painter,
                                                  qreal totalLength,
                                                  qreal startLength,
                                                  qreal endLength,
                                                  qreal phase,
                                                  qreal opacity)
{
    startLength = qBound<qreal>(0.0, startLength, totalLength);
    endLength = qBound<qreal>(0.0, endLength, totalLength);
    if (endLength <= startLength || opacity <= 0.0) {
        return;
    }

    if (m_borderGlowSamples.size() < 2) {
        return;
    }

    struct GlowLayer {
        qreal width;
        qreal alphaScale;
    };
    const GlowLayer layers[] = {
        {5.2, 0.10},
        {2.8, 0.20},
        {1.35, 0.92},
    };

    for (const GlowLayer& layer : layers) {
        QPen pen(Qt::white, layer.width, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin);
        painter.setPen(pen);

        for (int i = 1; i < m_borderGlowSamples.size(); ++i) {
            const BorderGlowSample& previous = m_borderGlowSamples.at(i - 1);
            const BorderGlowSample& current = m_borderGlowSamples.at(i);
            if (current.length <= startLength) {
                continue;
            }
            if (previous.length >= endLength) {
                break;
            }

            const qreal visibleStart = qMax(startLength, previous.length);
            const qreal visibleEnd = qMin(endLength, current.length);
            if (visibleEnd <= visibleStart || current.length <= previous.length) {
                continue;
            }

            const qreal segmentLength = current.length - previous.length;
            const qreal startProgress = (visibleStart - previous.length) / segmentLength;
            const qreal endProgress = (visibleEnd - previous.length) / segmentLength;
            const QPointF segmentStart = previous.point +
                    (current.point - previous.point) * startProgress;
            const QPointF segmentEnd = previous.point +
                    (current.point - previous.point) * endProgress;
            const qreal segmentMid = (visibleStart + visibleEnd) * 0.5;
            pen.setColor(borderGlowColor(segmentMid / totalLength,
                                         phase,
                                         opacity,
                                         layer.alphaScale));
            painter.setPen(pen);
            painter.drawLine(segmentStart, segmentEnd);
        }
    }
}
