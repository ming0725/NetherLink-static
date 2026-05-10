#pragma once

#include <QWidget>

#include "shared/types/RepositoryTypes.h"

class AiChatMessageListModel;
class AiChatMessageListView;
class AiChatFloatingInputBar;
class QLabel;
class QTimer;

class AiChatConversationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AiChatConversationWidget(QWidget* parent = nullptr);

public slots:
    void openConversation(const AiChatListEntry& entry);
    void closeConversation();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onSendText(const QString& text);
    void advanceAiReplyStream();

private:
    void updateLayout();
    void updateBottomSpace();
    void updateHeader();
    void appendAiReply(const QString& prompt);
    QString randomReplyText(const QString& prompt) const;
    void finishActiveAiReplyStream();

    AiChatMessageListView* m_messageView = nullptr;
    AiChatMessageListModel* m_messageModel = nullptr;
    AiChatFloatingInputBar* m_inputBar = nullptr;
    QLabel* m_titleLabel = nullptr;
    QWidget* m_headerDivider = nullptr;
    QWidget* m_bottomGapGradientOverlay = nullptr;
    QLabel* m_emptyLabel = nullptr;
    QTimer* m_streamTimer = nullptr;
    AiChatListEntry m_currentConversation;
    QString m_streamConversationId;
    QString m_streamMessageId;
    QString m_streamReplyText;
    int m_streamOffset = 0;

    static constexpr int kHeaderHeight = 62;
    static constexpr int kInputBarHeight = 195;
    static constexpr int kInputBarSideMargin = 20;
    static constexpr int kInputBarBottomMargin = 18;
    static constexpr int kListBottomPadding = 10;
};
