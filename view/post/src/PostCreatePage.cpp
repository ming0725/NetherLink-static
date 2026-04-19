#include "PostCreatePage.h"
#include "NotificationManager.h"
#include "CurrentUser.h"
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
    setAttribute(Qt::WA_TranslucentBackground);
}

void PostCreatePage::setupUI()
{
    // 创建标题输入框
    m_titleEdit = new LineEditComponent(this);
    m_titleEdit->setIcon(QPixmap(":/resources/icon/blazer.png"));
    m_titleEdit->setIconSize(QSize(20, 20));
    m_titleEdit->getLineEdit()->setPlaceholderText("请输入标题");
    m_titleEdit->setFixedHeight(40);

    // 创建内容输入框
    m_contentEdit = new QTextEdit(this);
    m_contentEdit->setPlaceholderText("请输入内容...");
    m_contentEdit->setStyleSheet(
        "QTextEdit {"
        "   background-color: #F5F5F5;"
        "   border: none;"
        "   border-radius: 10px;"
        "   padding: 10px;"
        "   font-size: 14px;"
        "}"
        "QTextEdit:focus {"
        "   background-color: #FFFFFF;"
        "   border: 2px solid #0099FF;"
        "}"
    );

    // 创建图片按钮
    m_imageButton = new QPushButton(this);
    m_imageButton->setText("+");
    m_imageButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #F5F5F5;"
        "   border: 2px dashed #CCCCCC;"
        "   border-radius: 10px;"
        "   font-size: 24px;"
        "   color: #999999;"
        "}"
        "QPushButton:hover {"
        "   background-color: #EBEBEB;"
        "   border-color: #999999;"
        "}"
    );
    m_imageButton->setFixedSize(100, 100);
    m_imageButton->setCursor(Qt::PointingHandCursor);

    // 创建图片预览标签
    m_imagePreview = new QLabel(this);
    m_imagePreview->setFixedSize(100, 100);
    m_imagePreview->setScaledContents(true);
    m_imagePreview->hide();

    // 创建发送按钮
    m_sendButton = new QPushButton("发布", this);
    m_sendButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #0099FF;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 5px;"
        "   padding: 8px 20px;"
        "   font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #0088EE;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #0077DD;"
        "}"
    );
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
    QPixmap pixmap(path);
    m_imagePreview->setPixmap(pixmap);
    m_imagePreview->show();
    m_imageButton->hide();
}
