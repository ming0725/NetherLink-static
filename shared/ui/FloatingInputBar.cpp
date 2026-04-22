#include "FloatingInputBar.h"
#include <QPainter>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QScrollBar>
#include <QKeyEvent>
#include <QPainterPath>
#include <QGraphicsDropShadowEffect>

const QString FloatingInputBar::RESOURCE_PATH = ":/resources/icon/";

FloatingInputBar::FloatingInputBar(QWidget *parent)
        : QWidget(parent)
{
    setAutoFillBackground(false);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::FramelessWindowHint);

    initUI();

    // 创建提示框
    m_tooltip = new CustomTooltip(this);
    m_tooltip->hide();

    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(40);
    shadow->setOffset(0, 0);
    shadow->setColor(QColor(0, 0, 0, 120));
    setGraphicsEffect(shadow);
}

FloatingInputBar::~FloatingInputBar()
{
}

void FloatingInputBar::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, VERTICAL_MARGIN, 0, VERTICAL_MARGIN);
    mainLayout->setSpacing(TOOLBAR_INPUT_SPACING);

    // 顶部工具栏
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setContentsMargins(TOOLBAR_LEFT_MARGIN, 0, RIGHT_MARGIN + 4, 0);
    toolbarLayout->setSpacing(BUTTON_SPACING);

    // 创建标签按钮
    m_emojiLabel = new QLabel(this);
    m_imageLabel = new QLabel(this);
    m_screenshotLabel = new QLabel(this);
    m_historyLabel = new QLabel(this);

    // 设置标签大小
    QSize buttonSize(BUTTON_SIZE, BUTTON_SIZE);
    m_emojiLabel->setFixedSize(buttonSize);
    m_imageLabel->setFixedSize(buttonSize);
    m_screenshotLabel->setFixedSize(buttonSize);
    m_historyLabel->setFixedSize(buttonSize);

    // 设置标签图标
    QSize iconSize(ICON_SIZE, ICON_SIZE);
    updateLabelIcon(m_emojiLabel, "skull.png", "hovered_skull.png", iconSize);
    updateLabelIcon(m_imageLabel, "painting.png", "hovered_painting.png", iconSize);
    updateLabelIcon(m_screenshotLabel, "shears.png", "hovered_shears.png", iconSize);
    updateLabelIcon(m_historyLabel, "clock.png", "hovered_clock.png", iconSize);

    // 设置鼠标样式
    m_emojiLabel->setCursor(Qt::PointingHandCursor);
    m_imageLabel->setCursor(Qt::PointingHandCursor);
    m_screenshotLabel->setCursor(Qt::PointingHandCursor);
    m_historyLabel->setCursor(Qt::PointingHandCursor);

    m_emojiLabel->installEventFilter(this);
    m_imageLabel->installEventFilter(this);
    m_screenshotLabel->installEventFilter(this);
    m_historyLabel->installEventFilter(this);

    // 添加标签到工具栏
    toolbarLayout->addWidget(m_emojiLabel);
    toolbarLayout->addWidget(m_imageLabel);
    toolbarLayout->addWidget(m_screenshotLabel);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(m_historyLabel);

    // 创建输入框容器（用于设置不同的左边距）
    QHBoxLayout *inputLayout = new QHBoxLayout();
    inputLayout->setContentsMargins(INPUT_LEFT_MARGIN, 0, 5, 0);
    inputLayout->setSpacing(0);

    // 创建输入框
    m_inputEdit = new QTextEdit(this);
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

    // 设置文本渲染提示
    QFont font = m_inputEdit->font();
    font.setHintingPreference(QFont::PreferFullHinting);
    font.setStyleStrategy(QFont::PreferAntialias);
    m_inputEdit->setFont(font);

    inputLayout->addWidget(m_inputEdit);

    // 添加到主布局
    mainLayout->addLayout(toolbarLayout);
    mainLayout->addLayout(inputLayout);

    setLayout(mainLayout);

    // 创建悬浮发送按钮
    m_sendLabel = new QLabel(this);
    m_sendLabel->setFixedSize(SEND_BUTTON_SIZE, SEND_BUTTON_SIZE);
    m_sendLabel->setCursor(Qt::PointingHandCursor);
    updateLabelIcon(m_sendLabel, "book_and_pen.png", "hovered_book_and_pen.png",
                    QSize(SEND_BUTTON_SIZE, SEND_BUTTON_SIZE));
    m_sendLabel->installEventFilter(this);
    m_sendLabel->raise();  // 确保发送按钮显示在最上层
}


void FloatingInputBar::updateLabelIcon(QLabel *label,
                                       const QString &normalIcon,
                                       const QString &hoveredIcon,
                                       const QSize &size)
{
    // 加载并缩放正常状态图标
    QPixmap normalPixmap(RESOURCE_PATH + normalIcon);
    qreal ratio = qMin(size.width() / (qreal)normalPixmap.width(),
                       size.height() / (qreal)normalPixmap.height());
    QSize targetSize(normalPixmap.width() * ratio, normalPixmap.height() * ratio);

    QPixmap scaledNormal = normalPixmap.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // 创建一个空白的pixmap作为背景
    QPixmap finalIcon(size.width(), size.height());
    finalIcon.fill(Qt::transparent);

    // 在中心位置绘制图标
    QPainter painter(&finalIcon);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawPixmap(
            (size.width() - targetSize.width()) / 2,
            (size.height() - targetSize.height()) / 2,
            scaledNormal
    );

    // 保存悬停状态的图标路径
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
    QPixmap pixmap(hoveredIcon);
    qreal ratio = qMin(size.width() / (qreal)pixmap.width(),
                       size.height() / (qreal)pixmap.height());
    QSize targetSize(pixmap.width() * ratio, pixmap.height() * ratio);

    QPixmap finalIcon(size.width(), size.height());
    finalIcon.fill(Qt::transparent);

    QPainter painter(&finalIcon);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawPixmap(
            (size.width() - targetSize.width()) / 2,
            (size.height() - targetSize.height()) / 2,
            pixmap.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)
    );

    label->setPixmap(finalIcon);
}

void FloatingInputBar::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(rect(), CORNER_RADIUS, CORNER_RADIUS);
    painter.fillPath(path, QColor(255, 255, 255, 120));
}

void FloatingInputBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    // 更新发送按钮位置
    if (m_sendLabel) {
        m_sendLabel->move(
                width() - SEND_BUTTON_SIZE - RIGHT_MARGIN,
                height() - SEND_BUTTON_SIZE - VERTICAL_MARGIN
        );
        m_sendLabel->raise();  // 确保发送按钮始终在最上层
    }
}

bool FloatingInputBar::eventFilter(QObject *watched, QEvent *event)
{
    // 处理输入框事件
    if (watched == m_inputEdit) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_BracketLeft && keyEvent->modifiers() == Qt::NoModifier) {
                sendCurrentMessage(SendMode::Peer);
                return true;
            }
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                if (!(keyEvent->modifiers() & Qt::ShiftModifier)) {
                    sendCurrentMessage();
                    return true;
                }
            }
        }
    }

    // 处理标签悬停效果
    QLabel* label = qobject_cast<QLabel*>(watched);
    if (label) {
        if (event->type() == QEvent::Enter) {
            QString hoveredIcon = label->property("hoveredIcon").toString();
            QString normalIcon = label->property("normalIcon").toString();
            QSize iconSize = label->property("iconSize").toSize();
            handleLabelHover(label, normalIcon, hoveredIcon, iconSize);

            // 显示对应的提示文本
            if (label == m_emojiLabel) {
                showTooltip(label, "表情");
            } else if (label == m_imageLabel) {
                showTooltip(label, "图片");
            } else if (label == m_screenshotLabel) {
                showTooltip(label, "截图");
            } else if (label == m_historyLabel) {
                showTooltip(label, "历史");
            } else if (label == m_sendLabel) {
                showTooltip(label, "发送");
            }
        } else if (event->type() == QEvent::Leave) {
            QString hoveredIcon = label->property("hoveredIcon").toString();
            QString normalIcon = label->property("normalIcon").toString();
            QSize iconSize = label->property("iconSize").toSize();
            handleLabelHover(label, hoveredIcon, normalIcon, iconSize);
            hideTooltip();
        }
    }

    return QWidget::eventFilter(watched, event);
}

void FloatingInputBar::mousePressEvent(QMouseEvent *event)
{
    QPoint pos = event->pos();
    if (m_sendLabel && m_sendLabel->geometry().contains(pos)) {
        sendCurrentMessage();
    } else if (m_imageLabel->geometry().contains(pos)) {
        QString filePath = QFileDialog::getOpenFileName(this,
                                                        tr("选择图片"), "",
                                                        tr("图片文件 (*.png *.jpg *.jpeg *.bmp)"));
        if (!filePath.isEmpty()) {
            emit sendImage(filePath);
        }
    }
    QWidget::mousePressEvent(event);
}


void FloatingInputBar::sendCurrentMessage(SendMode mode)
{
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

void FloatingInputBar::showTooltip(QLabel *label, const QString &text)
{
    if (!label) return;

    m_tooltip->setText(text);

    // 计算提示框位置
    QPoint labelPos = label->mapToGlobal(QPoint(0, 0));
    QPoint tooltipPos(labelPos.x() + label->width()/2 - m_tooltip->width()/2,
                      labelPos.y());

    m_tooltip->showTooltip(tooltipPos);
}

void FloatingInputBar::hideTooltip()
{
    m_tooltip->hide();
}
