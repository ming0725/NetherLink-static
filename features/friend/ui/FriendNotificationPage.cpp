#include "FriendNotificationPage.h"

#include <QApplication>
#include <QPainter>
#include <QResizeEvent>

#include "features/friend/data/FriendNotificationRepository.h"
#include "features/friend/ui/FriendNotificationListWidget.h"
#include "shared/theme/ThemeManager.h"

namespace {

constexpr int kHeaderHeight = 62;
constexpr int kPageSize = 16;

const QFont& headerFont()
{
    static const QFont font = []() {
        QFont f = QApplication::font();
        f.setPixelSize(17);
        return f;
    }();
    return font;
}

} // namespace

FriendNotificationPage::FriendNotificationPage(QWidget* parent)
    : QWidget(parent)
    , m_listWidget(new FriendNotificationListWidget(this))
    , m_header(new QWidget(this))
{
    m_header->setAttribute(Qt::WA_StyledBackground, false);

    m_listWidget->setNotifications({});

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this]() { applyTheme(); });

    connect(m_listWidget, &FriendNotificationListWidget::acceptRequest,
            this, &FriendNotificationPage::acceptRequest);
    connect(m_listWidget, &FriendNotificationListWidget::rejectRequest,
            this, &FriendNotificationPage::rejectRequest);
    connect(m_listWidget, &FriendNotificationListWidget::loadMoreRequested,
            this, &FriendNotificationPage::loadMoreNotifications);

    applyTheme();
}

void FriendNotificationPage::reloadNotifications()
{
    m_loadedCount = 0;
    m_hasMore = true;
    m_listWidget->setNotifications({});
    loadMoreNotifications();
}

void FriendNotificationPage::refreshLoadedNotifications()
{
    const int count = qMax(kPageSize, m_listWidget->notificationCount());
    QVector<FriendNotification> notifications =
            FriendNotificationRepository::instance().requestNotificationList({0, count});
    m_loadedCount = notifications.size();
    m_hasMore = m_loadedCount < FriendNotificationRepository::instance().notificationCount();
    m_listWidget->setNotifications(std::move(notifications));
}

void FriendNotificationPage::loadMoreNotifications()
{
    if (m_loading || !m_hasMore) {
        return;
    }

    m_loading = true;
    QVector<FriendNotification> notifications =
            FriendNotificationRepository::instance().requestNotificationList({m_loadedCount, kPageSize});
    m_loadedCount += notifications.size();
    m_hasMore = !notifications.isEmpty() &&
            m_loadedCount < FriendNotificationRepository::instance().notificationCount();
    m_listWidget->appendNotifications(std::move(notifications));
    m_loading = false;
}

void FriendNotificationPage::applyTheme()
{
    m_header->update();
    update();
}

void FriendNotificationPage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    m_header->setGeometry(0, 0, width(), kHeaderHeight);
    m_listWidget->setGeometry(0, kHeaderHeight, width(), qMax(0, height() - kHeaderHeight));
}

void FriendNotificationPage::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Full background
    painter.fillRect(rect(), ThemeManager::instance().color(ThemeColor::PageBackground));

    // Header background
    painter.fillRect(QRect(0, 0, width(), kHeaderHeight),
                     ThemeManager::instance().color(ThemeColor::PageBackground));

    // Header text
    painter.setFont(headerFont());
    painter.setPen(ThemeManager::instance().color(ThemeColor::PrimaryText));
    painter.drawText(QRect(20, 0, width() - 40, kHeaderHeight),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("好友通知"));
}
