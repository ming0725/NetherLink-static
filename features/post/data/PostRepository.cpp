#include "PostRepository.h"

#include <QImageReader>
#include <QMetaObject>
#include <QRunnable>
#include <QStringList>
#include <QThread>
#include <QThreadPool>
#include <QUuid>

#include "app/state/CurrentUser.h"
#include "features/friend/data/UserRepository.h"

namespace {

constexpr int kSamplePostCount = 40;

struct AuthorIdentity {
    QString name;
    QString avatarPath;
};

AuthorIdentity authorIdentityForUserId(const QString& userId)
{
    const CurrentUser& currentUser = CurrentUser::instance();
    if (currentUser.isCurrentUserId(userId)) {
        return {currentUser.getUserName(), currentUser.getAvatarPath()};
    }

    const User author = UserRepository::instance().requestUserDetail({userId});
    return {author.nick, author.avatarPath};
}

const QStringList& postTitleSamples()
{
    static const QStringList samples = {
            QStringLiteral("生存服第一天就被苦力怕教育了"),
            QStringLiteral("村民交易所终于毕业，绿宝石自由"),
            QStringLiteral("这个樱花山谷种子真的适合开档"),
            QStringLiteral("下界合金套成型，准备去打末影龙"),
            QStringLiteral("红石门调了一晚上，终于不抽风了"),
            QStringLiteral("海底神殿排水记录，海绵不够用"),
            QStringLiteral("光影一开，主城像换了个游戏"),
            QStringLiteral("刷铁机上线，铁傀儡开始上班"),
            QStringLiteral("和朋友修了一条下界交通冰道"),
            QStringLiteral("末地城开箱，鞘翅终于到手"),
            QStringLiteral("挖矿挖到远古残骸的那一刻"),
            QStringLiteral("整理了一套适合新人开荒的模组清单")
    };
    return samples;
}

const QStringList& postContentSamples()
{
    static const QStringList samples = {
            QStringLiteral(
                    "今天开新档，出生点直接刷在平原村庄旁边，运气看起来还不错 🌲\n"
                    "\n"
                    "村子后面有一片小树林，再往前走两百格就是樱花林，截图党应该会很喜欢。\n"
                    "我本来只想先砍点木头，把第一晚安稳混过去。\n"
                    "\n"
                    "结果天刚黑，背后一个苦力怕贴脸打招呼，直接把临时基地炸成开放式厨房 💥\n"
                    "箱子、熔炉和半面墙一起消失，连小麦田都被顺手掀了。\n"
                    "\n"
                    "目前进度记录：\n"
                    "木镐 -> 石镐\n"
                    "临时房 -> 半开放\n"
                    "床 -> 还差一块羊毛 🛏️\n"
                    "心态 -> 勉强稳定\n"
                    "\n"
                    "明天第一件事就是把出生点插满火把，煤可以再挖，家不能再炸了。"),
            QStringLiteral(
                    "村民交易所终于修完了，这个工程比我预想中折磨太多了 🧱\n"
                    "\n"
                    "最开始我以为只是抓几个村民，放职业方块，再等他们乖乖上班。\n"
                    "真正开始以后才发现，运村民、卡位置、刷职业，每一步都能把人气笑。\n"
                    "\n"
                    "今天的施工流程：\n"
                    "抓村民 -> 铺轨\n"
                    "运村民 -> 翻车\n"
                    "改线路 -> 再翻车\n"
                    "刷附魔书 -> 终于出经验修补 📚\n"
                    "\n"
                    "现在已经有：\n"
                    "图书管理员 x 6\n"
                    "制箭师 x 3\n"
                    "盔甲匠 x 2\n"
                    "工具匠 x 2\n"
                    "经验修补刷出来的时候，全服都在公屏打 666，我当场觉得之前的铁轨没白铺。\n"
                    "\n"
                    "下一步想把交易大厅改成地下要塞风，用石砖、苔石砖、铁栏杆和灵魂灯做层次。\n"
                    "顺便把天花板补厚一点，别再有僵尸从上面掉下来 🧟"),
            QStringLiteral(
                    "分享一个今天跑图遇到的种子点位，整体地形很适合开长期生存服 🌸\n"
                    "\n"
                    "出生点附近不是那种特别夸张的神种，但胜在舒服，资源和风景都离得很近。\n"
                    "左边是樱花山谷，右边是深色橡木森林，河对面还有一座能直接住人的村庄。\n"
                    "\n"
                    "地下入口连着大型矿洞，前期铁、煤和铜都不缺，往深处走还能听到岩浆声 ⛏️\n"
                    "我已经在山腰挖了一个临时基地，窗户正好能看到整片山谷。\n"
                    "\n"
                    "个人感觉适合：\n"
                    "生存开荒\n"
                    "小型服务器\n"
                    "日式建筑\n"
                    "山谷主城\n"
                    "\n"
                    "早上看日出的时候很安静，晚上看怪刷在对面山坡的时候也很真实。\n"
                    "风景好归好，火把还是要老老实实插满。"),
            QStringLiteral(
                    "下界合金套终于成型，最近几天基本都在下界当矿工 🔥\n"
                    "\n"
                    "床炸法试了，TNT 也试了，最后发现最缺的不是远古残骸，而是继续挖下去的耐心。\n"
                    "每次准备回家，脚边又冒出来一块残骸，Minecraft 真的很会拿捏人。\n"
                    "\n"
                    "目前装备进度：\n"
                    "保护 IV\n"
                    "耐久 III\n"
                    "经验修补\n"
                    "摔落保护 IV 🛡️\n"
                    "\n"
                    "剑还差一个锋利 V，弓还差无限，药水箱也要再补一轮。\n"
                    "今晚准备进末地，末影珍珠、南瓜头和床都已经塞进潜影盒。\n"
                    "\n"
                    "朋友说他负责放床输出。\n"
                    "我负责相信他不会把自己送走。\n"
                    "\n"
                    "龙：危 🐉"),
            QStringLiteral(
                    "红石门调了一晚上，终于从抽风机器变成了正常入口 🔴\n"
                    "\n"
                    "一开始我只是想做个 2x2 活塞门，后来觉得太普通，就顺手改成隐藏门。\n"
                    "改着改着又想加密码锁，最后线路越接越长，地底空间被我挖成了红石机房。\n"
                    "\n"
                    "最离谱的是中继器延迟错一格，粘性活塞就会开始反复伸缩，像在给主城打节拍。\n"
                    "我对着线路看了半小时，才发现有一段红石粉被方块挡住了。\n"
                    "\n"
                    "现在版本终于稳定：\n"
                    "拉杆隐藏在书架后面。\n"
                    "门开的时候有音符盒提示。\n"
                    "关门会自动锁住。\n"
                    "\n"
                    "虽然背面红石像一盘炒面，但正面完全看不出来。\n"
                    "对建筑党来说，这就已经算大成功了 ✨"),
            QStringLiteral(
                    "今天服务器一起修下界交通，终于把几个主要据点连起来了 🚇\n"
                    "\n"
                    "目标本来很简单，就是把主城、刷怪塔、末地传送门和沙漠基地接到同一条冰道上。\n"
                    "实际施工以后才发现，材料、坐标和队友的方向感，每一个都可能出问题。\n"
                    "\n"
                    "现场情况：\n"
                    "有人忘带黑曜石。\n"
                    "有人在冰道上开船撞墙。\n"
                    "有人把猪灵引到施工现场 🐷\n"
                    "还有人把站台出口开到了岩浆湖旁边。\n"
                    "\n"
                    "好在最后还是修通了，蓝冰跑起来是真的快，两边挂灵魂灯也很有下界高速的感觉。\n"
                    "从主城到末地门现在只要几十秒，以后打龙和找末地城都方便很多。\n"
                    "\n"
                    "下次准备给每个站台做编号和颜色标识。\n"
                    "不然新人第一次进下界，真的像直接进了迷宫。")
    };
    return samples;
}

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
    const AuthorIdentity author = authorIdentityForUserId(post.authorID);
    return PostDetailData{
            post.postID,
            post.title,
            post.content,
            post.authorID,
            author.name,
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

bool PostRepository::adjustPostCommentCount(const QString& postId, int delta)
{
    if (postId.isEmpty() || delta == 0) {
        return false;
    }

    PostSummary summary;
    {
        QMutexLocker locker(&mutex);
        const int index = postIndexForId(postId);
        if (index < 0) {
            return false;
        }

        Post post = buildPostAt(index);
        const int nextCount = qMax(0, post.commentCount + delta);
        m_commentCountDeltas.insert(postId,
                                    m_commentCountDeltas.value(postId, 0) + nextCount - post.commentCount);
        post.commentCount = nextCount;
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
    post.title = postTitleSamples().at((index * 5 + 2) % postTitleSamples().size());
    post.content = postContentSamples().at((index * 7 + 1) % postContentSamples().size());
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
    post.commentCount = qMax(0, post.commentCount + m_commentCountDeltas.value(post.postID, 0));
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
    const AuthorIdentity author = authorIdentityForUserId(post.authorID);
    return PostSummary{
            post.postID,
            post.title,
            post.thumbnailPath,
            post.thumbnailSize,
            post.authorID,
            author.name,
            author.avatarPath,
            post.likes,
            post.commentCount,
            post.isLiked,
            post.createdAt
    };
}
