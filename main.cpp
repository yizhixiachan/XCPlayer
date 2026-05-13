#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickStyle>
#include "XCPlayer.h"
#include "platform/NativeEventFilter.h"
#include <QQmlContext>
#include "manager/CoverManager.h"
#include "manager/DatabaseManager.h"
#include <QStandardPaths>
#include <QDir>
#include <QSettings>

int main(int argc, char *argv[])
{
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
    QQuickStyle::setStyle("Basic");

    QGuiApplication app(argc, argv);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &XCPlayer::GetInstance(), &XCPlayer::Quit);

    QString launchFile = "";
    QStringList args = app.arguments();
    if(args.size() > 1) {
        // 从 args[1] 开始，用空格拼接所有剩余参数
        QStringList pathParts;
        for(int i = 1; i < args.size(); i++) {
            pathParts.append(args[i]);
        }
        launchFile = pathParts.join(" ");
        if(!QFile::exists(launchFile)) {
            launchFile.clear();
        }
    }

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    // 复用 Qt Quick 底层的 D3D11 Device
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated,
        &app,[](QObject *obj, const QUrl &) {
            if(QQuickWindow* window = qobject_cast<QQuickWindow*>(obj)) {
                QObject::connect(window, &QQuickWindow::beforeRendering, window, [window]() {
                    static bool d3d_initialized = false;
                    if(!d3d_initialized) {
                        d3d_initialized = true;
                        QSGRendererInterface* ri = window->rendererInterface();
                        if(ri) {
                            void* dev = ri->getResource(window, QSGRendererInterface::DeviceResource);
                            XCPlayer::GetInstance().SetD3D11Device(dev);
                        }
                    }
                });
            }
        },
        Qt::QueuedConnection);

    app.setOrganizationDomain("appXCPlayer");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(
        QSettings::IniFormat,
        QSettings::UserScope,
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));

    QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/XCPlayer.db";
    QDir().mkpath(QFileInfo(dbPath).path());
    DatabaseManager::GetInstance().Init(dbPath);

    NativeEventFilter eventFilter;
    app.installNativeEventFilter(&eventFilter);
    engine.rootContext()->setContextProperty("LaunchFile", launchFile);
    engine.rootContext()->setContextProperty("NativeEventFilter", &eventFilter);
    engine.addImageProvider(QString("covers"), new CoverProvider());
    engine.loadFromModule("XCPlayer", "Main");

    return app.exec();
}
