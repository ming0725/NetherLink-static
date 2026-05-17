#pragma once

#include <QVariantAnimation>
#include <QVector>
#include <QWidget>

class QtFallbackLiquidGlassController;

class PostApplicationBar : public QWidget {
    Q_OBJECT
public:
    explicit PostApplicationBar(QWidget* parent = nullptr);
    ~PostApplicationBar() override;
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;
    void setCurrentIndex(int index);
    void setVisualOpacity(qreal opacity);
    void refreshPlatformAppearance();
    void setLiquidGlassSourceWidget(QWidget* widget);
    void scheduleLiquidGlassUpdate(int delayMs = 0);
    void scheduleLiquidGlassInteractiveUpdate();
    qreal visualOpacity() const { return m_visualOpacity; }
    bool usesNativeBar() const { return m_usesNativeBar; }
    bool usesQtFallbackLiquidGlass() const;

signals:
    void pageClicked(int index);

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void onItemClicked(int index);
    void onHighlightValueChanged(const QVariant& value);
    void onNativeSelectionChanged(int index);

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
    void syncPlatformBar();
    void updatePanelShadow();
    bool shouldUseQtFallbackLiquidGlass() const;
    void updateQtFallbackLiquidGlassState();
    void releaseQtFallbackLiquidGlassResources(bool updateWidget = true);
    QStringList labels() const;

    QtFallbackLiquidGlassController* m_liquidGlass = nullptr;
    QVector<TabItem> items;
    int selectedIndex = 0;
    int hoveredIndex = -1;
    QVariantAnimation* highlightAnim = nullptr;
    int highlightX = 0;
    const int itemHeight = 32;
    const int minItemWidth = 100;
    const int spacing = 0;
    const int margin = 6;

    QRect selectedRect;
    qreal m_visualOpacity = 1.0;
    bool m_usesNativeBar = false;
};
