// NotificationBadge.h
#pragma once
#include <QWidget>
#include <QPixmap>
#include <QColor>

class NotificationBadge : public QWidget {
    Q_OBJECT
public:
    explicit NotificationBadge(QWidget* parent = nullptr);

    void setCount(int count);
    void setDoNotDisturb(bool dnd);
    void setDndIcon(const QPixmap& pix);

    void setSingleSize(int d);
    void setTwoDigitWidth(int w);
    void setPlusWidth(int w);
    void setSelected(bool select);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* ev) override;

private:
    int     m_count           = 0;
    bool    m_dnd             = false;
    bool    m_selected = false;
    QPixmap m_dndIcon;
    QPixmap m_dndSelectedIcon;
    QColor  m_bgColor;
    QColor  m_textColor;

    int     m_singleSize      = 16;
    int     m_twoDigitWidth   = 24;
    int     m_plusWidth       = 32;
    static constexpr int kHeight = 16;
};
