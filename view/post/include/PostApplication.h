// PostApplication.h
#pragma once

#include <QWidget>
#include <QStackedWidget>
#include <QImage>
#include <QPixmap>
#include "Post.h"

class PostFeedPage;
class PostApplicationBar;
class PostDetailView;
class PostCreatePage;

class PostApplication : public QWidget {
Q_OBJECT
public:
    explicit PostApplication(QWidget* parent = nullptr);
private slots:
    void onPostClickedWithGeometry(Post& post, const QRect& sourceGeometry, const QPixmap& originalImage);
protected:
    void resizeEvent(QResizeEvent* ev) override;
    void paintEvent(QPaintEvent*) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;
private:
    void onPageChanged(int index);
    PostApplicationBar*   m_bar;
    QWidget* m_overlay = nullptr;
    QStackedWidget*       m_stack;
    QImage noiseTexture;
    PostDetailView* m_detailView;
    PostCreatePage* m_createPage;
};
