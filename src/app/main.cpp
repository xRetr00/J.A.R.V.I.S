#include <QApplication>

#include "app/JarvisApplication.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationDisplayName(QStringLiteral("JARVIS"));
    QApplication::setApplicationName(QStringLiteral("JARVIS"));
    QApplication::setOrganizationName(QStringLiteral("JARVIS"));

    JarvisApplication jarvis;
    if (!jarvis.initialize()) {
        return 1;
    }

    return app.exec();
}
