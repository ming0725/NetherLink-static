#pragma once

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QImage>
#include "User.h"

class FriendListItem : public QWidget
{
    Q_OBJECT
public:
    explicit FriendListItem(const User& user, QWidget* parent = nullptr);
    QSize sizeHint() const override;
    void setSelected(bool select);
    bool isSelected();
    UserStatus getUserStatus() { return status; }
    QString getUserName() { return fullNameText; }

protected:
    void paintEvent(QPaintEvent*) Q_DECL_OVERRIDE;
    void resizeEvent(QResizeEvent*) Q_DECL_OVERRIDE;
    void enterEvent(QEnterEvent*) Q_DECL_OVERRIDE;
    void leaveEvent(QEvent*) Q_DECL_OVERRIDE;
    void mousePressEvent(QMouseEvent*) Q_DECL_OVERRIDE;
signals:
    void itemClicked(FriendListItem* item);

private:
    QLabel* avatarLabel;
    QLabel* nameLabel;
    QLabel* statusIconLabel;
    QLabel* statusTextLabel;
    QString fullNameText;
    QString fullStatusAndSignText;
    UserStatus status;

    bool hovered = false;
    bool selected = false;

    void setupUI(const User& user);
};
