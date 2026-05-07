#ifndef CHATAREA_H
#define CHATAREA_H

#include <QWidget>
#include <QSharedPointer>
#include <QDateTime>
#include "ChatListView.h"
#include "features/chat/model/ChatListModel.h"
#include "ChatItemDelegate.h"
#include "shared/types/ChatMessage.h"
#include "NewMessageNotifier.h"
#include "shared/ui/FloatingInputBar.h"
#include "shared/types/Group.h"
#include "shared/types/RepositoryTypes.h"

class QPropertyAnimation;
class QPushButton;
class QLabel;
class ChatSessionController;
class DirectConversationInfoPanel;
class GroupConversationInfoPanel;

class ChatArea : public QWidget
{
    Q_OBJECT
    using ChatMessagePtr = QSharedPointer<ChatMessage>;
public:
    explicit ChatArea(QWidget *parent = nullptr);
    void openConversation(const ConversationThreadData& conversation);
    void addMessage(ChatMessagePtr message);
    void addImageMessage(QSharedPointer<ImageMessage> message,
                         const QDateTime& timestamp = QDateTime::currentDateTime());
    void addTextMessage(QSharedPointer<TextMessage> message,
                        const QDateTime& timestamp = QDateTime::currentDateTime());
    void closeConversation();
    void clearMessageSelection();
    void handleGlobalMousePress(const QPoint& globalPos);
    void setSystemFloatingBarsSuppressed(bool suppressed);

signals:
    void currentConversationRemoved();

protected:
    void resizeEvent(QResizeEvent *event) override;
private slots:
    void onScrollValueChanged(int value);
    void onNewMessageNotifierClicked();
    void onSendImage(const QString &path);
    void onSendText(const QString &text);
    void onSendTextAsPeer(const QString &text);
    void onDeleteMessageRequested(int row);
    void onInfoButtonClicked();
    void confirmClearChatHistory();
    void confirmDeleteFriend();
    void confirmExitGroup();

private:
    struct ConversationState {
        ConversationMeta meta;
        int unreadMessageCount = 0;
        int loadedMessageCount = 0;
        bool isAtBottom = true;
        bool hasMoreBefore = false;
        bool loadingOlderMessages = false;
        bool allowOlderMessageFetch = false;
    };

    ChatListView* chatView;
    ChatListModel* chatModel;
    ChatItemDelegate* chatDelegate;
    NewMessageNotifier* newMessageNotifier;
    QWidget* bottomGapGradientOverlay;
    FloatingInputBar* inputBar;
    QLabel* statusIcon;
    QLabel* nameLabel;
    QPushButton* infoButton;
    GroupConversationInfoPanel* groupInfoPanel = nullptr;
    DirectConversationInfoPanel* directInfoPanel = nullptr;
    ChatSessionController* sessionController;
    QPropertyAnimation* infoPanelAnimation;
    bool infoPanelOpen = false;
    bool m_systemFloatingBarsSuppressed = false;
    bool m_inputBarVisibleBeforeSystemSuppression = false;
    ConversationState m_state;
    
    void updateNewMessageNotifier();
    void updateNewMessageNotifierPosition();
    void scrollToBottom(bool accelerateFarDistance = false);
    bool isScrollAtBottom() const;
    bool isNearBottom() const;
    void adjustBottomSpace();
    void updateInputBarPosition();
    void applyConversationMeta();
    void clearConversation(bool closeInfoPanel = true);
    void loadOlderMessages();
    QString conversationId() const;
    bool isGroupMode() const;
    QWidget* activeInfoPanel() const;
    QWidget* inactiveInfoPanel() const;
    QWidget* ensureActiveInfoPanel();
    void connectGroupInfoPanel(GroupConversationInfoPanel* panel);
    void connectDirectInfoPanel(DirectConversationInfoPanel* panel);
    void releaseInfoPanels();
    void requestInfoPanelData(bool resetTransientState);
    int visibleInfoPanelWidth() const;
    void showInfoPanel(bool animated);
    void hideInfoPanel(bool animated);
    void updateInfoPanelGeometry();
    QRect infoPanelOpenGeometry() const;
    QRect infoPanelClosedGeometry() const;
    bool containsGlobalPoint(QWidget* widget, const QPoint& globalPos) const;
    void updateGroupInfoPanelState();
    void updateDirectInfoPanelState(bool animated);
    void onSessionChanged(const ConversationMeta& meta);
    void onDirectPanelDataLoaded(const ConversationMeta& meta, const User& directUser);
    void onGroupPanelDataLoaded(const ConversationMeta& meta,
                                const Group& group,
                                const QVector<User>& previewMembers,
                                int totalMembers,
                                bool canEditGroupInfo,
                                bool canExitGroup);
    void onGroupMembersPageLoaded(const ConversationMeta& meta, const GroupMembersPage& page);
    void onSessionMessagesCleared();
    void onSessionConversationRemoved();
};

#endif // CHATAREA_H 
