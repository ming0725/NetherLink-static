#include "IconLineEdit.h"

#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"

#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPalette>
#include <QProxyStyle>

namespace {

constexpr int kRadius = 8;
constexpr int kMarginLeft = 5;
constexpr int kMarginRight = 5;
constexpr int kIconTextSpacing = 5;
constexpr int kClearButtonSize = 20;
constexpr int kClearButtonSpacing = 4;

class TextOnlyLineEditStyle final : public QProxyStyle
{
public:
    void drawPrimitive(PrimitiveElement element,
                       const QStyleOption* option,
                       QPainter* painter,
                       const QWidget* widget = nullptr) const override
    {
        if (element == PE_PanelLineEdit || element == PE_FrameLineEdit) {
            return;
        }

        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
};

} // namespace

IconLineEdit::IconLineEdit(QWidget *parent)
        : QLineEdit(parent)
{
    setMouseTracking(true);
    setFrame(false);
    setFocusPolicy(Qt::ClickFocus);
    setPlaceholderText(QStringLiteral("搜索"));
    setAutoFillBackground(false);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
#ifdef Q_OS_MACOS
    setAttribute(Qt::WA_MacShowFocusRect, false);
#endif

    QFont inputFont;
    inputFont.setPixelSize(12);
    setFont(inputFont);

    applyThemePalette();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        applyThemePalette();
        update();
    });

    auto* lineEditStyle = new TextOnlyLineEditStyle;
    lineEditStyle->setParent(this);
    setStyle(lineEditStyle);

    connect(this, &QLineEdit::textChanged, this, [this](const QString& text) {
        Q_UNUSED(text);
        if (!showsClearButton()) {
            clearHovered = false;
            clearPressed = false;
            unsetCursor();
        }
        update();
    });

    updateControlRects();
    updateTextMargins();
}

IconLineEdit::~IconLineEdit() {}

QString IconLineEdit::currentText() const
{
    return text();
}

void IconLineEdit::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(hasFocus
                     ? ThemeManager::instance().color(ThemeColor::InputFocusBackground)
                     : ThemeManager::instance().color(ThemeColor::InputBackground));
    painter.drawRoundedRect(rect(), kRadius, kRadius);

    if (hasFocus) {
        QPen pen(ThemeManager::instance().color(ThemeColor::Accent));
        pen.setWidth(2);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), kRadius - 2, kRadius - 2);
    }
    painter.end();

    QLineEdit::paintEvent(event);

    QPainter foregroundPainter(this);
    foregroundPainter.setRenderHint(QPainter::SmoothPixmapTransform);
    const QString currentIconSource = ThemeManager::instance().isDark() && !darkIconSource.isEmpty()
            ? darkIconSource
            : iconSource;
    foregroundPainter.drawPixmap(iconRect,
                                 ImageService::instance().scaled(currentIconSource,
                                                                 iconRect.size(),
                                                                 Qt::IgnoreAspectRatio,
                                                                 foregroundPainter.device()->devicePixelRatioF()));

    if (showsClearButton()) {
        const QString clearIconSource = clearPressed
                ? QStringLiteral(":/resources/icon/hovered_close.png")
                : QStringLiteral(":/resources/icon/close.png");
        const QSize clearIconSize(12, 12);
        const QRect clearIconRect(clearButtonRect.x() + (clearButtonRect.width() - clearIconSize.width()) / 2,
                                  clearButtonRect.y() + (clearButtonRect.height() - clearIconSize.height()) / 2,
                                  clearIconSize.width(),
                                  clearIconSize.height());
        foregroundPainter.drawPixmap(clearIconRect,
                                      ImageService::instance().scaled(clearIconSource,
                                                                      clearIconSize,
                                                                      Qt::IgnoreAspectRatio,
                                                                      foregroundPainter.device()->devicePixelRatioF()));
    }
}

QLineEdit *IconLineEdit::getLineEdit() const
{
    return const_cast<IconLineEdit*>(this);
}

void IconLineEdit::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Tab) {
        emit requestNextFocus();
        event->accept();
        return;
    }

    QLineEdit::keyPressEvent(event);
}

void IconLineEdit::resizeEvent(QResizeEvent *event)
{
    QLineEdit::resizeEvent(event);
    updateControlRects();
    updateTextMargins();
}

void IconLineEdit::setIcon(const QString& source)
{
    iconSource = source;
    darkIconSource.clear();
    update();
}

void IconLineEdit::setDarkIcon(const QString& source)
{
    darkIconSource = source;
    update();
}

void IconLineEdit::setIconSize(QSize size)
{
    iconSize = size;
    updateControlRects();
    updateTextMargins();
    update();
}

void IconLineEdit::updateTextMargins()
{
    const int left = kMarginLeft + iconSize.width() + kIconTextSpacing;
    const int right = kMarginRight + kClearButtonSize + kClearButtonSpacing;
    setTextMargins(left, 0, right, 0);
}

void IconLineEdit::updateControlRects()
{
    const int iconX = kMarginLeft;
    const int iconY = (height() - iconSize.height()) / 2;
    iconRect = QRect(iconX, iconY, iconSize.width(), iconSize.height());

    const int clearX = width() - kMarginRight - kClearButtonSize;
    const int clearY = (height() - kClearButtonSize) / 2;
    clearButtonRect = QRect(clearX, clearY, kClearButtonSize, kClearButtonSize);
}

bool IconLineEdit::showsClearButton() const
{
    return !text().isEmpty();
}

void IconLineEdit::updateClearHoverState(const QPoint& pos)
{
    const bool hovered = showsClearButton() && clearButtonRect.contains(pos);
    if (clearHovered == hovered) {
        return;
    }

    clearHovered = hovered;
    if (!clearHovered) {
        clearPressed = false;
        unsetCursor();
    } else {
        setCursor(Qt::PointingHandCursor);
    }
    update();
}

void IconLineEdit::mouseMoveEvent(QMouseEvent* event)
{
    updateClearHoverState(event->pos());
    QLineEdit::mouseMoveEvent(event);
}

void IconLineEdit::leaveEvent(QEvent* event)
{
    if (clearHovered || clearPressed) {
        clearHovered = false;
        clearPressed = false;
        unsetCursor();
        update();
    }
    QLineEdit::leaveEvent(event);
}

void IconLineEdit::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && showsClearButton() && clearButtonRect.contains(event->pos())) {
        clearPressed = true;
        clearHovered = true;
        update();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        focusInnerLineEdit();
    }

    QLineEdit::mousePressEvent(event);
}

void IconLineEdit::mouseReleaseEvent(QMouseEvent* event)
{
    if (clearPressed && event->button() == Qt::LeftButton) {
        const bool shouldClear = clearButtonRect.contains(event->pos());
        clearPressed = false;
        clearHovered = shouldClear;
        if (shouldClear) {
            clear();
            focusInnerLineEdit();
        }
        update();
        event->accept();
        return;
    }

    QLineEdit::mouseReleaseEvent(event);
}

void IconLineEdit::focusInEvent(QFocusEvent* event)
{
    QLineEdit::focusInEvent(event);
    hasFocus = true;
    update();
    emit lineEditFocused();
}

void IconLineEdit::focusOutEvent(QFocusEvent* event)
{
    QLineEdit::focusOutEvent(event);
    hasFocus = false;
    clearPressed = false;
    clearHovered = false;
    unsetCursor();
    update();
    emit lineEditUnfocused();
}

void IconLineEdit::focusInnerLineEdit()
{
    setFocus();
    setCursorPosition(text().size());
}

void IconLineEdit::applyThemePalette()
{
    QPalette inputPalette = palette();
    inputPalette.setColor(QPalette::Base, Qt::transparent);
    inputPalette.setColor(QPalette::Window, Qt::transparent);
    inputPalette.setColor(QPalette::Text, ThemeManager::instance().color(ThemeColor::PrimaryText));
    inputPalette.setColor(QPalette::PlaceholderText,
                          ThemeManager::instance().color(ThemeColor::PlaceholderText));
    inputPalette.setColor(QPalette::Highlight, ThemeManager::instance().color(ThemeColor::Accent));
    inputPalette.setColor(QPalette::HighlightedText, Qt::white);
    setPalette(inputPalette);
}
