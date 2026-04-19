#pragma once
#include <QWidget>
#include <QSplitter>
#include "TopSearchWidget.h"
#include "FriendListWidget.h"
#include "DefaultPage.h"

class FriendApplication : public QWidget {
    Q_OBJECT
public:
    explicit FriendApplication(QWidget* parent = nullptr);
protected:
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
private:
    class LeftPane : public QWidget {
    public:
        explicit LeftPane(QWidget* parent = nullptr)
                : QWidget(parent)
                , m_topSearch(new TopSearchWidget(this))
                , m_content(new FriendListWidget(this))
        {
            setMinimumWidth(144);
            setMaximumWidth(305);
            m_content->setStyleSheet("border-width:0px;border-style:solid;");
        }

    protected:
        void resizeEvent(QResizeEvent* ev) override {
            QWidget::resizeEvent(ev);
            int lw = width();
            int lh = height();
            int topH = m_topSearch->height();

            m_topSearch->setGeometry(0, 0, lw, topH);
            m_content->setGeometry(0, topH, lw, lh - topH);
        }

    private:
        TopSearchWidget*  m_topSearch;
        FriendListWidget* m_content;
    };

    LeftPane*    m_leftPane;     // 左侧面板
    DefaultPage* m_defaultPage;  // 右侧默认页
    QSplitter*   m_splitter;     // 中间分隔器
};
