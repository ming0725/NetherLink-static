#include "PostRepository.h"

#include <QImageReader>
#include <QMetaObject>
#include <QRunnable>
#include <QThread>
#include <QThreadPool>
#include <QUuid>

#include "features/friend/data/UserRepository.h"

namespace {

constexpr int kSamplePostCount = 40;

const QString kPostContent = QString(
        "救命！今天下班路上抬头一看\n"
        "直接被天空美到走不动路\n"
        "这哪是晚霞啊 分明是神仙打翻调色盘\n"
        "\n"
        "✨【绝美时刻记录】\n"
        "▪️17:38 粉色棉花糖云开始膨胀\n"
        "▪️18:02 整片天空变成橘子汽水\n"
        "▪️18:20 紫红色晚霞开始\"燃烧\"\n"
        "（手机拍到没电预警⚡️）\n"
        "\n"
        "【拍照小心机】\n"
        "1⃣ 找到路灯/电线杆当前景（构图立刻高级）\n"
        "2⃣ 调低曝光拍剪影（发丝都在发光）\n"
        "3⃣ 对焦云层裂缝（金光穿透效果绝了）\n"
        "\n"
        "【朋友圈文案已备好】\n"
        "\"今天的天空在写浪漫诗\"✍️\n"
        "\"上帝下班前画的速写\"\n"
        "救命！今天下班路上抬头一看\n"
        "直接被天空美到走不动路\n"
        "这哪是晚霞啊 分明是神仙打翻调色盘\n"
        "\n"
        "✨【绝美时刻记录】\n"
        "▪️17:38 粉色棉花糖云开始膨胀\n"
        "▪️18:02 整片天空变成橘子汽水\n"
        "▪️18:20 紫红色晚霞开始\"燃烧\"\n"
        "（手机拍到没电预警⚡️）\n"
        "\n"
        "【拍照小心机】\n"
        "1⃣ 找到路灯/电线杆当前景（构图立刻高级）\n"
        "2⃣ 调低曝光拍剪影（发丝都在发光）\n"
        "3⃣ 对焦云层裂缝（金光穿透效果绝了）\n"
        "\n"
        "【朋友圈文案已备好】\n"
        "\"今天的天空在写浪漫诗\"✍️\n"
        "\"上帝下班前画的速写\"\n"
        "救命！今天下班路上抬头一看\n"
        "直接被天空美到走不动路\n"
        "这哪是晚霞啊 分明是神仙打翻调色盘\n"
        "\n"
        "✨【绝美时刻记录】\n"
        "▪️17:38 粉色棉花糖云开始膨胀\n"
        "▪️18:02 整片天空变成橘子汽水\n"
        "▪️18:20 紫红色晚霞开始\"燃烧\"\n"
        "（手机拍到没电预警⚡️）\n"
        "\n"
        "【拍照小心机】\n"
        "1⃣ 找到路灯/电线杆当前景（构图立刻高级）\n"
        "2⃣ 调低曝光拍剪影（发丝都在发光）\n"
        "3⃣ 对焦云层裂缝（金光穿透效果绝了）\n"
        "\n"
        "【朋友圈文案已备好】\n"
        "\"今天的天空在写浪漫诗\"✍️\n"
        "\"上帝下班前画的速写\"\n"
        "救命！今天下班路上抬头一看\n"
        "直接被天空美到走不动路\n"
        "这哪是晚霞啊 分明是神仙打翻调色盘\n"
        "\n"
        "✨【绝美时刻记录】\n"
        "▪️17:38 粉色棉花糖云开始膨胀\n"
        "▪️18:02 整片天空变成橘子汽水\n"
        "▪️18:20 紫红色晚霞开始\"燃烧\"\n"
        "（手机拍到没电预警⚡️）\n"
        "\n"
        "【拍照小心机】\n"
        "1⃣ 找到路灯/电线杆当前景（构图立刻高级）\n"
        "2⃣ 调低曝光拍剪影（发丝都在发光）\n"
        "3⃣ 对焦云层裂缝（金光穿透效果绝了）\n"
        "\n"
        "【朋友圈文案已备好】\n"
        "\"今天的天空在写浪漫诗\"✍️\n"
        "\"上帝下班前画的速写\"\n"
        "\"偷看了黄昏的日记本\"\n");

QSize imageSizeForSource(const QString& source)
{
    if (source.isEmpty()) {
        return {};
    }

    QImageReader reader(source);
    reader.setAutoTransform(true);
    return reader.size();
}

} // namespace

PostRepository::PostRepository(QObject* parent)
        : QObject(parent)
{
}

PostRepository& PostRepository::instance()
{
    static PostRepository repo;
    return repo;
}

QVector<PostSummary> PostRepository::requestPostFeed(const PostFeedRequest& query) const
{
    QMutexLocker locker(&mutex);
    QVector<PostSummary> result;
    if (query.limit <= 0 || query.offset < 0 || query.offset >= kSamplePostCount) {
        return result;
    }

    const int end = qMin(kSamplePostCount, query.offset + query.limit);
    result.reserve(end - query.offset);
    for (int index = query.offset; index < end; ++index) {
        result.push_back(buildSummary(buildPostAt(index)));
    }
    return result;
}

PostDetailData PostRepository::requestPostDetail(const PostDetailRequest& query) const
{
    QMutexLocker locker(&mutex);
    const int index = postIndexForId(query.postId);
    if (index < 0) {
        return {};
    }

    const Post post = buildPostAt(index);
    const User author = UserRepository::instance().requestUserDetail({post.authorID});
    return PostDetailData{
            post.postID,
            post.title,
            post.content,
            post.authorID,
            author.nick,
            author.avatarPath,
            post.picturesPath,
            post.likes,
            post.commentCount,
            post.isLiked,
            post.createdAt,
            post.contentCreatedAt
    };
}

QString PostRepository::requestPostDetailAsync(const PostDetailRequest& query, int delayMs)
{
    const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QThreadPool::globalInstance()->start(QRunnable::create([this, requestId, query, delayMs]() {
        const int boundedDelayMs = qMax(0, delayMs);
        if (boundedDelayMs > 0) {
            QThread::msleep(static_cast<unsigned long>(boundedDelayMs));
        }

        const PostDetailData detail = requestPostDetail(query);
        QMetaObject::invokeMethod(this, [this, requestId, detail]() {
            emit postDetailReady(requestId, detail);
        }, Qt::QueuedConnection);
    }));
    return requestId;
}

bool PostRepository::setPostLiked(const QString& postId, bool liked)
{
    PostSummary summary;

    {
        QMutexLocker locker(&mutex);
        const int index = postIndexForId(postId);
        if (index < 0) {
            return false;
        }

        Post post = buildPostAt(index);
        if (post.isLiked != liked) {
            post.isLiked = liked;
            post.likes = qMax(0, post.likes + (liked ? 1 : -1));
            m_likeStates.insert(post.postID, {post.likes, post.isLiked});
        }
        summary = buildSummary(post);
    }

    emit postUpdated(summary);
    return true;
}

Post PostRepository::buildPostAt(int index) const
{
    if (index < 0 || index >= kSamplePostCount) {
        return {};
    }

    const QStringList authorIDs = {
            "u001", "u002", "u003", "u004", "u005",
            "u006", "u007", "u008", "u009", "u010"
    };

    const QDateTime baseTime = QDateTime::fromString("2024-05-21T18:00:00", Qt::ISODate);

    Post post;
    post.postID = QString("p%1").arg(index + 1, 3, 10, QChar('0'));
    post.title = QString("Post 标题 %1").arg(index + 1);
    post.content = kPostContent;
    post.likes = (index * 137 + 211) % 1000;
    post.commentCount = (index * 29 + 17) % 200;
    post.authorID = authorIDs.at((index * 3 + 1) % authorIDs.size());
    post.createdAt = baseTime.addSecs(-index * 1800);
    post.contentCreatedAt = post.createdAt;
    const QString imagePath = QString(":/resources/post/%1.jpg").arg((index * 7 + 3) % 10);
    post.thumbnailPath = imagePath;
    post.thumbnailSize = imageSizeForSource(imagePath);
    post.picturesPath.append(imagePath);

    const auto likeIt = m_likeStates.constFind(post.postID);
    if (likeIt != m_likeStates.constEnd()) {
        post.likes = likeIt->likes;
        post.isLiked = likeIt->isLiked;
    }
    return post;
}

int PostRepository::postIndexForId(const QString& postId) const
{
    if (!postId.startsWith(QLatin1Char('p'))) {
        return -1;
    }

    bool ok = false;
    const int serial = postId.mid(1).toInt(&ok);
    const int index = serial - 1;
    return ok && index >= 0 && index < kSamplePostCount ? index : -1;
}

PostSummary PostRepository::buildSummary(const Post& post) const
{
    const User author = UserRepository::instance().requestUserDetail({post.authorID});
    return PostSummary{
            post.postID,
            post.title,
            post.thumbnailPath,
            post.thumbnailSize,
            post.authorID,
            author.nick,
            author.avatarPath,
            post.likes,
            post.commentCount,
            post.isLiked,
            post.createdAt
    };
}
