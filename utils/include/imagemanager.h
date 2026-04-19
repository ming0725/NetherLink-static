#ifndef IMAGEMANAGER_H
#define IMAGEMANAGER_H

#include <QObject>
#include <QPixmap>
#include <QStringList>
#include <QString>
#include <QMap>
#include <QMutex>
#include <QThread>
#include <QThreadPool>
#include <QRunnable>
#include <QTimer>
#include <QCache>
#include <QReadWriteLock>
#include <QAtomicInt>
#include <QSet>

//---------------------------------------------------------------------------
//  {@ Define area
//---------------------------------------------------------------------------
#define IMAGE_DEFAULT_CIRCLE_PREFIX  ""
#define IMAGE_DEFAULT_SQUARE_PREFIX  ""
#define IMAGE_CACHE_SIZE             1000   // 默认缓存1000个
#define IMAGE_MAX_PRELOAD_COUNT      20     // 默认最大同时预加载图片数
#define IMAGE_MAX_CACHE_SIZE_MB      50     // 默认最大缓存大小50MB
#define IMAGE_CLEANUP_INTERVAL_MS    300000 // 清理定时器间隔 毫秒
#define IMAGE_DEBUG
//---------------------------------------------------------------------------
//  } @Define area
//---------------------------------------------------------------------------


// Forward declarations
class ImageLoadTask;
class ImageManager;

class ImageLoadTask : public QRunnable {
public:
    /**
     * @brief Constructor
     * @param avatarName     图片文件名
     * @param resourcePrefix 资源路径前缀
     * @param manager AvatarManager实例指针，用于回调
     */
    explicit ImageLoadTask(const QString& avatarName, const QString& resourcePrefix, ImageManager* manager);

    /**
     * @brief 异步加载任务的主执行函数
     *
     * 在后台线程中执行，负责：
     * 1. 检查图片是否已在缓存中
     * 2. 从文件系统加载图片图片
     * 3. 通过Qt::QueuedConnection通知主线程加载结果
     */
    void run() override;

private:
    //---------------------------------------------------------------------------
    // Section Name: Private Members
    //---------------------------------------------------------------------------
    QString         m_avatarName;      /*!< Avatar file name               */
    QString         m_resourcePrefix;  /*!< Avatar resources prefix        */
    ImageManager*  m_manager;         /*!< ImageManager instance pointer */
    //---------------------------------------------------------------------------
    // Section Name: Private Members End
    //---------------------------------------------------------------------------
};


class ImageManager : public QObject {
    Q_OBJECT

public:

    static ImageManager& instance();

    /**
     * @brief 初始化图片列表（异步操作）
     * @param resourcePrefix 图片资源路径前缀，默认为 IMAGE_DEFAULT_CIRCLE_PREFIX
     *
     * 此方法异步扫描指定路径下的图片文件，不会阻塞UI线程。
     * 初始化完成后会发射`initializeCompleted`信号。
     */
    void        initialize(const QString& resourcePrefix = IMAGE_DEFAULT_CIRCLE_PREFIX);

public:
    /**************************  === Public Getters ===  ***************************/
    /**
     * @brief 获取指定图片
     * @param avatarName 图片文件名
     * @return 图片QPixmap，如果未加载则返回默认图片
     *
     * 如果图片在缓存中，立即返回；否则返回默认图片并触发异步加载。
     * 加载完成后会发射`avatarLoaded`信号。
     */
    QPixmap     getAvatar(const QString& avatarName) const;

    /**
     * @brief  获取随机图片名称
     * @return 随机选择的图片文件名，如果没有可用图片则返回空字符串
     */
    QString     getRandomAvatar() const;

    /**
     * @brief 获取图片总数
     * @return 可用图片的总数
     */
    int         getAvatarCount() const;

    /**
     * @brief 获取所有可用图片列表
     * @return 所有可用图片文件名的列表
     * @note 线程安全
     */
    QStringList getAvailableAvatars() const;

    /**
     * @brief 获取当前缓存使用量
     * @return 当前缓存使用的内存大小（KB）
     */
    int         getCacheUsage() const;
    /**************************  === Public Getters End ===  ***********************/

public:

    /**************************  === Public Operations ===  ***************************/

    void preloadAvatar(const QString& avatarName);
    void preloadAvatars(const QStringList& avatarNames);

    /**
     * @brief 检查是否有指定图片
     * @param avatarName 图片文件名
     * @return 如果图片存在返回true，否则返回false
     */
    bool hasAvatar(const QString& avatarName) const;

    /**
     * @brief 检查图片是否已加载到缓存
     * @param avatarName 图片文件名
     * @return 如果已在缓存中返回true，否则返回false
     */
    bool isAvatarLoaded(const QString& avatarName) const;

    /**************************  === Public Operations ===  ***************************/


    /**************************  === Public Setters ===  ******************************/
    /**
     * @brief 设置缓存大小限制
     * @param maxSizeInMB 最大缓存大小（MB），默认50MB
     *
     * 设置缓存的最大大小，超出限制时会自动清理最少使用的图片。
     */
    void setCacheSize(int maxSizeInMB = 50);

    /**
     * @brief 设置预加载策略
     * @param enabled 是否启用智能预加载，默认true
     * @param maxPreloadCount 最大同时预加载数量，默认20
     *
     * 智能预加载会根据用户的访问模式预测并预加载可能需要的图片。
     */
    void setPreloadStrategy(bool enabled = true, int maxPreloadCount = 20);

    /**
     * @brief 清空所有缓存
     *
     * 立即清空所有已加载的图片缓存，释放内存。
     * 下次访问时需要重新加载。
     */
    void clearCache();
    /**************************  === Public Setters End ===  ***************************/

signals:
    //---------------------------------------------------------------------------
    // Section Signals @{
    //---------------------------------------------------------------------------
    /**
     * @brief 图片加载完成信号
     * @param avatarName 已加载的图片文件名
     * @param pixmap 加载完成的图片图片
     *
     * 当异步加载完成时发射此信号，UI可以监听此信号更新显示。
     */
    void avatarLoaded(const QString& avatarName, const QPixmap& pixmap);

    /**
     * @brief 图片加载失败信号
     * @param avatarName 加载失败的图片文件名
     *
     * 当图片文件不存在或加载失败时发射此信号。
     */
    void avatarLoadFailed(const QString& avatarName);

    /**
     * @brief 初始化完成信号
     * @param totalCount 发现的图片总数
     *
     * 当initialize()方法完成图片列表扫描时发射此信号。
     */
    void initializeCompleted(int totalCount);
    //---------------------------------------------------------------------------
    // Section Signals End @}
    //---------------------------------------------------------------------------

private slots:
    //---------------------------------------------------------------------------
    // Private Slots @{
    //---------------------------------------------------------------------------
    /**
     * @brief 图片加载完成的内部槽函数
     * @param avatarName 已加载的图片文件名
     * @param pixmap 加载完成的图片图片
     */
    void onAvatarLoadFinished(const QString& avatarName, const QPixmap& pixmap);

    /**
     * @brief 缓存清理定时器槽函数
     * 定期检查和清理缓存，释放不常用的图片。
     */
    void onCleanupTimer();
    //---------------------------------------------------------------------------
    // Private Slots End @}
    //---------------------------------------------------------------------------

private:

    //---------------------------------------------------------------------------
    // Section : For Singleton Pattern @{
    //---------------------------------------------------------------------------
    ImageManager();
    ~ImageManager();
    ImageManager(const ImageManager&) = delete;
    ImageManager& operator=(const ImageManager&) = delete;
    //---------------------------------------------------------------------------
    // Section : For Singleton Pattern End @}
    //---------------------------------------------------------------------------

private:

    /**************************  === Internal Methods ===  ***************************/
    /**
     * @brief 异步加载图片（内部方法）
     * @param avatarName 要加载的图片文件名
     *
     * 创建AvatarLoadTask并提交到线程池执行。
     */
    void   loadImageAsync(const QString& avatarName) const;

    /**
     * @brief 获取默认图片（内部方法）
     * @return 默认图片QPixmap
     *
     * 当请求的图片不存在或正在加载时返回的默认图片。
     */
    QPixmap getDefaultAvatar() const;

    /**
     * @brief 更新访问历史（内部方法）
     * @param avatarName 被访问的图片文件名
     *
     * 记录图片访问历史，用于智能预加载和LRU缓存。
     */
    void updateAccessHistory(const QString& avatarName) const;

    /**
     * @brief 智能预加载相关图片（内部方法）
     * @param currentAvatar 当前访问的图片文件名
     *
     * 根据当前访问的图片，预测并预加载可能需要的相关图片。
     * 使用简单的策略：预加载同一系列的前后几个图片。
     */
    void smartPreload(const QString& currentAvatar) const;

private:
    //---------------------------------------------------------------------------
    // Private variables @{
    //---------------------------------------------------------------------------
    /**
     * @brief 图片资源路径前缀
     * @example ":/header/Circle/" 或 ":/header/Square/"
     */
    QString m_resourcePrefix;

    /**
     * @brief 所有可用图片文件名列表
     *
     * 在initialize()时填充，包含所有发现的图片文件名。
     * 受m_avatarListLock保护。
     */
    QStringList m_availableAvatars;

    /**
     * @brief LRU图片缓存
     *
     * 使用QCache实现的LRU缓存，自动管理内存使用。
     * Key: 图片文件名, Value: QPixmap指针
     * 受m_cacheLock读写锁保护。
     */
    mutable QCache<QString, QPixmap> m_avatarCache;


    /**
     * @brief 缓存读写锁
     *
     * 保护m_avatarCache的并发访问。
     * 读操作（查找）使用读锁，写操作（插入/删除）使用写锁。
     */
    mutable QReadWriteLock m_cacheLock;

    /**
     * @brief 图片列表互斥锁
     *
     * 保护m_availableAvatars的并发访问。
     */
    mutable QMutex m_avatarListLock;

    /**
     * @brief 正在加载的图片集合
     *
     * 防止同一个图片被重复加载。
     * 受m_loadingLock保护。
     */
    mutable QSet<QString> m_loadingAvatars;

    /**
     * @brief 加载状态锁
     *
     * 保护m_loadingAvatars的并发访问。
     */
    mutable QMutex m_loadingLock;


    /**
     * @brief 是否启用智能预加载
     *
     * 如果启用，会根据访问模式预测并预加载可能需要的图片。
     */
    bool m_preloadEnabled;

    /**
     * @brief 最大同时预加载数量
     *
     * 限制同时进行的预加载任务数量，避免过载。
     */
    int m_maxPreloadCount;

    /**
     * @brief 图片访问历史记录
     *
     * 记录最近访问的图片，用于智能预加载算法。
     * 最新访问的在列表前面，受m_historyLock保护。
     */
    mutable QStringList m_accessHistory;

    /**
     * @brief 访问历史锁
     *
     * 保护m_accessHistory的并发访问。
     */
    mutable QMutex m_historyLock;

    // ==================== 内存管理 ====================

    /**
     * @brief 缓存清理定时器
     *
     * 定期触发缓存清理，释放不常用的图片。
     * 默认5分钟触发一次。
     */
    QTimer* m_cleanupTimer;

    /**
     * @brief 线程池指针
     *
     * 用于执行异步加载任务，默认使用QThreadPool::globalInstance()。
     */
    QThreadPool* m_threadPool;

    /**
     * @brief 最大缓存大小（MB）
     *
     * 缓存的最大内存使用限制，超出时会自动清理最少使用的图片。
     */
    int m_maxCacheSizeMB;

    //---------------------------------------------------------------------------
    // Private variables End @}
    //---------------------------------------------------------------------------
private:
    friend class ImageLoadTask;
};




#endif // IMAGEMANAGER_H
