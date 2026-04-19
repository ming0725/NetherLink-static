#pragma once
#include <QWidget>
#include <QPropertyAnimation>

class ApplicationBarItem;
class ApplicationBar : public QWidget {
    Q_OBJECT
public:
    ApplicationBar(QWidget* parent = nullptr);
    void addItem(ApplicationBarItem*);
    void addBottomItem(ApplicationBarItem*);
    void setAvatar(QPixmap);
    int topItemsCount() const { return topItems.size(); }
    int indexOfTopItem(ApplicationBarItem* item) const {
        return topItems.indexOf(item);
    }
protected:
    void paintEvent(QPaintEvent*) Q_DECL_OVERRIDE;
    void resizeEvent(QResizeEvent*) Q_DECL_OVERRIDE;
private slots:
    void onItemClicked(ApplicationBarItem*);
signals:
    void applicationClicked(ApplicationBarItem*);
private:
    void layoutItems();
    ApplicationBarItem* selectedItem = nullptr;
    QPixmap avatarPixmap;
    QImage noiseTexture;
    QVector<ApplicationBarItem*> topItems;
    QVector<ApplicationBarItem*> bottomItems;
    const int marginTop     = 20;
    const int marginBottom  = 10;
    const int iconSize      = 40;
    const int spacing       = 5;
    int avatarAndItemDist   = 22;
    int avatarSize          = 35;
    int highlightPosY       = 0;
    QVariantAnimation* highlightAnim = nullptr;
};
