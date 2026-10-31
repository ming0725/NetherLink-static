#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QPointF>
#include <QRectF>
#include <QTimer>
#include <QVector>
#include <QWidget>

#include "shared/types/RepositoryTypes.h"

class QAbstractButton;
class QMouseEvent;
class QPainter;
class TransparentTextEdit;

class AiChatFloatingInputBar : public QWidget
{
    Q_OBJECT

public:
    explicit AiChatFloatingInputBar(QWidget* parent = nullptr);

    void focusInput();
    void refocusInputAfterPositionChange();
    void setStreaming(bool streaming, bool animateTransition = true);
    QString text() const;
    void setText(const QString& text);
    void clearText();
    void setContextUsage(const AiChatContextUsage& usage);
    void setContextUsageVisible(bool visible);
    int preferredHeightForWidth(int width) const;
    QSize sizeHint() const override;

signals:
    void sendText(const QString& text);
    void stopStreamingRequested();
    void inputFocused();
    void preferredHeightChanged(int height);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    enum class BorderGlowState {
        Idle,
        Streaming,
        PendingFinish,
        Finishing,
    };

    struct BorderGlowSample {
        QPointF point;
        qreal length = 0.0;
    };

    void applyTheme();
    void onActionButtonClicked();
    void showPermissionMenu();
    void showModelMenu();
    void sendCurrentText();
    void updateActionButtonIcon();
    void updateModelButtonText();
    void updateModelButtonState();
    void updateInputGeometry();
    void updatePreferredHeight();
    void startBorderGlow();
    void finishBorderGlow();
    void stopBorderGlow();
    void updateBorderGlowAnimation();
    void paintBorderGlow(QPainter& painter, const QRectF& panelRect);
    bool ensureBorderGlowCache(const QRectF& rect);
    void invalidateBorderGlowCache();
    void updateBorderGlowRegion();
    QColor borderGlowColor(qreal position, qreal phase, qreal opacity, qreal alphaScale) const;
    void paintBorderGlowRange(QPainter& painter,
                              qreal totalLength,
                              qreal startLength,
                              qreal endLength,
                              qreal phase,
                              qreal opacity);

    TransparentTextEdit* m_inputEdit = nullptr;
    QAbstractButton* m_addButton = nullptr;
    QAbstractButton* m_permissionButton = nullptr;
    QAbstractButton* m_modelButton = nullptr;
    QAbstractButton* m_actionButton = nullptr;
    QTimer* m_borderGlowTimer = nullptr;
    QElapsedTimer m_borderGlowClock;
    QString m_selectedThinkingLevel = QStringLiteral("High");
    QString m_selectedModelName = QStringLiteral("DeepSeek V4 Pro");
    BorderGlowState m_borderGlowState = BorderGlowState::Idle;
    QVector<BorderGlowSample> m_borderGlowSamples;
    QRectF m_borderGlowCachedRect;
    qreal m_borderGlowTotalLength = 0.0;
    qint64 m_borderGlowStartMs = 0;
    qint64 m_borderGlowFinishStartMs = 0;
    bool m_streaming = false;
    int m_preferredHeight = 104;

    static constexpr int kCornerRadius = 22;
    static constexpr int kInputRefocusAfterMoveDelayMs = 80;
    static constexpr int kBorderGlowRevealDurationMs = 2100;
    static constexpr int kBorderGlowFinishDurationMs = 1350;
    static constexpr int kBorderGlowFrameMs = 16;
    static constexpr qreal kBorderGlowInset = 1.4;
    static constexpr qreal kBorderGlowSampleStep = 2.0;
    static constexpr qreal kBorderGlowMaxPenWidth = 5.2;
    static constexpr int kInputLeftMargin = 18;
    static constexpr int kInputRightPadding = 18;
    static constexpr int kInputTopMargin = 16;
    static constexpr int kToolbarHeight = 34;
    static constexpr int kToolbarBottomMargin = 10;
    static constexpr int kInputToolbarGap = 6;
    static constexpr int kMinInputHeight = 38;
    static constexpr int kMaxInputHeight = 130;
    static constexpr int kMinHeight = kInputTopMargin + kMinInputHeight + kInputToolbarGap +
            kToolbarHeight + kToolbarBottomMargin;
    static constexpr int kMaxHeight = kInputTopMargin + kMaxInputHeight + kInputToolbarGap +
            kToolbarHeight + kToolbarBottomMargin;
    static constexpr int kInputTextTopPadding = 1;
    static constexpr int kInputTextHorizontalPadding = 0;
    static constexpr int kInputTextBottomPadding = 5;
    static constexpr int kActionButtonSize = 34;
    static constexpr int kActionButtonRightMargin = 13;
    static constexpr int kToolbarSideMargin = 14;
    static constexpr int kToolbarGap = 10;
};
