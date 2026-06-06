#ifndef PLAYLIST_H
#define PLAYLIST_H

#include"global.h"

class Playlist {
public:
    Playlist(const QString &name);

    QString name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }

    QStringList &filePathList() { return m_filePathList; }
    const QStringList &filePathList() const { return m_filePathList; }

    int currentIndex() const { return m_currentIndex; }
    void setCurrentIndex(int index) { m_currentIndex = index; }

    int mediaCount() const { return m_filePathList.size(); }
    QString filePath(int index) const;
    void addMedia(const QString &filePath);
    void removeMedia(int index);
    void clear();
    
    // 持久化存储相关方法
    bool saveToFile(const QString &filePath) const;
    bool loadFromFile(const QString &filePath);

private:
    QString m_name;
    QStringList m_filePathList; // 存储文件路径
    int m_currentIndex; // 当前播放索引
};


// Playlist类实现
inline Playlist::Playlist(const QString &name)
    : m_name(name), m_currentIndex(-1)
{}

inline QString Playlist::filePath(int index) const
{
    if (index >= 0 && index < m_filePathList.size()) {
        return m_filePathList[index];
    }
    return QString();
}

inline void Playlist::addMedia(const QString &filePath)
{
    m_filePathList.append(filePath);
}

inline void Playlist::removeMedia(int index)
{
    if (index >= 0 && index < m_filePathList.size()) {
        m_filePathList.removeAt(index);
        if (m_currentIndex == index) {
            m_currentIndex = -1;
        } else if (m_currentIndex > index) {
            m_currentIndex--;
        }
    }
}

inline void Playlist::clear()
{
    m_filePathList.clear();
    m_currentIndex = -1;
}

inline bool Playlist::saveToFile(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream out(&file);
    out << "[Playlist]" << Qt::endl;
    out << "Name=" << m_name << Qt::endl;
    out << "CurrentIndex=" << m_currentIndex << Qt::endl;
    out << "MediaCount=" << m_filePathList.size() << Qt::endl;
    out << "[Media]" << Qt::endl;
    
    foreach (const QString &path, m_filePathList) {
        // 存储相对路径，方便移动项目
        out << path << Qt::endl;
    }
    
    file.close();
    return true;
}

inline bool Playlist::loadFromFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
//        return true;
    }
    
    QTextStream in(&file);
    enum Section { None, Playlist, Media } section = None;
    
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        
        if (line == "[Playlist]") {
            section = Playlist;
        } else if (line == "[Media]") {
            section = Media;
        } else if (section == Playlist) {
            if (line.startsWith("Name=")) {
                m_name = line.mid(5);
            } else if (line.startsWith("CurrentIndex=")) {
                m_currentIndex = line.mid(13).toInt();
            }
        } else if (section == Media) {
            if (!line.isEmpty()) {
                m_filePathList.append(line);
            }
        }
    }
    
    file.close();
    return true;
}
#endif
