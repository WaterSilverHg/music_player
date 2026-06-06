#ifndef HEAD_H
#define HEAD_H

#include <QMainWindow>
#include <QMediaPlayer>
#include <QFileDialog>
#include <QDirIterator>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QList>
#include <QStringList>
#include <QAudioOutput>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDebug>
#include <QListWidget>
#include <QInputDialog>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QSettings>
#include <QCoreApplication>
#include <QDir>
#include <QTextStream>

#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <QThread>
#include <QFuture>
#include <QtConcurrent>
#include <QUuid>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}

#endif // HEAD_H
