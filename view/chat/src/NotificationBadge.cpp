// NotificationBadge.cpp
#include "NotificationBadge.h"
#include <QPainter>
#include <QFontMetrics>
#include <QFont>

NotificationBadge::NotificationBadge(QWidget *parent)
        : QWidget(parent)
{
    m_bgColor = QColor(0xf74c30);
    m_textColor = QColor(0xffffff);
    m_dndIcon = QPixmap(":/resources/icon/notification.png");
    m_dndSelectedIcon = QPixmap(":/resources/icon/selected_notification.png");
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
}

void NotificationBadge::setCount(int count) {
    m_count = count;
    updateGeometry();
    update();
}

void NotificationBadge::setDoNotDisturb(bool dnd) {
    m_dnd = dnd;
    if (m_dnd) {
        m_bgColor = QColor(0xcccccc);
        m_textColor = QColor(0xfffafa);
    }
    else {
        m_bgColor = QColor(0xf74c30);
        m_textColor = QColor(0xffffff);
    }
    update();
}

void NotificationBadge::setDndIcon(const QPixmap &pix) {
    m_dndIcon = pix;
    update();
}

void NotificationBadge::setSingleSize(int d) {
    m_singleSize = d;
    updateGeometry();
    update();
}

void NotificationBadge::setTwoDigitWidth(int w) {
    m_twoDigitWidth = w;
    updateGeometry();
    update();
}

void NotificationBadge::setPlusWidth(int w) {
    m_plusWidth = w;
    updateGeometry();
    update();
}

QSize NotificationBadge::sizeHint() const {
    if (m_dnd && m_count == 0) {
        return QSize(m_singleSize + 7, m_singleSize);
    }
    if (m_count <= 0) {
        return QSize(0, 0);
    }
    if (m_count < 10) {
        return QSize(m_singleSize, m_singleSize);
    }
    if (m_count < 100) {
        return QSize(m_twoDigitWidth, m_singleSize);
    }
    return QSize(m_plusWidth, m_singleSize);
}

void NotificationBadge::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    if (m_dnd && m_count == 0) {
        // 缩放图标并绘制
        if (!m_selected)
            p.drawPixmap(0, 0,
                     m_singleSize, m_singleSize,
                     m_dndIcon.scaled(m_singleSize, m_singleSize,
                                      Qt::KeepAspectRatio,
                                      Qt::SmoothTransformation));
        else
            p.drawPixmap(0, 0,
                         m_singleSize, m_singleSize,
                         m_dndSelectedIcon.scaled(m_singleSize, m_singleSize,
                                          Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation));
        return;
    }
    if (m_count <= 0) return;

    QString text = (m_count < 100 ? QString::number(m_count) : QStringLiteral("99+"));

    // 背景
    p.setBrush(m_bgColor);
    p.setPen(Qt::NoPen);
    if (m_count < 10) {
        p.drawEllipse(0, 0, m_singleSize, m_singleSize);
    }
    else {
        p.drawRoundedRect(0, 0, sizeHint().width(), kHeight, kHeight / 2.0, kHeight / 2.0);
    }

    // 文字
    QFont f = font();
    f.setBold(true);
    p.setFont(f);
    p.setPen(m_textColor);

    QFontMetrics fm(f);
    int tw = fm.horizontalAdvance(text);
    int th = fm.height();
    int x = (sizeHint().width() - tw) / 2;
    int y = (kHeight + th) / 2 - fm.descent();
    p.drawText(x, y, text);
}

void NotificationBadge::setSelected(bool select) {
    m_selected = select;
    update();
}
