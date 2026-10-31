#pragma once

#include <QRect>

class QPainter;

namespace MediaPlaceholderRenderer {

void drawAvatar(QPainter* painter, const QRect& rect);
void drawImage(QPainter* painter, const QRect& rect, int radius);

} // namespace MediaPlaceholderRenderer
