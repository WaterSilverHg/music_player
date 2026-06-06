#include "lyrics_parser.h"

LyricsParser::LyricsParser()
{}

LyricsParser::~LyricsParser()
{}

bool LyricsParser::parseLRC(const QString& lrcContent)
{
    m_lyrics.clear();

    QStringList lines = lrcContent.split("\n");
    // 使用 QRegularExpression 替代 QRegExp
    QRegularExpression timeRegex("\\[([0-9]{2}:[0-9]{2}\\.?[0-9]*)\\]");

    for (const QString& line : lines) {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty())
            continue;

        // 查找所有时间标签
        QList<double> timestamps;

        // 使用全局匹配查找所有时间标签
        QRegularExpressionMatchIterator iterator = timeRegex.globalMatch(trimmedLine);
        while (iterator.hasNext()) {
            QRegularExpressionMatch match = iterator.next();
            QString timeTag = match.captured(1);
            double time = parseLRCTime(timeTag);
            timestamps.append(time);
        }

        // 如果没有找到时间标签，跳过这一行
        if (timestamps.isEmpty())
            continue;

        // 提取歌词文本（最后一个时间标签之后的内容）
        QString text;

        // 查找最后一个匹配
        QRegularExpressionMatch lastMatch;
        QRegularExpressionMatchIterator lastIterator = timeRegex.globalMatch(trimmedLine);
        while (lastIterator.hasNext()) {
            lastMatch = lastIterator.next();
        }

        if (lastMatch.hasMatch()) {
            int lastTagEnd = lastMatch.capturedEnd(0);  // 最后一个匹配的结束位置
            text = trimmedLine.mid(lastTagEnd).trimmed();
        }

        // 创建歌词行
        for (double startTime : timestamps) {
            LyricLine lyric;
            lyric.startTime = startTime;
            lyric.endTime = -1; // 初始化为-1，后面会计算
            lyric.text = text;
            m_lyrics.append(lyric);
        }
    }

    // 按时间排序
    std::sort(m_lyrics.begin(), m_lyrics.end(), [](const LyricLine& a, const LyricLine& b) {
        return a.startTime < b.startTime;
    });

    // 计算每行的结束时间（下一行的开始时间）
    for (int i = 0; i < m_lyrics.size() - 1; ++i) {
        m_lyrics[i].endTime = m_lyrics[i+1].startTime;
    }

    // 最后一行的结束时间设置为很大的值
    if (!m_lyrics.isEmpty()) {
        m_lyrics.last().endTime = 1000000.0;
    }

    return !m_lyrics.isEmpty();
}

QString LyricsParser::generateLRC(const QList<LyricLine>& lyrics)
{
    QString  lrcContent;
    
    foreach (const LyricLine& lyric, lyrics) {
        lrcContent += "[" + formatTimeToLRC(lyric.startTime) + "]" + lyric.text + "\n";
    }
    
    return lrcContent;
}

bool LyricsParser::saveToFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "无法打开文件保存歌词: " << filePath;
        return false;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << generateLRC(m_lyrics);
    file.close();
    
    return true;
}

bool LyricsParser::loadFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "无法打开文件加载歌词: " << filePath;
        return false;
    }
    
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    QString lrcContent = in.readAll();
    file.close();
    
    return parseLRC(lrcContent);
}

int LyricsParser::getCurrentLyricIndex(double time)
{
    for (int i = 0; i < m_lyrics.size(); ++i) {
        if (time >= m_lyrics[i].startTime && time < m_lyrics[i].endTime) {
            return i;
        }
    }
    return -1; // 没有找到对应的歌词
}

QString LyricsParser::formatTimeToLRC(double time)
{
    int minutes = static_cast<int>(time) / 60;
    int seconds = static_cast<int>(time) % 60;
    int milliseconds = static_cast<int>((time - static_cast<int>(time)) * 100);
    
//    return QString("%1:%2.%3").arg(minutes, 2, 10, QLatin1Char('0'))
//                             .arg(seconds, 2, 10, QLatin1Char('0'))
//                             .arg(milliseconds, 2, 10, QLatin1Char('0'));
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%02d:%02d.%02d", minutes, seconds, milliseconds);
    QString result = buffer;
    //std::string result = std::format("{:02d}:{:02d}.{:02d}", minutes, seconds, milliseconds);
    return result;
}

double LyricsParser::parseLRCTime(const QString& timeTag)
{
    QStringList parts = timeTag.split(":");
    if (parts.size() != 2) {
        return 0.0;
    }
    
    int minutes = parts[0].toInt();
    double seconds = parts[1].toDouble();
    
    return minutes * 60.0 + seconds;
}
