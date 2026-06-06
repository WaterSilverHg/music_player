#include "mainwindow.h"
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QStringConverter>
#include <QDir>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("Music Player");
    a.setApplicationVersion("2.0.0");
    a.setOrganizationName("MusicPlayer");

    // 设置 FFmpeg 媒体后端（用于 RTSP 流播放）
    qputenv("QT_MEDIA_BACKEND", "ffmpeg");

    // 加载 QSS 深蓝×紫主题
    // 首先尝试从资源加载
    QString styleSheet;
    QFile styleFile(":/resources/style.qss");
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&styleFile);
        stream.setEncoding(QStringConverter::Utf8);
        styleSheet = stream.readAll();
        styleFile.close();
        qDebug() << "[Main] QSS 主题已从资源加载";
    } else {
        // 如果资源加载失败，尝试从文件系统加载
        QString appDir = QCoreApplication::applicationDirPath();
        QString stylePath = QDir(appDir).filePath("resources/style.qss");
        QFile fsStyleFile(stylePath);
        if (fsStyleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&fsStyleFile);
            stream.setEncoding(QStringConverter::Utf8);
            styleSheet = stream.readAll();
            fsStyleFile.close();
            qDebug() << "[Main] QSS 主题已从文件系统加载: " << stylePath;
        } else {
            qWarning() << "[Main] 无法加载 QSS 主题文件";
            qWarning() << "[Main] 尝试的路径: resources/style.qss, " << stylePath;
        }
    }
    
    if (!styleSheet.isEmpty()) {
        a.setStyleSheet(styleSheet);
    }

    MainWindow w;
    w.show();
    return a.exec();
}
