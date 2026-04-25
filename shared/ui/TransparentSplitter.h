#pragma once

#include <QSplitter>

class TransparentSplitter : public QSplitter
{
public:
    explicit TransparentSplitter(Qt::Orientation orientation, QWidget* parent = nullptr);

protected:
    QSplitterHandle* createHandle() override;
};
