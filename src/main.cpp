#include "app/application.h"
#include "net/askpass.h"
#include "storage/databasemanager.h"
#include "ui/mainwindow.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    if (argc > 1 && QByteArray(argv[1]) == "-askpass")
        return AskPass::run();

    QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL, true);
    QCoreApplication::setAttribute(Qt::AA_CompressHighFrequencyEvents, true);

    Application app(argc, argv);

    if (!DatabaseManager::instance().open()) {
        QMessageBox::critical(nullptr, QStringLiteral("LinHub"),
                              QStringLiteral("无法打开会话数据库：\n%1")
                                  .arg(DatabaseManager::instance().lastError()));
        return 1;
    }

    MainWindow w;
    w.show();
    return app.exec();
}
