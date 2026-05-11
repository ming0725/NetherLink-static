#pragma once

#include <QTextEdit>

class TransparentTextEdit : public QTextEdit
{
    Q_OBJECT

public:
    explicit TransparentTextEdit(QWidget* parent = nullptr);

    void setViewportPadding(int left, int top, int right, int bottom);
};
