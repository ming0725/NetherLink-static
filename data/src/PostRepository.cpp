#include "PostRepository.h"
#include <QRandomGenerator>
#include <QString>

static const QString postContent = QString(
"救命！今天下班路上抬头一看\n"
"直接被天空美到走不动路\n"
"这哪是晚霞啊 分明是神仙打翻调色盘\n"
"\n"
"✨【绝美时刻记录】\n"
"▪\uFE0F17:38 粉色棉花糖云开始膨胀\n"
"▪\uFE0F18:02 整片天空变成橘子汽水\n"
"▪\uFE0F18:20 紫红色晚霞开始\"燃烧\"\n"
"（手机拍到没电预警⚡\uFE0F）\n"
"\n"
"【拍照小心机】\n"
"1\uFE0F⃣ 找到路灯/电线杆当前景（构图立刻高级）\n"
"2\uFE0F⃣ 调低曝光拍剪影（发丝都在发光）\n"
"3\uFE0F⃣ 对焦云层裂缝（金光穿透效果绝了）\n"
"\n"
"【朋友圈文案已备好】\n"
"\"今天的天空在写浪漫诗\"✍\uFE0F\n"
"\"上帝下班前画的速写\n"
"\"偷看了黄昏的日记本\n"
"\n"
"小遗憾：\n"
"没带相机！！\n"
"手机根本装不下这份震撼\n"
"现在看着照片还是觉得\n"
"眼睛才是最好的镜头\n"
"\n"
"#晚霞收藏家 #手机摄影挑战 #治愈系天空 #朋友圈素材 #浪漫生活的记录者"
);

PostRepository::PostRepository(QObject* parent)
        : QObject(parent)
{
    generateSamplePosts();
}

PostRepository& PostRepository::instance() {
    static PostRepository repo;
    return repo;
}

QVector<Post> PostRepository::getAllPosts() const {
    QMutexLocker locker(&mutex);
    return postList;
}

Post PostRepository::getPost(const QString& postID) const {
    QMutexLocker locker(&mutex);
    for (const auto& post : postList) {
        if (post.postID == postID)
            return post;
    }
    return {};
}

void PostRepository::generateSamplePosts() {
    QStringList authorIDs = {
            "u001", "u002", "u003", "u004", "u005",
            "u006", "u007", "u008", "u009", "u010"
    };

    for (int i = 0; i < 40; ++i) {
        QString postID = QString("p%1").arg(i + 1, 3, 10, QChar('0'));
        QString title = QString("Post 标题 %1").arg(i + 1);
        int likes = QRandomGenerator::global()->bounded(1000);
        QString authorID = authorIDs[QRandomGenerator::global()->bounded(authorIDs.size())];
        QString imagePath = QString(":/resources/post/%1.jpg")
                .arg(QRandomGenerator::global()->bounded(10));
        Post post;
        post.postID = postID;
        post.title = title;
        post.likes = likes;
        post.authorID = authorID;
        post.content = postContent;
        post.picturesPath.append(imagePath);
        postList.append(post);
    }
}
