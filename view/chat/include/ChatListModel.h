#ifndef CHATLISTMODEL_H
#define CHATLISTMODEL_H

#include <QAbstractListModel>
#include <QVector>
#include <QSharedPointer>
#include <QDateTime>
#include "ChatMessage.h"

// 时间标识类型
enum class TimeHeaderType {
    Time,           // 同一天内的时间（HH:mm）
    Yesterday,      // 昨天（昨天 HH:mm）
    DayOfWeek,     // 星期几（周X HH:mm）
    ThisYear,       // 同一年内的日期（MM-dd HH:mm）
    FullDate       // 完整日期（yyyy-MM-dd HH:mm）
};

// 时间标识项
struct TimeHeader {
    QDateTime timestamp;
    TimeHeaderType type;
    QString text;
};

// 底部空白项
struct BottomSpace {
    static constexpr int DEFAULT_HEIGHT = 200;  // 默认底部空白高度（像素）
};

// 时间间隔设置（秒）
namespace TimeSettings {
    // 调试模式下使用1分钟
    #ifdef QT_DEBUG
    static constexpr int MESSAGE_TIME_INTERVAL = 60;  // 1分钟
    #else
    static constexpr int MESSAGE_TIME_INTERVAL = 300; // 5分钟
    #endif
}

class ChatListModel : public QAbstractListModel {
    Q_OBJECT
public:
    explicit ChatListModel(QObject* parent = nullptr);
    
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    
    void addMessage(QSharedPointer<ChatMessage> message);
    const ChatMessage* messageAt(int index) const;
    void clearSelection();
    bool removeMessage(int index);

    bool isTimeHeader(int index) const;
    const TimeHeader* getTimeHeader(int index) const;
    bool isBottomSpace(int index) const;

    void ensureBottomSpace();
    void setBottomSpaceHeight(int height);

    void clear();

private:
    struct ListItem {
        QSharedPointer<ChatMessage> message;
        QSharedPointer<TimeHeader> timeHeader;
        bool isHeader = false;
        bool isBottomSpace = false;
        int bottomSpaceHeight = BottomSpace::DEFAULT_HEIGHT;  // 使用默认高度
    };
    QVector<ListItem> items;
    int selectedMessageIndex = -1;

    QString formatTimeHeader(const QDateTime& timestamp) const;
    TimeHeaderType getTimeHeaderType(const QDateTime& timestamp) const;
    bool shouldAddTimeHeader(const QDateTime& prevTime, const QDateTime& currTime) const;
};

#endif // CHATLISTMODEL_H 
