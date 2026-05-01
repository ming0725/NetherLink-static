#include "PostCreatePage.h"
#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"
#include "app/frame/NotificationManager.h"
#include "app/state/CurrentUser.h"
#include <QVBoxLayout>
#include <QFileDialog>
#include <QPainter>
#include <QPainterPath>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QMimeDatabase>


PostCreatePage::PostCreatePage(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
#ifdef Q_OS_WIN
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(true);
    QPalette palette = this->palette();
    palette.setColor(QPalette::Window, ThemeManager::instance().color(ThemeColor::WindowBackground));
    setPalette(palette);
#else
    setAttribute(Qt::WA_TranslucentBackground);
#endif
}

void PostCreatePage::setupUI()
{
    // 创建标题输入框
    m_titleEdit = new IconLineEdit(this);
    m_titleEdit->setIcon(QStringLiteral(":/resources/icon/blazer.png"));
    m_titleEdit->setIconSize(QSize(20, 20));
    m_titleEdit->getLineEdit()->setPlaceholderText("请输入标题");
    m_titleEdit->setFixedHeight(40);

    // 创建内容输入框
    m_contentEdit = new QTextEdit(this);
    m_contentEdit->setPlaceholderText("请输入内容...");
    QPalette contentPalette = m_contentEdit->palette();
    contentPalette.setColor(QPalette::Text, ThemeManager::instance().color(ThemeColor::PrimaryText));
    contentPalette.setColor(QPalette::PlaceholderText,
                            ThemeManager::instance().color(ThemeColor::PlaceholderText));
    contentPalette.setColor(QPalette::Highlight, ThemeManager::instance().color(ThemeColor::Accent));
    contentPalette.setColor(QPalette::HighlightedText, Qt::white);
    m_contentEdit->setPalette(contentPalette);
    m_contentEdit->setStyleSheet(QStringLiteral(
        "QTextEdit {"
        "   color: %1;"
        "   background-color: %2;"
        "   border: none;"
        "   border-radius: 10px;"
        "   padding: 10px;"
        "   font-size: 14px;"
        "}"
        "QTextEdit:focus {"
        "   background-color: %3;"
        "   border: 2px solid %4;"
        "}"
    ).arg(ThemeManager::instance().color(ThemeColor::PrimaryText).name(),
          ThemeManager::instance().color(ThemeColor::InputBackground).name(),
          ThemeManager::instance().color(ThemeColor::InputFocusBackground).name(),
          ThemeManager::instance().color(ThemeColor::Accent).name()));

    // 创建图片按钮
    m_imageButton = new QPushButton(this);
    m_imageButton->setText("+");
    m_imageButton->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "   background-color: %1;"
        "   border: 2px dashed %2;"
        "   border-radius: 10px;"
        "   font-size: 24px;"
        "   color: %3;"
        "}"
        "QPushButton:hover {"
        "   background-color: %4;"
        "   border-color: %3;"
        "}"
    ).arg(ThemeManager::instance().color(ThemeColor::InputBackground).name(),
          ThemeManager::instance().color(ThemeColor::Divider).name(),
          ThemeManager::instance().color(ThemeColor::TertiaryText).name(),
          ThemeManager::instance().color(ThemeColor::ListHover).name()));
    m_imageButton->setFixedSize(100, 100);
    m_imageButton->setCursor(Qt::PointingHandCursor);

    // 创建图片预览标签
    m_imagePreview = new QLabel(this);
    m_imagePreview->setFixedSize(100, 100);
    m_imagePreview->setScaledContents(true);
    m_imagePreview->hide();

    // 创建发送按钮
    m_sendButton = new QPushButton("发布", this);
    m_sendButton->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "   background-color: %1;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 5px;"
        "   padding: 8px 20px;"
        "   font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "   background-color: %2;"
        "}"
        "QPushButton:pressed {"
        "   background-color: %3;"
        "}"
    ).arg(ThemeManager::instance().color(ThemeColor::Accent).name(),
          ThemeManager::instance().color(ThemeColor::AccentHover).name(),
          ThemeManager::instance().color(ThemeColor::AccentPressed).name()));
    m_sendButton->setCursor(Qt::PointingHandCursor);

    // 连接信号槽
    connect(m_imageButton, &QPushButton::clicked, this, &PostCreatePage::onImageButtonClicked);
}

void PostCreatePage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    
    int w = width();
    int h = height() - 32;
    
    // 标题输入框
    m_titleEdit->setGeometry(MARGIN, MARGIN, w - 2 * MARGIN, 40);
    
    // 内容输入框
    int contentY = MARGIN + 40 + SPACING;
    m_contentEdit->setGeometry(MARGIN, contentY, w - 2 * MARGIN, h - contentY - 100 - 2 * MARGIN);
    
    // 图片按钮和预览
    int imageY = h - MARGIN - 100;
    m_imageButton->setGeometry(MARGIN, imageY, 100, 100);
    m_imagePreview->setGeometry(MARGIN, imageY, 100, 100);
    
    // 发送按钮
    int sendX = w - MARGIN - 100;
    int sendY = h - MARGIN - 40;
    m_sendButton->setGeometry(sendX, sendY, 100, 40);
}

void PostCreatePage::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), ThemeManager::instance().color(ThemeColor::WindowBackground));
}

void PostCreatePage::onImageButtonClicked()
{
    QString filePath = QFileDialog::getOpenFileName(this,
        "选择图片", "", "图片文件 (*.png *.jpg *.jpeg *.bmp)");
    if (!filePath.isEmpty()) {
        handleImageSelected(filePath);
    }
}

void PostCreatePage::handleImageSelected(const QString& path)
{
    m_selectedImagePath = path;
    m_imagePreview->setPixmap(ImageService::instance().centerCrop(path, m_imagePreview->size(), 10));
    m_imagePreview->show();
    m_imageButton->hide();
}
