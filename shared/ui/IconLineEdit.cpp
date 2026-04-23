#include "IconLineEdit.h"

#include "shared/services/ImageService.h"

#include <QKeyEvent>
#include <QMouseEvent>

IconLineEdit::IconLineEdit(QWidget *parent)
        : QWidget(parent)
{
    setMouseTracking(true);

    iconLabel = new QLabel(this);
    iconLabel->setFixedSize(15, 15);
    iconLabel->setScaledContents(true);
    iconLabel->setAttribute(Qt::WA_TranslucentBackground);

    QFont font;
    font.setPixelSize(12);
    lineEdit = new QLineEdit(this);
    lineEdit->installEventFilter(this);
    lineEdit->setMouseTracking(true);
    lineEdit->setFont(font);
    lineEdit->setStyleSheet("QLineEdit{background-color:transparent;} "
                            "QLineEdit:hover{background:transparent;}");
    lineEdit->setFrame(QFrame::NoFrame);
    lineEdit->setFocusPolicy(Qt::ClickFocus);
    lineEdit->setPlaceholderText("搜索");

    connect(lineEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        Q_UNUSED(text);
        if (!showsClearButton()) {
            clearHovered = false;
            clearPressed = false;
            unsetCursor();
        }
        update();
    });

    refreshIcons();
}

IconLineEdit::~IconLineEdit() {}

QString IconLineEdit::currentText() const {
    if (this->lineEdit->text().isEmpty())
        return "";
    return this->lineEdit->text();
}

void IconLineEdit::paintEvent(QPaintEvent *ev) {
    QPainter painter(this);
    int radius = 8;
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0xF5F5F5));
    painter.drawRoundedRect(rect(), radius, radius);

    if (hasFocus) {
        QPen pen(QColor(0x0099ff));
        pen.setWidth(2);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        QRect borderRect = rect().adjusted(1, 1, -1, -1);
        painter.drawRoundedRect(borderRect, radius - 2, radius - 2);
    }

    if (showsClearButton()) {
        const QString clearIconSource = clearPressed
                ? QStringLiteral(":/resources/icon/hovered_close.png")
                : QStringLiteral(":/resources/icon/close.png");
        const QSize iconSize(12, 12);
        const QRect iconRect(clearButtonRect.x() + (clearButtonRect.width() - iconSize.width()) / 2,
                             clearButtonRect.y() + (clearButtonRect.height() - iconSize.height()) / 2,
                             iconSize.width(),
                             iconSize.height());
        painter.drawPixmap(iconRect,
                           ImageService::instance().scaled(clearIconSource,
                                                           iconSize,
                                                           Qt::IgnoreAspectRatio,
                                                           painter.device()->devicePixelRatioF()));
    }

    QWidget::paintEvent(ev);
}

QLineEdit *IconLineEdit::getLineEdit() const {
    return this->lineEdit;
}

void IconLineEdit::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Tab) {
        emit requestNextFocus();
    }
    else {
        QWidget::keyPressEvent(event);
    }
}

bool IconLineEdit::eventFilter(QObject *watched, QEvent *event) {
    if (watched == lineEdit) {
        if (event->type() == QEvent::FocusIn) {
            hasFocus = true;
            update();
            emit lineEditFocused();
        }
        else if (event->type() == QEvent::FocusOut) {
            hasFocus = false;
            update();
            emit lineEditUnfocused();
        }
        else if (event->type() == QEvent::MouseMove) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            updateClearHoverState(lineEdit->mapToParent(mouseEvent->position().toPoint()));
        }
        else if (event->type() == QEvent::Leave) {
            if (clearHovered || clearPressed) {
                clearHovered = false;
                clearPressed = false;
                unsetCursor();
                update();
            }
        }
        else if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            const QPoint parentPos = lineEdit->mapToParent(mouseEvent->position().toPoint());
            if (mouseEvent->button() == Qt::LeftButton && showsClearButton() && clearButtonRect.contains(parentPos)) {
                clearPressed = true;
                clearHovered = true;
                update();
                mouseEvent->accept();
                return true;
            }
            if (mouseEvent->button() == Qt::LeftButton) {
                focusInnerLineEdit();
            }
        }
        else if (event->type() == QEvent::MouseButtonRelease) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            const QPoint parentPos = lineEdit->mapToParent(mouseEvent->position().toPoint());
            if (clearPressed && mouseEvent->button() == Qt::LeftButton) {
                const bool shouldClear = clearButtonRect.contains(parentPos);
                clearPressed = false;
                clearHovered = shouldClear;
                if (shouldClear) {
                    lineEdit->clear();
                    focusInnerLineEdit();
                }
                update();
                mouseEvent->accept();
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void IconLineEdit::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);

    const int marginLeft = 5;
    const int marginRight = 5;
    const int iconLineEditMargin = 5;
    const int clearButtonSize = 20;
    const int clearButtonSpacing = 4;

    int w = width();
    int h = height();

    int iconX = marginLeft;
    int iconY = (h - iconLabel->height()) / 2;
    iconLabel->setGeometry(iconX, iconY, iconLabel->width(), iconLabel->height());

    int clearX = w - marginRight - clearButtonSize;
    int clearY = (h - clearButtonSize) / 2;
    clearButtonRect = QRect(clearX, clearY, clearButtonSize, clearButtonSize);

    int editX = iconX + iconLabel->width() + iconLineEditMargin;
    int editWidth = qMax(0, clearX - clearButtonSpacing - editX);
    lineEdit->setGeometry(editX, 0, editWidth, h);

    refreshIcons();
}

void IconLineEdit::setIcon(const QString& source) {
    iconSource = source;
    refreshIcons();
}

void IconLineEdit::setIconSize(QSize size) {
    iconLabel->setFixedSize(size);
    refreshIcons();
}

void IconLineEdit::refreshIcons()
{
    iconLabel->setPixmap(ImageService::instance().scaled(iconSource,
                                                         iconLabel->size(),
                                                         Qt::IgnoreAspectRatio,
                                                         devicePixelRatioF()));
}

bool IconLineEdit::showsClearButton() const
{
    return lineEdit && !lineEdit->text().isEmpty();
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
    QWidget::mouseMoveEvent(event);
}

void IconLineEdit::leaveEvent(QEvent* event)
{
    if (clearHovered || clearPressed) {
        clearHovered = false;
        clearPressed = false;
        unsetCursor();
        update();
    }
    QWidget::leaveEvent(event);
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

    QWidget::mousePressEvent(event);
}

void IconLineEdit::mouseReleaseEvent(QMouseEvent* event)
{
    if (clearPressed && event->button() == Qt::LeftButton) {
        const bool shouldClear = clearButtonRect.contains(event->pos());
        clearPressed = false;
        clearHovered = shouldClear;
        if (shouldClear) {
            lineEdit->clear();
            lineEdit->setFocus();
        }
        update();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void IconLineEdit::focusInnerLineEdit()
{
    lineEdit->setFocus();
    lineEdit->setCursorPosition(lineEdit->text().size());
}
