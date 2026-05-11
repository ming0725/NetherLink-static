#include "TransparentTextEdit.h"

#include <QFrame>

TransparentTextEdit::TransparentTextEdit(QWidget* parent)
    : QTextEdit(parent)
{
    setFrameShape(QFrame::NoFrame);
    setAutoFillBackground(false);
    document()->setDocumentMargin(0);
    viewport()->setAutoFillBackground(false);
}

void TransparentTextEdit::setViewportPadding(int left, int top, int right, int bottom)
{
    setViewportMargins(left, top, right, bottom);
}
