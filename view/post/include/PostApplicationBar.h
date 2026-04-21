#pragma once
#include <QWidget>
#include <QVariantAnimation>
#include <QVector>
#include <QPainter>
#include <QEnterEvent>
#include <QGraphicsBlurEffect>
#include <QPainterPath>

class TextBarItem : public QWidget {
    Q_OBJECT
public:
    TextBarItem(const QString& text, int idx, QWidget* parent)
            : QWidget(parent), label(text), index(idx) {
        setAttribute(Qt::WA_Hover);
    }
    QString label;
    int index;
    bool hovered = false;
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QColor bg = QColor(0,0,0, 0);
        p.setBrush(bg);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(rect(), 10, 10);
        QColor textColor = Qt::black;
        p.setPen(textColor);
        p.drawText(rect(), Qt::AlignCenter, label);
    }
    void mousePressEvent(QMouseEvent*) override {
        emit clicked(index);
    }
    void enterEvent(QEnterEvent*) override {
        hovered = true;
        setCursor(Qt::PointingHandCursor);
    }
    void leaveEvent(QEvent*) override {
        hovered = false;
        unsetCursor();
    }
signals:
    void clicked(int index);
};

class PostApplicationBar : public QWidget {
    Q_OBJECT
public:
    explicit PostApplicationBar(QWidget* parent = nullptr);
    QSize minimumSizeHint() const Q_DECL_OVERRIDE;
    QSize sizeHint() const Q_DECL_OVERRIDE;
    void enableBlur(bool enabled = true);
    void setCurrentIndex(int index);

signals:
    void pageClicked(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void updateBlurBackground();

private slots:
    void onItemClicked(int index);
    void onHighlightValueChanged(const QVariant &value);

private:
    QVector<TextBarItem*> items;
    TextBarItem* selectedItem = nullptr;
    QVariantAnimation* highlightAnim;
    int highlightX = 0;
    const int itemHeight = 32;
    const int spacing = 0;
    const int margin = 6;
    void initItems();
    void layoutItems();
private:
    QWidget* m_parent;
    QTimer* m_updateTimer;
    QImage m_blurredBackground;
    QRect selectedRect;
    bool isEnableBlur = true;
    QSize preSize;
};
