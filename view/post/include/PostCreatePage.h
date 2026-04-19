#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include "LineEditComponent.h"

class PostCreatePage : public QWidget {
    Q_OBJECT
public:
    explicit PostCreatePage(QWidget* parent = nullptr);
protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onImageButtonClicked();
    void handleImageSelected(const QString& path);
private:
    void setupUI();

    LineEditComponent* m_titleEdit;
    QTextEdit* m_contentEdit;
    QPushButton* m_imageButton;
    QPushButton* m_sendButton;
    QLabel* m_imagePreview;
    QString m_selectedImagePath;
    
    static const int CORNER_RADIUS = 20;
    static const int MARGIN = 20;
    static const int SPACING = 15;
}; 