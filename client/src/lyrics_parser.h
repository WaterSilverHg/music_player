#ifndef LYRICS_PARSER_H
#define LYRICS_PARSER_H

#include"global.h""

// 歌词行结构，包含时间戳和文本
struct LyricLine {
    double startTime;  // 开始时间（秒）
    double endTime;    // 结束时间（秒）
    QString text;      // 歌词文本
};

class LyricsParser {
public:
    LyricsParser();
    ~LyricsParser();

    // 解析LRC格式歌词
    bool parseLRC(const QString& lrcContent);
    
    // 从原始文本生成LRC格式
    QString  generateLRC(const QList<LyricLine>& lyrics);
    
    // 保存歌词到文件
    bool saveToFile(const QString& filePath);
    
    // 从文件加载歌词
    bool loadFromFile(const QString& filePath);
    
    // 获取当前时间对应的歌词索引
    int getCurrentLyricIndex(double time);
    
    // 获取歌词列表
    QList<LyricLine>& lyrics() { return m_lyrics; }
    
    // 设置歌词列表
    void setLyrics(const QList<LyricLine>& lyrics) { m_lyrics = lyrics; }
    
    // 清空歌词
    void clear() { m_lyrics.clear(); }
    
    // 是否有歌词
    bool hasLyrics() const { return !m_lyrics.empty(); }
    
    // 格式化时间为LRC格式（分:秒.毫秒）
    static QString formatTimeToLRC(double time);
    
    // 解析LRC时间标签为秒
    static double parseLRCTime(const QString& timeTag);

private:
    QList<LyricLine> m_lyrics;  // 歌词列表
};

#endif // LYRICS_PARSER_H
