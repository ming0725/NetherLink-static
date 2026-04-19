#pragma once

#include <QWidget>
#include <QDateTime>
#include <QMenu>
#include <QLabel>
#include <QPushButton>

class AiChatListItem : public QWidget {
    Q_OBJECT
public:
    explicit AiChatListItem(QWidget* parent = nullptr);

    void setTitle(const QString& title);
    QString title() const;

    void setTime(const QDateTime& time);
    QDateTime time() const;

    void setSelected(bool select);
    bool isSelected();

    bool operator<(const AiChatListItem& other) const;

signals:
    void clicked(AiChatListItem*);
    void requestRename(AiChatListItem* item);
    void requestDelete(AiChatListItem* item);
    void timeUpdated(AiChatListItem* item);

protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QString m_title;
    QDateTime m_time;

    bool m_hovered = false;
    bool m_pressed = false;

    QLabel* m_titleLabel;
    QPushButton* m_moreButton;
    QMenu* m_menu;

    void initMenu();
};
