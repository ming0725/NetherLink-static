/*****************************************************************************
*  imagemanager.h
*  Copyright (C) 2025 hesphoros <hesphoros@gmail.com>
*
*  This file is part of imagemanager.h.
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License version 3 as
*  published by the Free Software Foundation.
*
*  You should have received a copy of the GNU General Public License
*  along with this program. If not, see <http://www.gnu.org/licenses/>.
*
*  @file     ImageManager.h
*  @brief    高性能图片图片管理器
*  @note      本文件定义了ImageManager类和相关的异步加载任务类，
*             提供高效的图片加载、缓存和内存管理功能。
*  @details  None
*
*  @author   hesphoros
*  @email    hesphoros@gmail.com
*  @version  1.0.0.1
*  @date     2025/06/26
*  @license  GNU General Public License (GPL)
*---------------------------------------------------------------------------*
*  Remark         : 图片管理器用于加载、显示和缓存等功能。
*---------------------------------------------------------------------------*
*  Change History :
*  <Date>     | <Version> | <Author>       | <Description>
*  2025/06/26 | 1.0.0.1   | hesphoros      | Create file
*****************************************************************************/

#include "imagemanager.h"


#include <QDir>
#include <QRandomGenerator>
#include <QDirIterator>
#include <QDebug>


/**
 * @brief AvatarLoadTask构造函数
 * @param avatarName 要加载的图片文件名
 * @param resourcePrefix 资源路径前缀
 * @param manager AvatarManager实例指针，用于回调
 *
 * 创建一个异步图片加载任务。任务设置为自动删除，
 * 完成后会自动释放内存。
 */
ImageLoadTask::ImageLoadTask(const QString& avatarName, const QString& resourcePrefix, ImageManager* manager)
    : m_avatarName(avatarName)
    , m_resourcePrefix(resourcePrefix)
    , m_manager(manager)
{
    // 任务完成后自动删除
    setAutoDelete(true);
}

/**
 * @brief 异步加载任务的主执行函数
 *
 * 此方法在后台线程中执行，负责：
 * 1. 验证管理器指针的有效性
 * 2. 检查图片是否已在缓存中（避免重复加载）
 * 3. 从文件系统加载图片图片
 * 4. 通过Qt::QueuedConnection异步通知主线程加载结果
 *
 * @note 此方法在只工作线程中执行
 */
void ImageLoadTask::run() {
    if (!m_manager) {
#ifdef AVATAR_DEBUG
        qWarning() << "AvatarLoadTask: Invalid manager pointer";
        g_AvatarTaskLogger.WriteLogContent(LOG_ERROR, "AvatarLoadTask: Invalid manager pointer");
#endif
        return;
    }

    // 检查是否已经在缓存中
    if (m_manager->isAvatarLoaded(m_avatarName)) {
        return;
    }

    // 加载图片
    QString fullPath = m_resourcePrefix + m_avatarName;
    QPixmap pixmap(fullPath);

    if (!pixmap.isNull()) {
        // 加载成功，通知管理器
        QMetaObject::invokeMethod(m_manager, "onAvatarLoadFinished",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, m_avatarName),
                                  Q_ARG(QPixmap, pixmap));
    }
    else {
#ifdef AVATAR_DEBUG
        qWarning() << "AvatarLoadTask: Failed to load avatar:" << fullPath;
        g_AvatarTaskLogger.WriteLogContent(LOG_ERROR, "AvatarLoadTask: Failed to load avatar: " + fullPath.toStdString());
#endif
    }
}


/// <summary>
/// 获取AvatarManager单例实例
/// </summary>
ImageManager& ImageManager::instance() {
    static ImageManager inst;
    return inst;
}

/// <summary>
/// Constructor for ImageManager
/// 初始化所有成员变量
///  - 设置默认资源路径前缀
///  - 创建LRU缓存 默认大小1000
///  - 配置预加载策略
///  - 启动定时清理器
///  - 设置缓存大小限制
/// </summary>
ImageManager::ImageManager()
    : QObject(nullptr)
    , m_resourcePrefix(IMAGE_DEFAULT_CIRCLE_PREFIX)
    , m_avatarCache(IMAGE_CACHE_SIZE)
    , m_preloadEnabled(true)
    , m_maxPreloadCount(IMAGE_MAX_PRELOAD_COUNT)
    , m_cleanupTimer(new QTimer(this))
    , m_threadPool(QThreadPool::globalInstance())
    , m_maxCacheSizeMB(IMAGE_MAX_CACHE_SIZE_MB)
{
    // 设置缓存的最大大小
    this->m_avatarCache.setMaxCost(m_maxCacheSizeMB * 1024);

    // 设置清理定时器
    this->m_cleanupTimer->setInterval(IMAGE_CLEANUP_INTERVAL_MS);
    this->connect(m_cleanupTimer, &QTimer::timeout, this, &ImageManager::onCleanupTimer);
    this->m_cleanupTimer->start();
}

ImageManager::~ImageManager()
{
}

/// <summary>
/// 初始化图片列表
/// param resourcePrefix 图片资源路径前缀，默认为 AVATAR_DEFAULT_CIRCLE_PREFIX
/// <brief>
///    自动扫描资源文件中的所有图片文件，支持多种图片格式。
///    此方法会递归扫描指定资源路径，识别所有有效的图片文件。
///    完成后会发射initializeCompleted信号。
///    支持的图片格式：png, jpg, jpeg, bmp, gif, svg
/// </brief>
/// </summary>
void ImageManager::initialize(const QString& resourcePrefix/* = AVATAR_DEFAULT_CIRCLE_PREFIX*/) {
    this->m_resourcePrefix = resourcePrefix;

    QMutexLocker locker(&m_avatarListLock);
    m_availableAvatars.clear();

    // 支持的图片格式
    QStringList imageExtensions = { "*.png", "*.jpg", "*.jpeg", "*.bmp", "*.gif", "*.svg" };

    // 如果是资源路径，使用QDirIterator扫描
    if (resourcePrefix.startsWith(":/")) {
        QDirIterator it(resourcePrefix, imageExtensions, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);

        while (it.hasNext()) {
            QString fullPath = it.next();
            QFileInfo fileInfo(fullPath);
            QString fileName = fileInfo.fileName();

            // 验证文件是否真的存在且可读
            if (QFile::exists(fullPath)) {
                m_availableAvatars << fileName;
#ifdef AVATAR_DEBUG
                //qDebug() << "Found avatar resource:" << fileName;
                g_AvatarMgrLogger.WriteLogContent(LOG_INFO, "Found avatar resource: " + fileName.toStdString());
#endif // AVATAR_DEBUG
            }
        }
    }

    else {
        // 文件系统路径，使用QDir扫描
        QDir dir(resourcePrefix);
        if (dir.exists()) {
            QStringList filters;
            for (const QString& ext : imageExtensions) {
                filters << ext;
            }

            QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files | QDir::Readable);
            for (const QFileInfo& fileInfo : fileList) {
                QString fileName = fileInfo.fileName();

                // 验证文件确实是有效的图片文件
                QPixmap testPixmap(fileInfo.absoluteFilePath());
                if (!testPixmap.isNull()) {
                    m_availableAvatars << fileName;
#ifdef AVATAR_DEBUG
                    qDebug() << "Found avatar file:" << fileName;
                    g_AvatarMgrLogger.WriteLogContent(LOG_INFO, "Found avatar file: " + fileName.toStdString());
#endif // AVATAR_DEBUG
                }
            }
        }
    }

    // 如果没有找到任何图片文件，添加一些备用的默认图片
    if (m_availableAvatars.isEmpty()) {
#ifdef AVATAR_DEBUG
        qWarning() << "No avatar files found in" << resourcePrefix << ", using fallback avatars";
        g_AvatarMgrLogger.WriteLogContent(LOG_WARN, "No avatar files found in " + resourcePrefix.toStdString() + ", using fallback avatars");
#endif \
    // 尝试一些常见的图片文件名
        QStringList fallbackAvatars = {
            "01.png", "02.png", "03.png", "04.png", "05.png",
            "avatar1.png", "avatar2.png", "avatar3.png",
            "user.png", "default.png", "profile.png"
        };

        for (const QString& avatar : fallbackAvatars) {
            QString fullPath = resourcePrefix + avatar;
            if (QFile::exists(fullPath)) {
                m_availableAvatars << avatar;
#ifdef AVATAR_DEBUG
                qDebug() << "Found fallback avatar:" << avatar;
                g_AvatarMgrLogger.WriteLogContent(LOG_INFO, "Found fallback avatar: " + avatar.toStdString());
#endif // AVATAR_DEBUG
            }
        }
    }

    // 对图片列表进行排序，使结果可预测
    m_availableAvatars.sort();
#ifdef AVATAR_DEBUG
    qDebug() << "AvatarManager initialized with" << m_availableAvatars.size() << "avatars from" << resourcePrefix;
    qDebug() << "Available avatars:" << m_availableAvatars;
    g_AvatarMgrLogger.WriteLogContent(LOG_INFO,
                                      QString("AvatarManager initialized with %1 avatars from %2").arg(m_availableAvatars.size()).arg(resourcePrefix).toStdString());
#endif // AVATAR_DEBUG

    emit initializeCompleted(m_availableAvatars.size());
}

/**
 * @brief 获取指定图片
 * @param avatarName 图片文件名
 * @return 图片QPixmap，如果未加载则返回默认图片
 *
 * 此方法实现了多层加载策略：
 * 1. 首先检查缓存（快速路径）
 * 2. 如果缓存中没有，尝试同步加载（为了立即返回结果）
 * 3. 如果同步加载失败，触发异步加载并返回默认图片
 *
 * 缓存访问使用读写锁保护，支持多线程并发读取。
 * 访问历史会被记录用于智能预加载。
 */
QPixmap ImageManager::getAvatar(const QString& avatarName) const {
    // 首先检查缓存（快速路径）
    {
        QReadLocker cacheLocker(&m_cacheLock);
        QPixmap* cached = m_avatarCache.object(avatarName);
        if (cached) {
            // 更新访问历史（用于智能预加载）
            updateAccessHistory(avatarName);
            return *cached;
        }
    }

    // 如果缓存中没有，尝试同步加载（为了立即返回结果）
    QString fullPath = m_resourcePrefix + avatarName;
    QPixmap pixmap(fullPath);

    if (!pixmap.isNull()) {
        // 加载成功，添加到缓存
        QWriteLocker cacheLocker(&m_cacheLock);
        int cost = pixmap.width() * pixmap.height() * pixmap.depth() / 8 / 1024; // KB
        m_avatarCache.insert(avatarName, new QPixmap(pixmap), cost);
        updateAccessHistory(avatarName);
        return pixmap;
    }
    else {
        // 如果同步加载失败，触发异步加载（为下次准备）
        loadImageAsync(avatarName);

// 返回默认图片
#ifdef AVATAR_DEBUG
        qDebug() << "Avatar not found, loading asynchronously:" << fullPath;
        g_AvatarMgrLogger.WriteLogContent(LOG_WARN, "Avatar not found, loading asynchronously: " + fullPath.toStdString());
#endif // AVATAR_DEBUG
        return getDefaultAvatar();
    }
}

/**
 * @brief 获取随机图片名称
 * @return 随机选择的图片文件名
 * @retval 如果没有可用图片，则返回空字符串
 *
 * @detials
 * 使用QRandomGenerator生成安全的随机数，
 * 从可用图片列表中随机选择一个。
 */
QString ImageManager::getRandomAvatar() const {
    QMutexLocker locker(&m_avatarListLock);
    if (m_availableAvatars.isEmpty()) {
        return QString();
    }
    int index = QRandomGenerator::global()->bounded(m_availableAvatars.size());
    return m_availableAvatars.at(index);
}

/**
 * @brief 预加载指定图片
 * @param avatarName 要预加载的图片文件名
 *
 * @detials 异步预加载图片到缓存中，为后续快速访问做准备。如果图片已在缓存中，则不执行任何操作。
 */
void ImageManager::preloadAvatar(const QString& avatarName) {
    // 检查是否已经在缓存中
    {
        QReadLocker locker(&m_cacheLock);
        if (m_avatarCache.contains(avatarName)) {
            return; // 已经在缓存中，无需预加载
        }
    }
    // 异步预加载
    this->loadImageAsync(avatarName);
}

/**
 * @brief   批量预加载图片
 * @param   avatarNames 要预加载的图片文件名列表
 * @detials 异步批量预加载图片，会根据maxPreloadCount限制同时加载的数量。
 */
void ImageManager::preloadAvatars(const QStringList& avatarNames) {
    int preloadCount = 0;

    for (const QString& name : avatarNames) {
        // 检查是否已经在缓存中
        {
            QReadLocker locker(&m_cacheLock);
            if (m_avatarCache.contains(name)) {
                continue; // 已经在缓存中，跳过
            }
        }

        // 异步预加载
        loadImageAsync(name);
        preloadCount++;

        // 限制同时预加载的数量，避免过载
        if (m_preloadEnabled && preloadCount >= m_maxPreloadCount) {
            break;
        }
    }
#ifdef AVATAR_DEBUG
    qDebug() << "Started preloading" << preloadCount << "avatars";
    g_AvatarMgrLogger.WriteLogContent(LOG_INFO, QString("Started preloading %1 avatars").arg(preloadCount).toStdString());
#endif // AVATAR_DEBUG
}

/// <summary>
/// 缓存清理定时器槽函数
/// 释放不常用的图片资源
/// </summary>
void ImageManager::onCleanupTimer()
{
    // 定期清理最少使用的缓存项
    QWriteLocker locker(&m_cacheLock);
    // QCache会自动管理LRU，这里只需要触发一次清理
    if (m_avatarCache.totalCost() > m_maxCacheSizeMB * 1024 * 0.9) {
        // 如果缓存使用超过90%，触发更积极的清理
        m_avatarCache.setMaxCost(m_maxCacheSizeMB * 1024 * 0.8);
        m_avatarCache.setMaxCost(m_maxCacheSizeMB * 1024);
    }
}

bool ImageManager::isAvatarLoaded(const QString& avatarName) const {
    // 检查图片是否已在缓存中
    QReadLocker locker(&m_cacheLock);
    return m_avatarCache.contains(avatarName);
}

QStringList ImageManager::getAvailableAvatars() const {
    QMutexLocker locker(&m_avatarListLock);
    return m_availableAvatars;
}

int ImageManager::getAvatarCount() const {
    QMutexLocker locker(&m_avatarListLock);
    return m_availableAvatars.size();
}

bool ImageManager::hasAvatar(const QString& avatarName) const {
    QMutexLocker locker(&m_avatarListLock);
    return m_availableAvatars.contains(avatarName);
}

void ImageManager::setCacheSize(int maxSizeInMB) {
    m_maxCacheSizeMB = maxSizeInMB;
    QWriteLocker locker(&m_cacheLock);
    m_avatarCache.setMaxCost(maxSizeInMB * 1024);
}

void ImageManager::clearCache() {
    QWriteLocker locker(&m_cacheLock);
    m_avatarCache.clear();
}

int ImageManager::getCacheUsage() const {
    QReadLocker locker(&m_cacheLock);
    return m_avatarCache.totalCost();
}

void ImageManager::setPreloadStrategy(bool enabled, int maxPreloadCount) {
    m_preloadEnabled = enabled;
    m_maxPreloadCount = maxPreloadCount;
}

// private slots
void ImageManager::onAvatarLoadFinished(const QString& avatarName, const QPixmap& pixmap) {
    if (pixmap.isNull()) {
#ifdef AVATAR_DEBUG
        qWarning() << "AvatarManager: Received null pixmap for" << avatarName;
        g_AvatarMgrLogger.WriteLogContent(LOG_ERROR, "Received null pixmap for " + avatarName.toStdString());
#endif // AVATAR_DEBUG
        return;
    }

    // 添加到缓存
    {
        QWriteLocker locker(&m_cacheLock);
        int cost = pixmap.width() * pixmap.height() * pixmap.depth() / 8 / 1024; // KB
        m_avatarCache.insert(avatarName, new QPixmap(pixmap), cost);
    }

    // 更新访问历史
    updateAccessHistory(avatarName);

    // 发射图片加载完成信号
    emit avatarLoaded(avatarName, pixmap);
}

/**
 * @brief 异步加载图片（内部方法）
 * @param avatarName 要加载的图片文件名
 *
 * 创建AvatarLoadTask并提交到线程池执行异步加载。
 * 在提交任务前会检查缓存，避免重复加载。
 *
 * @note 此方法是const的，因为它不改变对象的逻辑状态，
 *       只是触发异步操作来填充缓存。
 */
void ImageManager::loadImageAsync(const QString& avatarName) const {
    // 检查是否已经在缓存中
    {
        QReadLocker locker(&m_cacheLock);
        if (m_avatarCache.contains(avatarName)) {
            return; // 已经在缓存中，无需重复加载
        }
    }

    // 创建异步加载任务
    ImageLoadTask* task = new ImageLoadTask(avatarName, m_resourcePrefix,
                                              const_cast<ImageManager*>(this));

    // 提交到线程池
    m_threadPool->start(task);
}

/**
 * @brief 更新访问历史（内部方法）
 * @param avatarName 被访问的图片文件名
 *
 * 记录图片访问历史，用于LRU缓存和智能预加载。
 * 实现策略：
 * 1. 移除重复项，将当前访问的图片移到列表最前面
 * 2. 限制历史记录长度为100，保持内存使用合理
 * 3. 如果启用智能预加载，触发相关图片的预加载
 *
 * @note 此方法是const的，因为访问历史是mutable的辅助数据，
 *       不影响对象的逻辑状态。
 */
void ImageManager::updateAccessHistory(const QString& avatarName) const {
    QMutexLocker locker(&m_historyLock);

    // 移除重复项并添加到最前面
    m_accessHistory.removeAll(avatarName);
    m_accessHistory.prepend(avatarName);

    // 限制历史记录长度
    if (m_accessHistory.size() > 100) {
        m_accessHistory = m_accessHistory.mid(0, 100);
    }

    // 智能预加载：如果启用了预加载，预加载最近访问的图片附近的图片
    if (m_preloadEnabled && m_accessHistory.size() > 1) {
        // 使用QTimer::singleShot延迟执行，避免在当前调用栈中执行耗时操作
        QTimer::singleShot(100, const_cast<ImageManager*>(this), [this, avatarName]() {
            smartPreload(avatarName);
        });
    }
}

/**
 * @brief 获取默认图片（内部方法）
 * @return 默认图片QPixmap
 *
 * 返回一个简单的灰色默认图片，当请求的图片不存在或正在加载时使用。
 * 使用静态变量缓存默认图片，避免重复创建。
 */
QPixmap ImageManager::getDefaultAvatar() const {
    // 返回一个简单的默认图片
    static QPixmap defaultPixmap;
    if (defaultPixmap.isNull()) {
        defaultPixmap = QPixmap(48, 48);
        defaultPixmap.fill(QColor(128, 128, 128));
    }
    return defaultPixmap;
}

/**
 * @brief 智能预加载相关图片（内部方法）
 * @param currentAvatar 当前访问的图片文件名
 *
 * 基于当前访问的图片，使用启发式算法预测并预加载可能需要的相关图片。
 *
 * 当前实现的预加载策略：
 * 1. 找到当前图片在可用图片列表中的位置
 * 2. 预加载前后3个图片（模拟用户可能浏览相邻图片的场景）
 * 3. 跳过已在缓存中的图片，避免浪费资源
 * 4. 异步执行，不阻塞当前操作
 *
 * @note 此策略可以根据实际使用模式进行优化，
 *       例如基于用户行为、时间模式等进行更智能的预测。
 */
void ImageManager::smartPreload(const QString& currentAvatar) const {
    // 基于当前图片名称，预测可能需要的图片
    // 这里使用简单的策略：预加载同一系列的图片

    QMutexLocker avatarLocker(&m_avatarListLock);
    QStringList candidates;

    // 找到当前图片在列表中的位置
    int currentIndex = m_availableAvatars.indexOf(currentAvatar);
    if (currentIndex >= 0) {
        // 预加载前后几个图片
        int preloadRange = 3;
        for (int i = qMax(0, currentIndex - preloadRange);
             i <= qMin(m_availableAvatars.size() - 1, currentIndex + preloadRange);
             ++i) {
            if (i != currentIndex) {
                candidates << m_availableAvatars[i];
            }
        }
    }

    // 异步预加载候选图片
    for (const QString& candidate : candidates) {
        // 检查是否已经在缓存中
        {
            QReadLocker cacheLocker(&m_cacheLock);
            if (m_avatarCache.contains(candidate)) {
                continue; // 已经在缓存中，跳过
            }
        }

        // 异步加载
        loadImageAsync(candidate);
    }
}


