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
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
    QQuickStyle::setStyle("Basic");

    QGuiApplication app(argc, argv);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &XCPlayer::GetInstance(), &XCPlayer::Quit);

    QCommandLineParser parser;
    parser.addPositionalArgument("file", "The file to open.");
    parser.process(app);
    const QStringList args = parser.positionalArguments();
    QString launchFile;

    if(!args.isEmpty()) {
        launchFile = args.first();

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
        &app,[launchFile](QObject *obj, const QUrl &) {
            if(QQuickWindow* window = qobject_cast<QQuickWindow*>(obj)) {
                QObject::connect(window, &QQuickWindow::beforeRendering, window, [window, launchFile]() {
                    static bool d3d_initialized = false;
                    if(!d3d_initialized) {
                        d3d_initialized = true;
                        QSGRendererInterface* ri = window->rendererInterface();
                        if(ri) {
                            void* dev = ri->getResource(window, QSGRendererInterface::DeviceResource);
                            XCPlayer::GetInstance().SetD3D11Device(dev);
                        }

                        // 确保 D3D11 Device 就绪后，再触发命令行文件播放
                        if(!launchFile.isEmpty()) {
                            QMetaObject::invokeMethod(&XCPlayer::GetInstance(), [launchFile]() {
                                XCPlayer::GetInstance().PlayTempUrl(launchFile, QVariantMap());
                            }, Qt::QueuedConnection);
                        }
                    }
                });
            }
        },
        Qt::QueuedConnection);


    app.setOrganizationDomain("appXCPlayer");

    QString rootDataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(rootDataPath);

    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, rootDataPath);

    QString dbPath = rootDataPath + "/XCPlayer.db";
    DatabaseManager::GetInstance().Init(dbPath);

    NativeEventFilter eventFilter;
    app.installNativeEventFilter(&eventFilter);
    engine.rootContext()->setContextProperty("NativeEventFilter", &eventFilter);
    engine.addImageProvider(QString("covers"), new CoverProvider());
    engine.loadFromModule("XCPlayer", "Main");

    return app.exec();
}
