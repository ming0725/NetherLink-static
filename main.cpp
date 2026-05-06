#include <QApplication>
#include "app/frame/MainWindow.h"
#include "shared/services/AudioService.h"
#include "shared/theme/ThemeManager.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    ThemeManager::instance().applyToApplication(a);
    AudioService::instance().preloadUiSounds();
    MainWindow w;
    w.show();
    return a.exec();
}
