#include "PostRepository.h"

#include <QImageReader>
#include <QRandomGenerator>
#include <QTimer>
#include <QUuid>

#include "RepositoryTemplate.h"
#include "UserRepository.h"

namespace {

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

class PostFeedRequestOperation final
    : public RepositoryTemplate<PostFeedRequest, QVector<PostSummary>> {
public:
    explicit PostFeedRequestOperation(QVector<Post> posts)
        : m_posts(std::move(posts))
    {
    }

private:
    QVector<PostSummary> doRequest(const PostFeedRequest& query) const override
    {
        QVector<PostSummary> result;
        if (query.limit <= 0 || query.offset < 0 || query.offset >= m_posts.size()) {
            return result;
        }

        const int end = qMin(m_posts.size(), query.offset + query.limit);
        result.reserve(end - query.offset);

        for (int index = query.offset; index < end; ++index) {
            const Post& post = m_posts.at(index);
            const User author = UserRepository::instance().requestUserDetail({post.authorID});
            result.push_back(PostSummary{
                    post.postID,
                    post.title,
                    post.thumbnailPath,
                    post.thumbnailSize,
                    post.authorID,
                    author.nick,
                    author.avatarPath,
                    post.likes,
                    post.commentCount,
                    post.isLiked
            });
        }
        return result;
    }

    QVector<Post> m_posts;
};

class PostDetailRequestOperation final
    : public RepositoryTemplate<PostDetailRequest, PostDetailData> {
public:
    explicit PostDetailRequestOperation(QVector<Post> posts)
        : m_posts(std::move(posts))
    {
    }

private:
    void validate(const PostDetailRequest& query) const override
    {
        Q_UNUSED(query);
    }

    PostDetailData doRequest(const PostDetailRequest& query) const override
    {
        for (const Post& post : m_posts) {
            if (post.postID != query.postId) {
                continue;
            }

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
                    post.isLiked
            };
        }
        return {};
    }

    QVector<Post> m_posts;
};

} // namespace

PostRepository::PostRepository(QObject* parent)
        : QObject(parent)
{
    generateSamplePosts();
}

PostRepository& PostRepository::instance()
{
    static PostRepository repo;
    return repo;
}

QVector<PostSummary> PostRepository::requestPostFeed(const PostFeedRequest& query) const
{
    QMutexLocker locker(&mutex);
    return PostFeedRequestOperation(postList).request(query);
}

PostDetailData PostRepository::requestPostDetail(const PostDetailRequest& query) const
{
    QMutexLocker locker(&mutex);
    return PostDetailRequestOperation(postList).request(query);
}

QString PostRepository::requestPostDetailAsync(const PostDetailRequest& query, int delayMs)
{
    const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QTimer::singleShot(qMax(0, delayMs), this, [this, requestId, query]() {
        emit postDetailReady(requestId, requestPostDetail(query));
    });
    return requestId;
}

bool PostRepository::setPostLiked(const QString& postId, bool liked)
{
    PostSummary summary;
    bool updated = false;

    {
        QMutexLocker locker(&mutex);
        for (Post& post : postList) {
            if (post.postID != postId) {
                continue;
            }

            if (post.isLiked == liked) {
                summary = buildSummary(post);
                updated = true;
                break;
            }

            post.isLiked = liked;
            post.likes += liked ? 1 : -1;
            post.likes = qMax(0, post.likes);
            summary = buildSummary(post);
            updated = true;
            break;
        }
    }

    if (updated) {
        emit postUpdated(summary);
    }
    return updated;
}

void PostRepository::generateSamplePosts()
{
    const QStringList authorIDs = {
            "u001", "u002", "u003", "u004", "u005",
            "u006", "u007", "u008", "u009", "u010"
    };

    const QDateTime baseTime = QDateTime::fromString("2024-05-21T18:00:00", Qt::ISODate);

    for (int i = 0; i < 40; ++i) {
        Post post;
        post.postID = QString("p%1").arg(i + 1, 3, 10, QChar('0'));
        post.title = QString("Post 标题 %1").arg(i + 1);
        post.content = kPostContent;
        post.likes = QRandomGenerator::global()->bounded(1000);
        post.commentCount = QRandomGenerator::global()->bounded(200);
        post.authorID = authorIDs[QRandomGenerator::global()->bounded(authorIDs.size())];
        post.createdAt = baseTime.addSecs(-i * 1800);
        const QString imagePath = QString(":/resources/post/%1.jpg")
                                          .arg(QRandomGenerator::global()->bounded(10));
        post.thumbnailPath = imagePath;
        post.thumbnailSize = imageSizeForSource(imagePath);
        post.picturesPath.append(imagePath);
        postList.append(post);
    }
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
            post.isLiked
    };
}
