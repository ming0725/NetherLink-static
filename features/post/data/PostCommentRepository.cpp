#include "PostCommentRepository.h"

#include <QDateTime>
#include <QMetaObject>
#include <QRunnable>
#include <QStringList>
#include <QThread>
#include <QThreadPool>
#include <QUuid>

#include "app/state/CurrentUser.h"
#include "features/friend/data/UserRepository.h"

namespace {

constexpr int kSampleCommentCount = 72;

const QStringList& commentSamples()
{
    static const QStringList samples = {
            QStringLiteral("这个开局点可以啊，平原加村庄基本就是养老档标配。先插火把，别问我怎么知道的 🔥"),
            QStringLiteral("苦力怕贴脸真的经典，前期最贵的方块不是钻石，是刚摆好的箱子和熔炉。"),
            QStringLiteral("村民交易所建议先做防僵尸隔离，门口再放铁傀儡，不然一晚上回来全员打折失败。"),
            QStringLiteral("经验修补刷出来就毕业一半了，剩下就是把每个村民的职业方块锁好，别让他们乱认 📚"),
            QStringLiteral("这个樱花山谷很适合做主城，山腰建仓库，河边做码头，晚上挂灯会特别好看。"),
            QStringLiteral("下界合金套可以去打龙了，记得带水桶和慢落药水，末地岛边缘真的很容易出事故 🐉"),
            QStringLiteral("红石背面像炒面太真实了，只要正面门能开，服务器里就没人会追究线路美观。"),
            QStringLiteral("刷铁机上线之后就舒服了，铁砧、漏斗、铁轨都不用抠抠搜搜算材料。"),
            QStringLiteral("蓝冰下界交通建议每段都放牌子，不然新人第一次进来真的会坐船坐到别人家仓库 🚇"),
            QStringLiteral("末地城开到鞘翅那一下最爽，回主城第一件事就是从最高的塔上跳下来试飞。"),
            QStringLiteral("远古残骸这东西就是越找越没有，准备回家了反而在脚边刷两个，老 Minecraft 了。"),
            QStringLiteral("模组清单可以加 JEI、小地图和一键整理，新人开荒体验会好很多 🧰"),
            QStringLiteral("光影确实能改变存档气质，但我电脑一开光影风扇就开始合成飞机音效。"),
            QStringLiteral("海底神殿排水属于看别人做很治愈，自己做十分钟就想去挖沙子的工程。"),
            QStringLiteral("建议把出生点附近的洞口都封一下，很多新档不是被怪打崩，是被家门口小黑屋偷袭崩的。"),
            QStringLiteral(
                    "长评论测试：这个帖子很有国服生存社区那味。\n"
                    "\n"
                    "先开荒。\n"
                    "再修家。\n"
                    "然后开始做刷铁机、村民交易所、刷怪塔。\n"
                    "\n"
                    "等这些机器都跑起来之后，大家又会开始嫌主城不好看。\n"
                    "于是拆了重建。\n"
                    "重建一半去打龙。\n"
                    "打完龙去找鞘翅。\n"
                    "找到鞘翅又回来继续修路。\n"
                    "\n"
                    "这就是服务器循环。")
    };
    return samples;
}

const QStringList& replySamples()
{
    static const QStringList samples = {
            QStringLiteral("先睡觉，别让幻翼出来团建 🛏️"),
            QStringLiteral("村民要用命名牌吗？我之前被僵尸清过一次档。"),
            QStringLiteral("蓝冰太贵的话前期可以先用普通冰过渡。"),
            QStringLiteral("慢落药水真的要带，别相信自己的搭桥手法 🪂"),
            QStringLiteral("红石能跑就行，别看背面，看了就想重做。"),
            QStringLiteral("刷铁机记得离村庄远一点，不然判定容易乱。"),
            QStringLiteral("这个种子适合开养老服，建筑党应该会很喜欢。"),
            QStringLiteral("鞘翅拿到之后建议第一时间附耐久和经验修补 ✨"),
            QStringLiteral("我一般先做临时仓库，不然开荒两天箱子就开始满地长。"),
            QStringLiteral("下界施工一定要带金头盔，猪灵比岩浆还烦。"),
            QStringLiteral("光影截图好看，真正挖矿我还是关掉，不然看不清矿。"),
            QStringLiteral(
                    "回复长文本测试：服务器前期最容易吵起来的不是资源分配，\n"
                    "是主城到底用什么风格。\n"
                    "\n"
                    "有人想中式。\n"
                    "有人想日式。\n"
                    "有人想蒸汽朋克。\n"
                    "最后大家一起住火柴盒。")
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

struct CommentUserIdentity {
    QString visibleName;
    QString avatarPath;
};

CommentUserIdentity commentUserIdentityForUserId(const QString& userId)
{
    const CurrentUser& currentUser = CurrentUser::instance();
    if (currentUser.isCurrentUserId(userId)) {
        return {currentUser.getUserName(), currentUser.getAvatarPath()};
    }

    const User user = UserRepository::instance().requestUserDetail({userId});
    return {visibleName(user), user.avatarPath};
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
    const CommentUserIdentity author = commentUserIdentityForUserId(authorId);
    const QString commentId = QStringLiteral("%1-c%2").arg(postId).arg(index + 1, 3, 10, QChar('0'));
    const int baseLikes = (index * 31 + 19) % 300;

    PostComment comment;
    comment.commentId = commentId;
    comment.postId = postId;
    comment.authorId = authorId;
    comment.authorName = author.visibleName;
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
    const CommentUserIdentity author = commentUserIdentityForUserId(authorId);
    const CommentUserIdentity target = commentUserIdentityForUserId(targetUserId);
    const QString replyId = QStringLiteral("%1-r%2").arg(commentId).arg(replyIndex + 1, 2, 10, QChar('0'));
    const int baseLikes = ((commentIndex + 1) * (replyIndex + 3) * 13 + 7) % 80;

    PostCommentReply reply;
    reply.replyId = replyId;
    reply.commentId = commentId;
    reply.postId = postId;
    reply.authorId = authorId;
    reply.authorName = author.visibleName;
    reply.authorAvatarPath = author.avatarPath;
    reply.targetUserId = targetUserId;
    reply.targetUserName = target.visibleName;
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
