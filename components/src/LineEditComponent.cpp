#include "LineEditComponent.h"
#include <QKeyEvent>
#include <QToolButton>

LineEditComponent::LineEditComponent(QWidget *parent)
        : QWidget(parent)
{
    iconLabel = new QLabel(this);
    iconLabel->setFixedSize(15, 15);
    iconLabel->setScaledContents(true);
    iconLabel->setAttribute(Qt::WA_TranslucentBackground);

    QFont font;
    font.setPixelSize(12);
    lineEdit = new QLineEdit(this);
    lineEdit->installEventFilter(this);
    lineEdit->setFont(font);
    lineEdit->setStyleSheet("QLineEdit{background-color:transparent;} "
                            "QLineEdit:hover{background:transparent;}");
    lineEdit->setFrame(QFrame::NoFrame);
    lineEdit->setFocusPolicy(Qt::ClickFocus);
    lineEdit->setPlaceholderText("搜索");
    clearBtn = new QToolButton(this);
    clearBtn->setAttribute(Qt::WA_TranslucentBackground);
    clearBtn->setCursor(Qt::PointingHandCursor);
    clearBtn->setIcon(QIcon(":/resources/icon/close.png"));
    clearBtn->setIconSize(QSize(20, 20));
    clearBtn->setAutoRaise(true);
    clearBtn->setVisible(false);
    clearBtn->setStyleSheet("QToolButton{background:transparent;} "
                            "QToolButton:hover{background:transparent;}");
    // 信号–槽连接
    connect(clearBtn, &QToolButton::clicked, lineEdit, &QLineEdit::clear);
    // 加载并缓存原始图标
    iconPixmap.load(":/resources/icon/search.png");
}

LineEditComponent::~LineEditComponent() {}


QString LineEditComponent::currentText() const {
    if (this->lineEdit->text().isEmpty())
        return "";
    return this->lineEdit->text();
}

void LineEditComponent::paintEvent(QPaintEvent *ev) {
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
        // 因为画笔宽度是 2，所以把 rect 缩小 1px，保证描边不被裁切
        QRect borderRect = rect().adjusted(1, 1, -1, -1);
        painter.drawRoundedRect(borderRect, radius - 2, radius - 2);
    }
    QWidget::paintEvent(ev);
}

QLineEdit *LineEditComponent::getLineEdit() const {
    return this->lineEdit;
}

void LineEditComponent::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Tab) {
        emit requestNextFocus();
    }
    else {
        QWidget::keyPressEvent(event);
    }
}

bool LineEditComponent::eventFilter(QObject *watched, QEvent *event) {
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
    }
    return QWidget::eventFilter(watched, event);
}

void LineEditComponent::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);

    const int marginLeft = 5;
    const int marginRight = 5;
    const int iconLineEditMargin = 5;
    const int lineEditExtraW = 7;

    int w = width();
    int h = height();

    // iconLabel 固定大小 15×15，垂直居中
    int iconX = marginLeft;
    int iconY = (h - iconLabel->height()) / 2;
    iconLabel->setGeometry(iconX, iconY, iconLabel->width(), iconLabel->height());

    // clearBtn 固定宽高来自 iconSize，垂直居中，右侧留 marginRight
    QSize clearSize = clearBtn->iconSize();
    int clearX = w - marginRight - clearSize.width();
    int clearY = (h - clearSize.height()) / 2;
    clearBtn->setGeometry(clearX, clearY, clearSize.width(), clearSize.height());

    // lineEdit 占据 iconLabel 与 clearBtn 之间的剩余空间
    int editX = iconX + iconLabel->width() + iconLineEditMargin;
    int editWidth = clearX - iconLineEditMargin - editX + lineEditExtraW;
    lineEdit->setGeometry(editX, 0, editWidth, h);

    // 根据布局调整后，再次缩放 iconPixmap 以适应 iconLabel（如果需要适配 DPI）
    QPixmap scaled = iconPixmap.scaled(iconLabel->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    iconLabel->setPixmap(scaled);
}

void LineEditComponent::setIcon(QPixmap icon) {
    iconPixmap = icon;
    resizeEvent(nullptr);
}

void LineEditComponent::setIconSize(QSize size) {
    iconLabel->setFixedSize(size);
    resizeEvent(nullptr);
}
