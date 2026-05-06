#include "PostCommentRepository.h"

#include <QDateTime>
#include <QMetaObject>
#include <QRunnable>
#include <QStringList>
#include <QThread>
#include <QThreadPool>
#include <QUuid>

#include "features/friend/data/UserRepository.h"

namespace {

constexpr int kSampleCommentCount = 72;

const QStringList& commentSamples()
{
    static const QStringList samples = {
            QStringLiteral("这张图的颜色太舒服了，尤其是云层边缘那一点点亮光，像游戏里刚刷新的天空盒。"),
            QStringLiteral("我也经常在下班路上拍天，结果每次手机相册都是同一个角度，但是看到的时候还是忍不住按快门。"),
            QStringLiteral("构图很干净，左边留白刚好把晚霞的层次放出来了。要是再压一点高光，云的纹理应该会更明显。"),
            QStringLiteral("今天这场晚霞真的离谱，我在地铁口也看到了，整条街都变成橘色。"),
            QStringLiteral("收藏了，下次拍天空的时候试试你说的低曝光剪影方法。"),
            QStringLiteral("这段文案有点会写，尤其是“天空在写浪漫诗”这句，很适合直接发朋友圈。"),
            QStringLiteral("看到这张突然想起以前服务器里大家一起等日落截图的晚上，光影包一开就特别有氛围。"),
            QStringLiteral("哈哈哈哈哈哈哈哈长文本测试：如果一条评论写得非常非常非常非常非常非常非常非常非常非常非常非常长，列表里应该先收起来，避免一条内容把整个评论区撑到很高。这里继续多写几句，用来验证查看全文展开后的高度刷新是否稳定，滚动条位置也不应该突然跳到奇怪的位置。")
    };
    return samples;
}

const QStringList& replySamples()
{
    static const QStringList samples = {
            QStringLiteral("同感，肉眼看到的永远比照片更夸张。"),
            QStringLiteral("可以试试锁曝光，颜色会稳很多。"),
            QStringLiteral("这句我也喜欢，已经记到备忘录了。"),
            QStringLiteral("我那边只看到一点点紫色，错过最佳时间了。"),
            QStringLiteral("回复长文本测试：如果一条评中评写得非常非常非常非常非常非常非常非常非常非常非常非常长，评中评也需要省略，特别是大家连续讨论拍摄参数的时候，一条回复可能会写很多内容。这里继续补充一些描述，确保它在默认状态下不会占用太多空间，点击查看全文以后再完整展示。")
    };
    return samples;
}

QStringList userIds()
{
    return {
            QStringLiteral("u001"),
            QStringLiteral("u002"),
            QStringLiteral("u003"),
            QStringLiteral("u004"),
            QStringLiteral("u005"),
            QStringLiteral("u006"),
            QStringLiteral("u007"),
            QStringLiteral("u008"),
            QStringLiteral("u009"),
            QStringLiteral("u010"),
            QStringLiteral("u011"),
            QStringLiteral("u012")
    };
}

QString visibleName(const User& user)
{
    if (!user.remark.isEmpty()) {
        return user.remark;
    }
    return user.nick;
}

} // namespace

PostCommentRepository::PostCommentRepository(QObject* parent)
    : QObject(parent)
{
}

PostCommentRepository& PostCommentRepository::instance()
{
    static PostCommentRepository repo;
    return repo;
}

PostCommentsPage PostCommentRepository::requestPostComments(const PostCommentsRequest& query) const
{
    QMutexLocker locker(&m_mutex);

    PostCommentsPage page;
    page.postId = query.postId;
    page.offset = qMax(0, query.offset);
    page.totalCount = kSampleCommentCount;
    if (query.postId.isEmpty() || query.limit <= 0 || page.offset >= kSampleCommentCount) {
        page.hasMore = false;
        return page;
    }

    const int end = qMin(kSampleCommentCount, page.offset + query.limit);
    page.comments.reserve(end - page.offset);
    for (int index = page.offset; index < end; ++index) {
        page.comments.append(buildCommentAt(query.postId, index));
    }
    page.hasMore = end < kSampleCommentCount;
    return page;
}

QString PostCommentRepository::requestPostCommentsAsync(const PostCommentsRequest& query, int delayMs)
{
    const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QThreadPool::globalInstance()->start(QRunnable::create([this, requestId, query, delayMs]() {
        const int boundedDelayMs = qMax(0, delayMs);
        if (boundedDelayMs > 0) {
            QThread::msleep(static_cast<unsigned long>(boundedDelayMs));
        }

        const PostCommentsPage page = requestPostComments(query);
        QMetaObject::invokeMethod(this, [this, requestId, page]() {
            emit postCommentsReady(requestId, page);
        }, Qt::QueuedConnection);
    }));
    return requestId;
}

bool PostCommentRepository::setCommentLiked(const QString& commentId, bool liked)
{
    QMutexLocker locker(&m_mutex);
    const int index = commentIndexForId(commentId);
    if (index < 0) {
        return false;
    }

    const int baseLikes = (index * 31 + 19) % 300;
    LikeState state = m_commentLikeStates.value(commentId, {baseLikes, false});
    if (state.isLiked != liked) {
        state.isLiked = liked;
        state.likes = qMax(0, state.likes + (liked ? 1 : -1));
        m_commentLikeStates.insert(commentId, state);
    }
    return true;
}

bool PostCommentRepository::setReplyLiked(const QString& replyId, bool liked)
{
    QMutexLocker locker(&m_mutex);
    const int index = replyIndexForId(replyId);
    if (index < 0) {
        return false;
    }

    const int baseLikes = (index * 13 + 7) % 80;
    LikeState state = m_replyLikeStates.value(replyId, {baseLikes, false});
    if (state.isLiked != liked) {
        state.isLiked = liked;
        state.likes = qMax(0, state.likes + (liked ? 1 : -1));
        m_replyLikeStates.insert(replyId, state);
    }
    return true;
}

PostComment PostCommentRepository::buildCommentAt(const QString& postId, int index) const
{
    const QStringList ids = userIds();
    const QString authorId = ids.at((index * 5 + 2) % ids.size());
    const User author = UserRepository::instance().requestUserDetail({authorId});
    const QString commentId = QStringLiteral("%1-c%2").arg(postId).arg(index + 1, 3, 10, QChar('0'));
    const int baseLikes = (index * 31 + 19) % 300;

    PostComment comment;
    comment.commentId = commentId;
    comment.postId = postId;
    comment.authorId = authorId;
    comment.authorName = visibleName(author);
    comment.authorAvatarPath = author.avatarPath;
    comment.content = commentSamples().at(index % commentSamples().size());
    comment.createdAt = QDateTime::fromString(QStringLiteral("2024-05-22T12:00:00"), Qt::ISODate)
                                .addSecs(-index * 430);
    comment.likeCount = baseLikes;
    comment.isLiked = false;
    comment.totalReplyCount = (index * 7 + 3) % 6;

    if (const auto likeIt = m_commentLikeStates.constFind(commentId);
        likeIt != m_commentLikeStates.constEnd()) {
        comment.likeCount = likeIt->likes;
        comment.isLiked = likeIt->isLiked;
    }

    for (int replyIndex = 0; replyIndex < comment.totalReplyCount; ++replyIndex) {
        comment.replies.append(buildReplyAt(postId, commentId, authorId, index, replyIndex));
    }
    return comment;
}

PostCommentReply PostCommentRepository::buildReplyAt(const QString& postId,
                                                     const QString& commentId,
                                                     const QString& parentAuthorId,
                                                     int commentIndex,
                                                     int replyIndex) const
{
    const QStringList ids = userIds();
    const QString authorId = ids.at((commentIndex * 3 + replyIndex * 2 + 5) % ids.size());
    const QString targetUserId = replyIndex == 0
            ? parentAuthorId
            : ids.at((commentIndex * 3 + replyIndex * 2 + 3) % ids.size());
    const User author = UserRepository::instance().requestUserDetail({authorId});
    const User target = UserRepository::instance().requestUserDetail({targetUserId});
    const QString replyId = QStringLiteral("%1-r%2").arg(commentId).arg(replyIndex + 1, 2, 10, QChar('0'));
    const int baseLikes = ((commentIndex + 1) * (replyIndex + 3) * 13 + 7) % 80;

    PostCommentReply reply;
    reply.replyId = replyId;
    reply.commentId = commentId;
    reply.postId = postId;
    reply.authorId = authorId;
    reply.authorName = visibleName(author);
    reply.authorAvatarPath = author.avatarPath;
    reply.targetUserId = targetUserId;
    reply.targetUserName = visibleName(target);
    if (replyIndex > 0) {
        reply.targetReplyId = QStringLiteral("%1-r%2").arg(commentId).arg(replyIndex, 2, 10, QChar('0'));
    }
    reply.content = replySamples().at((commentIndex + replyIndex) % replySamples().size());
    reply.createdAt = QDateTime::fromString(QStringLiteral("2024-05-22T12:30:00"), Qt::ISODate)
                              .addSecs(-(commentIndex * 430 + replyIndex * 83));
    reply.likeCount = baseLikes;
    reply.isLiked = false;

    if (const auto likeIt = m_replyLikeStates.constFind(replyId);
        likeIt != m_replyLikeStates.constEnd()) {
        reply.likeCount = likeIt->likes;
        reply.isLiked = likeIt->isLiked;
    }
    return reply;
}

int PostCommentRepository::commentIndexForId(const QString& commentId) const
{
    const int cPos = commentId.lastIndexOf(QStringLiteral("-c"));
    if (cPos < 0) {
        return -1;
    }

    bool ok = false;
    const int serial = commentId.mid(cPos + 2, 3).toInt(&ok);
    return ok && serial > 0 && serial <= kSampleCommentCount ? serial - 1 : -1;
}

int PostCommentRepository::replyIndexForId(const QString& replyId) const
{
    const int rPos = replyId.lastIndexOf(QStringLiteral("-r"));
    if (rPos < 0) {
        return -1;
    }

    bool ok = false;
    const int serial = replyId.mid(rPos + 2, 2).toInt(&ok);
    return ok && serial > 0 ? serial - 1 : -1;
}
