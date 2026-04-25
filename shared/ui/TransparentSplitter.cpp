#include "TransparentSplitter.h"

#include <QPaintEvent>
#include <QSplitterHandle>

namespace {

class TransparentSplitterHandle final : public QSplitterHandle
{
public:
    TransparentSplitterHandle(Qt::Orientation orientation, QSplitter* parent)
        : QSplitterHandle(orientation, parent)
    {
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);
    }
};

} // namespace

TransparentSplitter::TransparentSplitter(Qt::Orientation orientation, QWidget* parent)
    : QSplitter(orientation, parent)
{
}

QSplitterHandle* TransparentSplitter::createHandle()
{
    return new TransparentSplitterHandle(orientation(), this);
}
