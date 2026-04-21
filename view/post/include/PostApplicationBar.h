#pragma once

#include <QPainterPath>
#include <QTimer>
#include <QVariantAnimation>
#include <QVector>
#include <QWidget>

class PostApplicationBar : public QWidget {
    Q_OBJECT
public:
    explicit PostApplicationBar(QWidget* parent = nullptr);
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;
    void enableBlur(bool enabled = true);
    void setCurrentIndex(int index);

signals:
    void pageClicked(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void updateBlurBackground();

private slots:
    void onItemClicked(int index);
    void onHighlightValueChanged(const QVariant& value);

private:
    struct TabItem {
        QString label;
        QRect rect;
        int widthHint = 0;
    };

    void initItems();
    void layoutItems();
    void updateSelectedRect();
    int indexAtPosition(const QPoint& pos) const;

    QVector<TabItem> items;
    int selectedIndex = 0;
    int hoveredIndex = -1;
    QVariantAnimation* highlightAnim = nullptr;
    int highlightX = 0;
    const int itemHeight = 32;
    const int minItemWidth = 100;
    const int spacing = 0;
    const int margin = 6;

    QWidget* m_parent = nullptr;
    QTimer* m_updateTimer = nullptr;
    QImage m_blurredBackground;
    QRect selectedRect;
    bool isEnableBlur = true;
    QSize preSize;
};
