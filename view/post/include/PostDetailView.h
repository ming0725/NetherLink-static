#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QJsonObject>
#include "LineEditComponent.h"
#include "CustomScrollArea.h"
#include "RepositoryTypes.h"

class PostDetailScrollArea : public CustomScrollArea {
    Q_OBJECT
protected:
    void layoutContent() override;
public:
    explicit PostDetailScrollArea(QWidget* parent = nullptr);
    void setLabels(QLabel* titleLabel, QLabel* contentLabel);
    void addCommentWidget(QWidget* commentWidget);
    QLabel* m_titleLabel = nullptr;
    QLabel* m_contentLabel = nullptr;
private:
    QList<QWidget*> m_commentWidgets;
};

class PostDetailView : public QWidget {
    Q_OBJECT
public:
    explicit PostDetailView(QWidget* parent = nullptr);
    void setPostData(const PostDetailData& data);
    void setInitialGeometry(const QRect& geometry) { m_initialGeometry = geometry; }
    QRect initialGeometry() const { return m_initialGeometry; }

signals:
    void closed();
    void followClicked(bool followed);
    void likeClicked(bool liked);
    void commentClicked();
protected:
    void resizeEvent(QResizeEvent* ev) override;
    void paintEvent(QPaintEvent*) override;
    void showEvent(QShowEvent*) override;
private:
    void setupUI();
    void updateLayout();
    void addComment(const QString& content);
    QWidget* createCommentWidget(const QString& userName, const QString& content);
    QRect m_initialGeometry;
    bool m_isFirstShow = true;
private:
    QString m_imageSource;
    QLabel* m_authorAvatar;
    QLabel* m_authorName;
    QPushButton* m_followBtn;
    CustomScrollArea* m_contentArea;
    QWidget* m_contentWidget;
    QLabel* m_titleLabel;
    QLabel* m_contentLabel;
    QPushButton* m_likeBtn;
    QLabel* m_likeCount;
    QPushButton* m_commentBtn;
    QLabel* m_commentCount;
    QString m_postId;
    QString m_authorId;
    LineEditComponent* commentLineEdit;
    bool m_isFollowed = false;
    bool m_isLiked = false;
    int m_likes = 0;
    int m_comments = 0;
};
