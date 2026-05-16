#include "NetherLinkCreditsWindow.h"

#include <QBrush>
#include <QFont>
#include <QHideEvent>
#include <QImageReader>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QRadialGradient>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>

#include <cmath>

namespace {

const QString kBackgroundSource(QStringLiteral(":/resources/icon/options_background.png"));
const QString kLogoSource(QStringLiteral(":/resources/icon/netherlink.png"));

constexpr int kFrameIntervalMs = 33;
constexpr int kRevealDurationMs = 2400;
constexpr int kCreditsPauseAfterRevealMs = 0;
constexpr qreal kBackgroundSpeed = 15;
constexpr qreal kBackgroundTileScale = 1.85;
constexpr qreal kCreditsSpeed = 30.0;
constexpr qreal kMaxCreditsWidth = 620.0;

constexpr int kRevealTargetOverlayAlpha = 152;
constexpr qreal kRevealDistanceDelay = 0.38;

constexpr qreal kEdgeShadowWidthRatio = 0.22;
constexpr qreal kTopEdgeShadowHeightRatio = 0.32;
constexpr qreal kTopEdgeShadowMaxHeight = 220.0;
constexpr int kTopEdgeOuterAlpha = 188;
constexpr int kTopEdgeMidAlpha = 78;
constexpr qreal kBottomEdgeShadowHeightRatio = 0.27;
constexpr qreal kBottomEdgeShadowMaxHeight = 180.0;
constexpr int kBottomEdgeOuterAlpha = 150;
constexpr int kBottomEdgeMidAlpha = 54;
constexpr int kSideEdgeOuterAlpha = 142;
constexpr int kSideEdgeMidAlpha = 48;

constexpr qreal kCornerVignetteRadiusRatio = 0.28;
constexpr int kCornerVignetteInnerAlpha = 154;
constexpr int kCornerVignetteMidAlpha = 72;
constexpr qreal kCornerVignetteMidStop = 0.54;

qreal easeOutCubic(qreal value)
{
    const qreal t = qBound<qreal>(0.0, value, 1.0) - 1.0;
    return t * t * t + 1.0;
}

bool sameDevicePixelRatio(qreal lhs, qreal rhs)
{
    return std::abs(lhs - rhs) < 0.01;
}

QImage readImageResource(const QString& source)
{
    QImageReader reader(source);
    reader.setAutoTransform(true);
    return reader.read();
}

} // namespace

NetherLinkCreditsWindow::NetherLinkCreditsWindow(QWidget* parent)
    : QWidget(parent)
    , m_frameTimer(new QTimer(this))
{
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_NoMousePropagation);
    setFocusPolicy(Qt::StrongFocus);
    m_clock.start();

    connect(m_frameTimer, &QTimer::timeout, this, [this]() {
        if (isScrollFinished()) {
            close();
            return;
        }

        update();
    });
    buildCredits();
}

void NetherLinkCreditsWindow::hideEvent(QHideEvent* event)
{
    m_frameTimer->stop();
    releaseResources();
    QWidget::hideEvent(event);
}

void NetherLinkCreditsWindow::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        close();
        event->accept();
        return;
    default:
        break;
    }

    QWidget::keyPressEvent(event);
}

void NetherLinkCreditsWindow::mouseDoubleClickEvent(QMouseEvent* event)
{
    event->accept();
}

void NetherLinkCreditsWindow::mousePressEvent(QMouseEvent* event)
{
    event->accept();
}

void NetherLinkCreditsWindow::mouseReleaseEvent(QMouseEvent* event)
{
    event->accept();
}

void NetherLinkCreditsWindow::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    prepareResources();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, false);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    drawBackground(painter);
    drawRevealOverlay(painter);
    drawCredits(painter);
    drawVignette(painter);
}

void NetherLinkCreditsWindow::resizeEvent(QResizeEvent* event)
{
    m_backgroundStrip = QPixmap();
    m_backgroundStripSize = QSize();
    m_creditsPixmap = QPixmap();
    m_creditsPixmapSize = QSize();
    m_totalCreditsHeight = 0.0;
    m_vignettePixmap = QPixmap();
    m_vignetteSize = QSize();
    QWidget::resizeEvent(event);
}

void NetherLinkCreditsWindow::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    prepareResources();
    const qreal dpr = devicePixelRatioF();
    rebuildBackgroundTile(dpr);
    rebuildBackgroundStrip(dpr);
    rebuildCreditsPixmap(dpr);
    rebuildVignette(dpr);
    m_clock.restart();
    m_frameTimer->start(kFrameIntervalMs);
    setFocus(Qt::OtherFocusReason);
}

void NetherLinkCreditsWindow::buildCredits()
{
    auto addLogo = [this]() {
        m_entries.push_back({EntryType::Logo, {}, {}, 0});
    };
    auto addYellow = [this](const QString& text) {
        m_entries.push_back({EntryType::YellowLine, text, {}, 0});
    };
    auto addSection = [this](const QString& text) {
        m_entries.push_back({EntryType::Section, text, {}, 0});
    };
    auto addRole = [this](const QString& role, const QString& name) {
        m_entries.push_back({EntryType::RoleName, role, name, 0});
    };
    auto addSpacer = [this](int spacing) {
        m_entries.push_back({EntryType::Spacer, {}, {}, spacing});
    };

    addLogo();
    addSpacer(44);
    addYellow(QStringLiteral("======== NetherLink ========"));
    addYellow(QStringLiteral("鸣谢名单"));
    addYellow(QStringLiteral("愿下界之门始终保持连接"));
    addSpacer(54);

    addSection(QStringLiteral("制作名单"));
    addRole(QStringLiteral("项目构想"), QStringLiteral("ming0725"));
    addRole(QStringLiteral("产品方向"), QStringLiteral("ming0725"));
    addRole(QStringLiteral("界面设计"), QStringLiteral("ming0725"));
    addRole(QStringLiteral("像素视觉"), QStringLiteral("ming0725"));
    addRole(QStringLiteral("交互体验"), QStringLiteral("ming0725"));
    addRole(QStringLiteral("主题与窗口效果"), QStringLiteral("ming0725"));
    addRole(QStringLiteral("音效与反馈"), QStringLiteral("ming0725"));
    addRole(QStringLiteral("聊天系统"), QStringLiteral("ming0725"));
    addRole(QStringLiteral("好友系统"), QStringLiteral("ming0725"));
    addRole(QStringLiteral("帖子系统"), QStringLiteral("ming0725"));
    addRole(QStringLiteral("AI 会话体验"), QStringLiteral("ming0725"));
    addRole(QStringLiteral("测试与打磨"), QStringLiteral("ming0725"));
    addSpacer(44);

    addSection(QStringLiteral("编码协作"));
    addRole(QStringLiteral("核心编码"), QStringLiteral("ming0725"));
    addRole(QStringLiteral("代码生成与重构支持"), QStringLiteral("codex"));
    addRole(QStringLiteral("推理与实现辅助"), QStringLiteral("GPT5.5"));
    addSpacer(44);

    addSection(QStringLiteral("第三方库"));
    addRole(QStringLiteral("miniaudio"), QStringLiteral("David Reid"));
    addRole(QStringLiteral("sokol"), QStringLiteral("Andre Weissflog"));
    addSpacer(48);

    addSection(QStringLiteral("特别鸣谢"));
    addRole(QStringLiteral("NetherLink"), QStringLiteral("所有正在连接的人"));
    addSpacer(120);
}

void NetherLinkCreditsWindow::prepareResources()
{
    if (m_resourcesPrepared) {
        return;
    }

    m_backgroundImage = readImageResource(kBackgroundSource);
    const QImage logoImage = readImageResource(kLogoSource);
    m_logoPixmap = logoImage.isNull() ? QPixmap() : QPixmap::fromImage(logoImage);
    m_resourcesPrepared = true;
}

void NetherLinkCreditsWindow::releaseResources()
{
    m_resourcesPrepared = false;
    m_backgroundImage = QImage();
    m_backgroundTile = QPixmap();
    m_backgroundTileSize = QSize();
    m_backgroundTileDevicePixelRatio = 0.0;
    m_backgroundStrip = QPixmap();
    m_backgroundStripSize = QSize();
    m_backgroundStripDevicePixelRatio = 0.0;
    m_logoPixmap = QPixmap();
    m_creditsPixmap = QPixmap();
    m_creditsPixmapSize = QSize();
    m_creditsDevicePixelRatio = 0.0;
    m_totalCreditsHeight = 0.0;
    m_vignettePixmap = QPixmap();
    m_vignetteSize = QSize();
    m_vignetteDevicePixelRatio = 0.0;
}

void NetherLinkCreditsWindow::rebuildBackgroundTile(qreal devicePixelRatio)
{
    const qreal dpr = qMax<qreal>(1.0, devicePixelRatio);
    if (m_backgroundImage.isNull()) {
        m_backgroundTile = QPixmap();
        m_backgroundTileSize = QSize();
        m_backgroundTileDevicePixelRatio = dpr;
        return;
    }

    const qreal tileScale = qMax<qreal>(0.1, kBackgroundTileScale);
    const QSize logicalTileSize(qMax(1, qRound(m_backgroundImage.width() * tileScale)),
                                qMax(1, qRound(m_backgroundImage.height() * tileScale)));
    if (!m_backgroundTile.isNull()
        && m_backgroundTileSize == logicalTileSize
        && sameDevicePixelRatio(m_backgroundTileDevicePixelRatio, dpr)) {
        return;
    }

    const QSize pixelTileSize(qMax(1, qRound(logicalTileSize.width() * dpr)),
                              qMax(1, qRound(logicalTileSize.height() * dpr)));
    QImage scaledTile = m_backgroundImage.scaled(pixelTileSize,
                                                Qt::IgnoreAspectRatio,
                                                Qt::FastTransformation);
    if (scaledTile.format() != QImage::Format_ARGB32_Premultiplied) {
        scaledTile = scaledTile.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }

    m_backgroundTile = QPixmap::fromImage(scaledTile);
    m_backgroundTile.setDevicePixelRatio(dpr);
    m_backgroundTileSize = logicalTileSize;
    m_backgroundTileDevicePixelRatio = dpr;
}

void NetherLinkCreditsWindow::rebuildBackgroundStrip(qreal devicePixelRatio)
{
    const qreal dpr = qMax<qreal>(1.0, devicePixelRatio);
    rebuildBackgroundTile(dpr);
    if (m_backgroundTile.isNull() || m_backgroundTileSize.isEmpty() || width() <= 0 || height() <= 0) {
        m_backgroundStrip = QPixmap();
        m_backgroundStripSize = QSize();
        m_backgroundStripDevicePixelRatio = dpr;
        return;
    }

    const QSize logicalSize(width(), height() + m_backgroundTileSize.height());
    if (!m_backgroundStrip.isNull()
        && m_backgroundStripSize == logicalSize
        && sameDevicePixelRatio(m_backgroundStripDevicePixelRatio, dpr)) {
        return;
    }

    const QSize pixelSize(qMax(1, qRound(logicalSize.width() * dpr)),
                          qMax(1, qRound(logicalSize.height() * dpr)));
    QImage stripImage(pixelSize, QImage::Format_ARGB32_Premultiplied);
    stripImage.fill(QColor(16, 10, 6));

    QPainter stripPainter(&stripImage);
    stripPainter.scale(dpr, dpr);
    stripPainter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    const int tileWidth = m_backgroundTileSize.width();
    const int tileHeight = m_backgroundTileSize.height();
    for (int y = 0; y < logicalSize.height(); y += tileHeight) {
        for (int x = 0; x < logicalSize.width(); x += tileWidth) {
            stripPainter.drawPixmap(QRect(x, y, tileWidth, tileHeight), m_backgroundTile);
        }
    }
    stripPainter.end();

    m_backgroundStrip = QPixmap::fromImage(stripImage);
    m_backgroundStrip.setDevicePixelRatio(dpr);
    m_backgroundStripSize = logicalSize;
    m_backgroundStripDevicePixelRatio = dpr;
}

void NetherLinkCreditsWindow::rebuildCreditsPixmap(qreal devicePixelRatio)
{
    const qreal dpr = qMax<qreal>(1.0, devicePixelRatio);
    if (width() <= 0) {
        m_creditsPixmap = QPixmap();
        m_creditsPixmapSize = QSize();
        m_creditsDevicePixelRatio = dpr;
        m_totalCreditsHeight = 0.0;
        return;
    }

    const qreal totalHeight = totalCreditsHeight();
    const QSize logicalSize(width(), qMax(1, qCeil(totalHeight + 8.0)));
    if (!m_creditsPixmap.isNull()
        && m_creditsPixmapSize == logicalSize
        && sameDevicePixelRatio(m_creditsDevicePixelRatio, dpr)) {
        m_totalCreditsHeight = totalHeight;
        return;
    }

    const QSize pixelSize(qMax(1, qRound(logicalSize.width() * dpr)),
                          qMax(1, qRound(logicalSize.height() * dpr)));
    QImage creditsImage(pixelSize, QImage::Format_ARGB32_Premultiplied);
    creditsImage.fill(Qt::transparent);

    QPainter creditsPainter(&creditsImage);
    creditsPainter.scale(dpr, dpr);
    creditsPainter.setRenderHint(QPainter::Antialiasing, true);
    creditsPainter.setRenderHint(QPainter::TextAntialiasing, false);
    creditsPainter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    qreal y = 0.0;
    for (const CreditEntry& entry : m_entries) {
        const qreal height = entryHeight(entry);
        drawEntry(creditsPainter, entry, y, height);
        y += height;
    }
    creditsPainter.end();

    m_creditsPixmap = QPixmap::fromImage(creditsImage);
    m_creditsPixmap.setDevicePixelRatio(dpr);
    m_creditsPixmapSize = logicalSize;
    m_creditsDevicePixelRatio = dpr;
    m_totalCreditsHeight = totalHeight;
}

void NetherLinkCreditsWindow::rebuildVignette(qreal devicePixelRatio)
{
    const QSize logicalSize = size();
    const qreal dpr = qMax<qreal>(1.0, devicePixelRatio);
    if (logicalSize.isEmpty()) {
        m_vignettePixmap = QPixmap();
        m_vignetteSize = QSize();
        m_vignetteDevicePixelRatio = dpr;
        return;
    }

    if (!m_vignettePixmap.isNull()
        && m_vignetteSize == logicalSize
        && sameDevicePixelRatio(m_vignetteDevicePixelRatio, dpr)) {
        return;
    }

    const QSize pixelSize(qMax(1, qRound(logicalSize.width() * dpr)),
                          qMax(1, qRound(logicalSize.height() * dpr)));
    QImage cacheImage(pixelSize, QImage::Format_ARGB32_Premultiplied);
    cacheImage.fill(Qt::transparent);

    QPainter cachePainter(&cacheImage);
    cachePainter.scale(dpr, dpr);
    cachePainter.setRenderHint(QPainter::Antialiasing, true);
    paintVignetteLayer(cachePainter, QRectF(QPointF(0.0, 0.0), QSizeF(logicalSize)));
    cachePainter.end();

    m_vignettePixmap = QPixmap::fromImage(cacheImage);
    m_vignettePixmap.setDevicePixelRatio(dpr);
    m_vignetteSize = logicalSize;
    m_vignetteDevicePixelRatio = dpr;
}

void NetherLinkCreditsWindow::drawBackground(QPainter& painter)
{
    rebuildBackgroundStrip(painter.device()->devicePixelRatioF());
    if (m_backgroundStrip.isNull() || m_backgroundStripSize.isEmpty() || m_backgroundTileSize.isEmpty()) {
        painter.fillRect(rect(), QColor(16, 10, 6));
        return;
    }

    const int tileHeight = m_backgroundTileSize.height();
    const int offsetY = tileHeight > 0
        ? qRound(std::fmod(m_clock.elapsed() / 1000.0 * kBackgroundSpeed, tileHeight))
        : 0;
    painter.drawPixmap(QRect(0, -offsetY, m_backgroundStripSize.width(), m_backgroundStripSize.height()),
                       m_backgroundStrip);
}

void NetherLinkCreditsWindow::drawCredits(QPainter& painter)
{
    rebuildCreditsPixmap(painter.device()->devicePixelRatioF());
    if (m_creditsPixmap.isNull() || m_creditsPixmapSize.isEmpty()) {
        return;
    }

    painter.drawPixmap(QRect(0,
                             qRound(scrollOffset()),
                             m_creditsPixmapSize.width(),
                             m_creditsPixmapSize.height()),
                       m_creditsPixmap);
}

void NetherLinkCreditsWindow::drawEntry(QPainter& painter, const CreditEntry& entry, qreal y, qreal height)
{
    switch (entry.type) {
    case EntryType::Logo: {
        if (m_logoPixmap.isNull()) {
            drawCenteredText(painter, QStringLiteral("NetherLink"), y, 44, QColor(255, 255, 255), height, true);
            return;
        }

        const qreal sourceWidth = m_logoPixmap.width() / m_logoPixmap.devicePixelRatio();
        const qreal sourceHeight = m_logoPixmap.height() / m_logoPixmap.devicePixelRatio();
        if (sourceWidth <= 0.0 || sourceHeight <= 0.0) {
            return;
        }

        const qreal targetWidth = qMin(qMin(width() * 0.72, kMaxCreditsWidth), sourceWidth);
        const qreal targetHeight = targetWidth * sourceHeight / sourceWidth;
        const QRectF target((width() - targetWidth) / 2.0, y, targetWidth, targetHeight);
        const QRectF shadow = target.translated(4.0, 4.0);
        painter.setOpacity(0.42);
        painter.drawPixmap(shadow, m_logoPixmap, QRectF(0, 0, m_logoPixmap.width(), m_logoPixmap.height()));
        painter.setOpacity(1.0);
        painter.drawPixmap(target, m_logoPixmap, QRectF(0, 0, m_logoPixmap.width(), m_logoPixmap.height()));
        break;
    }
    case EntryType::YellowLine:
        drawCenteredText(painter, entry.primary, y, 23, QColor(255, 255, 85), height, false);
        break;
    case EntryType::Section:
        drawCenteredText(painter, entry.primary, y + 8.0, 23, QColor(255, 255, 85), height, true);
        break;
    case EntryType::RoleName:
        drawCenteredText(painter, entry.primary, y, 17, QColor(170, 170, 170), height / 2.0, false);
        drawCenteredText(painter, entry.secondary, y + 24.0, 19, QColor(255, 255, 255), height / 2.0, false);
        break;
    case EntryType::Spacer:
        break;
    }
}

void NetherLinkCreditsWindow::drawRevealOverlay(QPainter& painter) const
{
    const qreal time = qBound<qreal>(0.0, m_clock.elapsed() / static_cast<qreal>(kRevealDurationMs), 1.0);
    const QRectF bounds(rect());
    if (time >= 1.0) {
        painter.fillRect(bounds, QColor(0, 0, 0, kRevealTargetOverlayAlpha));
        return;
    }

    const QPointF center = bounds.center();
    const qreal radius = std::hypot(bounds.width() / 2.0, bounds.height() / 2.0);

    auto alphaAt = [time](qreal distance) {
        const qreal delay = distance * kRevealDistanceDelay;
        const qreal localTime = qBound<qreal>(0.0, (time - delay) / qMax<qreal>(0.001, 1.0 - delay), 1.0);
        const qreal reveal = easeOutCubic(localTime);
        return qRound(255.0 + (kRevealTargetOverlayAlpha - 255.0) * reveal);
    };

    QRadialGradient gradient(center, radius);
    gradient.setColorAt(0.0, QColor(0, 0, 0, alphaAt(0.0)));
    gradient.setColorAt(0.16, QColor(0, 0, 0, alphaAt(0.16)));
    gradient.setColorAt(0.34, QColor(0, 0, 0, alphaAt(0.34)));
    gradient.setColorAt(0.58, QColor(0, 0, 0, alphaAt(0.58)));
    gradient.setColorAt(0.80, QColor(0, 0, 0, alphaAt(0.80)));
    gradient.setColorAt(1.0, QColor(0, 0, 0, alphaAt(1.0)));
    painter.fillRect(bounds, QBrush(gradient));
}

void NetherLinkCreditsWindow::drawVignette(QPainter& painter)
{
    rebuildVignette(painter.device()->devicePixelRatioF());
    if (m_vignettePixmap.isNull()) {
        paintVignetteLayer(painter, QRectF(rect()));
        return;
    }

    painter.drawPixmap(QRect(QPoint(0, 0), size()), m_vignettePixmap);
}

void NetherLinkCreditsWindow::paintVignetteLayer(QPainter& painter, const QRectF& bounds) const
{
    const qreal edgeWidth = qMin(bounds.width(), bounds.height()) * kEdgeShadowWidthRatio;
    const qreal topHeight = qMin(bounds.height() * kTopEdgeShadowHeightRatio, kTopEdgeShadowMaxHeight);
    const qreal bottomHeight = qMin(bounds.height() * kBottomEdgeShadowHeightRatio, kBottomEdgeShadowMaxHeight);
    const QRectF topRect(bounds.left(), bounds.top(), bounds.width(), topHeight);
    const QRectF bottomRect(bounds.left(), bounds.bottom() - bottomHeight, bounds.width(), bottomHeight);
    const QRectF leftRect(bounds.left(), bounds.top(), edgeWidth, bounds.height());
    const QRectF rightRect(bounds.right() - edgeWidth, bounds.top(), edgeWidth, bounds.height());

    QLinearGradient topGradient(topRect.left(), topRect.top(), topRect.left(), topRect.bottom());
    topGradient.setColorAt(0.0, QColor(0, 0, 0, kTopEdgeOuterAlpha));
    topGradient.setColorAt(0.48, QColor(0, 0, 0, kTopEdgeMidAlpha));
    topGradient.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.fillRect(topRect, QBrush(topGradient));

    QLinearGradient bottomGradient(bottomRect.left(), bottomRect.bottom(), bottomRect.left(), bottomRect.top());
    bottomGradient.setColorAt(0.0, QColor(0, 0, 0, kBottomEdgeOuterAlpha));
    bottomGradient.setColorAt(0.55, QColor(0, 0, 0, kBottomEdgeMidAlpha));
    bottomGradient.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.fillRect(bottomRect, QBrush(bottomGradient));

    QLinearGradient leftGradient(leftRect.left(), leftRect.top(), leftRect.right(), leftRect.top());
    leftGradient.setColorAt(0.0, QColor(0, 0, 0, kSideEdgeOuterAlpha));
    leftGradient.setColorAt(0.55, QColor(0, 0, 0, kSideEdgeMidAlpha));
    leftGradient.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.fillRect(leftRect, QBrush(leftGradient));

    QLinearGradient rightGradient(rightRect.right(), rightRect.top(), rightRect.left(), rightRect.top());
    rightGradient.setColorAt(0.0, QColor(0, 0, 0, kSideEdgeOuterAlpha));
    rightGradient.setColorAt(0.55, QColor(0, 0, 0, kSideEdgeMidAlpha));
    rightGradient.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.fillRect(rightRect, QBrush(rightGradient));

    const qreal radius = qMin(bounds.width(), bounds.height()) * kCornerVignetteRadiusRatio;
    const QVector<QPointF> corners = {
        bounds.topLeft(),
        bounds.topRight(),
        bounds.bottomLeft(),
        bounds.bottomRight()
    };

    for (const QPointF& corner : corners) {
        QRadialGradient gradient(corner, radius);
        gradient.setColorAt(0.0, QColor(0, 0, 0, kCornerVignetteInnerAlpha));
        gradient.setColorAt(kCornerVignetteMidStop, QColor(0, 0, 0, kCornerVignetteMidAlpha));
        gradient.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter.fillRect(bounds, QBrush(gradient));
    }
}

void NetherLinkCreditsWindow::drawCenteredText(QPainter& painter,
                                               const QString& text,
                                               qreal y,
                                               int pixelSize,
                                               const QColor& color,
                                               qreal height,
                                               bool bold) const
{
    QFont textFont = font();
    textFont.setPixelSize(pixelSize);
    textFont.setBold(bold);
    painter.setFont(textFont);

    const qreal contentWidth = qMin(width() * 0.82, kMaxCreditsWidth);
    const QRectF textRect((width() - contentWidth) / 2.0, y, contentWidth, height);
    const QPointF shadowOffset(qMax<qreal>(1.0, pixelSize / 16.0), qMax<qreal>(1.0, pixelSize / 16.0));

    painter.setPen(QColor(0, 0, 0, 210));
    painter.drawText(textRect.translated(shadowOffset), Qt::AlignHCenter | Qt::AlignVCenter, text);
    painter.setPen(color);
    painter.drawText(textRect, Qt::AlignHCenter | Qt::AlignVCenter, text);
}

qreal NetherLinkCreditsWindow::entryHeight(const CreditEntry& entry) const
{
    switch (entry.type) {
    case EntryType::Logo:
        return logoHeight() + 8.0;
    case EntryType::YellowLine:
        return 31.0;
    case EntryType::Section:
        return 52.0;
    case EntryType::RoleName:
        return 62.0;
    case EntryType::Spacer:
        return entry.spacing;
    }

    return 0.0;
}

qreal NetherLinkCreditsWindow::logoHeight() const
{
    if (m_logoPixmap.isNull() || m_logoPixmap.width() <= 0) {
        return 58.0;
    }

    const qreal sourceWidth = m_logoPixmap.width() / m_logoPixmap.devicePixelRatio();
    const qreal sourceHeight = m_logoPixmap.height() / m_logoPixmap.devicePixelRatio();
    const qreal targetWidth = qMin(qMin(width() * 0.72, kMaxCreditsWidth), sourceWidth);
    return targetWidth * sourceHeight / sourceWidth;
}

bool NetherLinkCreditsWindow::isScrollFinished() const
{
    if (height() <= 0 || m_entries.isEmpty()) {
        return false;
    }

    const qreal startDelay = kRevealDurationMs + kCreditsPauseAfterRevealMs;
    if (m_clock.elapsed() < startDelay) {
        return false;
    }

    const qreal creditsHeight = visibleCreditsHeight();
    return creditsHeight > 0.0 && scrollOffset() + creditsHeight <= 0.0;
}

qreal NetherLinkCreditsWindow::scrollOffset() const
{
    const qreal startDelay = kRevealDurationMs + kCreditsPauseAfterRevealMs;
    const qreal scrollMs = qMax<qreal>(0.0, m_clock.elapsed() - startDelay);
    const qreal scrollStartY = height() + 48.0;
    const qreal progress = scrollMs / 1000.0 * kCreditsSpeed;
    return scrollStartY - progress;
}

qreal NetherLinkCreditsWindow::totalCreditsHeight() const
{
    qreal total = 0.0;
    for (const CreditEntry& entry : m_entries) {
        total += entryHeight(entry);
    }
    return total;
}

qreal NetherLinkCreditsWindow::visibleCreditsHeight() const
{
    qreal y = 0.0;
    qreal lastVisibleBottom = 0.0;
    for (const CreditEntry& entry : m_entries) {
        const qreal height = entryHeight(entry);
        if (entry.type != EntryType::Spacer) {
            lastVisibleBottom = y + height;
        }
        y += height;
    }

    return lastVisibleBottom;
}
