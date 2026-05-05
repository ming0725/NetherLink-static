#include "GroupNotificationPage.h"

#include <QApplication>
#include <QPainter>
#include <QResizeEvent>

#include "features/friend/data/GroupNotificationRepository.h"
#include "features/friend/ui/GroupNotificationListWidget.h"
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

GroupNotificationPage::GroupNotificationPage(QWidget* parent)
    : QWidget(parent)
    , m_listWidget(new GroupNotificationListWidget(this))
    , m_header(new QWidget(this))
{
    m_header->setAttribute(Qt::WA_StyledBackground, false);
    m_listWidget->setNotifications({});

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this]() { applyTheme(); });
    connect(m_listWidget, &GroupNotificationListWidget::acceptRequest,
            this, &GroupNotificationPage::acceptRequest);
    connect(m_listWidget, &GroupNotificationListWidget::rejectRequest,
            this, &GroupNotificationPage::rejectRequest);
    connect(m_listWidget, &GroupNotificationListWidget::loadMoreRequested,
            this, &GroupNotificationPage::loadMoreNotifications);

    applyTheme();
}

void GroupNotificationPage::reloadNotifications()
{
    m_loadedCount = 0;
    m_hasMore = true;
    m_listWidget->setNotifications({});
    loadMoreNotifications();
}

void GroupNotificationPage::refreshLoadedNotifications()
{
    const int count = qMax(kPageSize, m_listWidget->notificationCount());
    auto notifications = GroupNotificationRepository::instance().requestNotificationList({0, count});
    m_loadedCount = notifications.size();
    m_hasMore = m_loadedCount < GroupNotificationRepository::instance().notificationCount();
    m_listWidget->setNotifications(std::move(notifications));
}

void GroupNotificationPage::loadMoreNotifications()
{
    if (m_loading || !m_hasMore) {
        return;
    }
    m_loading = true;
    auto notifications = GroupNotificationRepository::instance().requestNotificationList({m_loadedCount, kPageSize});
    m_loadedCount += notifications.size();
    m_hasMore = !notifications.isEmpty() &&
            m_loadedCount < GroupNotificationRepository::instance().notificationCount();
    m_listWidget->appendNotifications(std::move(notifications));
    m_loading = false;
}

void GroupNotificationPage::applyTheme()
{
    m_header->update();
    update();
}

void GroupNotificationPage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m_header->setGeometry(0, 0, width(), kHeaderHeight);
    m_listWidget->setGeometry(0, kHeaderHeight, width(), qMax(0, height() - kHeaderHeight));
}

void GroupNotificationPage::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), ThemeManager::instance().color(ThemeColor::PageBackground));
    painter.fillRect(QRect(0, 0, width(), kHeaderHeight),
                     ThemeManager::instance().color(ThemeColor::PageBackground));
    painter.setFont(headerFont());
    painter.setPen(ThemeManager::instance().color(ThemeColor::PrimaryText));
    painter.drawText(QRect(20, 0, width() - 40, kHeaderHeight),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("群通知"));
}
