#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "global.h"
#include "lyrics_parser.h"
#include "apiclient.h"
#include "config_manager.h"
#include "rtsp_player.h"
#include "Playlist.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class Playlist;
class SpeechRecognizer;
class AudioExtractor;
struct RemoteSongInfo;

// 播放模式枚举
enum PlayMode {
    SinglePlay,      // 单曲播放
    SingleLoopPlay,  // 单曲循环
    LoopPlay,        // 循环播放
    RandomPlay       // 随机播放
};

// 音源模式
enum class SourceMode {
    Local,   // 本地模式
    Server   // 服务器模式
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // ---- 播放控制 ----
    void on_playButton_clicked();
    void on_prevButton_clicked();
    void on_nextButton_clicked();
    void on_playModeButton_clicked();

    // ---- 进度条和音量 ----
    void on_progressBar_sliderMoved(int position);
    void on_progressBar_sliderPressed();
    void on_progressBar_sliderReleased();
    void on_progressBar_valueChanged(int value);
    void on_volumeSlider_valueChanged(int value);

    // ---- 播放器状态 ----
    void updatePlayerStatus(QMediaPlayer::PlaybackState state);
    void updateProgress(qint64 position);
    void updateDuration(qint64 duration);
    void handlePlayerError(QMediaPlayer::Error error);

    // ---- 播放列表 ----
    void tab_currentChanged(int index);
    void onTabBarDoubleClicked(int index);
    void showPlaylistContextMenu(const QPoint& pos);
    void renameCurrentPlaylist();
    void setupPlaylistListWidget(QListWidget* widget);

    // ---- 菜单操作 ----
    void on_actionAdd_File_triggered();
    void on_actionAdd_Folder_triggered();
    void on_actionDelete_File_triggered();
    void on_actionNew_Playlist_triggered();
    void on_actionDelete_Playlist_triggered();
    void on_actionImport_Lyrics_triggered();

    // ---- 服务器连接 ----
    void on_actionConnect_Server_triggered();

    // ---- 服务器操作 ----
    void on_actionUpload_File_triggered();
    void on_actionDownload_File_triggered();

    // ---- 模式切换 ----
    void on_modeSwitchButton_clicked();

    // ---- 搜索 ----
    void on_searchLineEdit_textChanged(const QString& text);
    void on_searchLineEdit_returnPressed();

    // ---- 歌词 ----
    void on_lyricsTimer_timeout();
    void updateLyricsWithPosition(qint64 position);

    // ---- ApiClient 信号 ----
    void onServerConnected();
    void onServerDisconnected();
    void onServerFilesReceived(const QList<RemoteSongInfo>& files);
    void onSearchResultReceived(const QList<RemoteSongInfo>& results);
    void onStreamUrlReady(const QString& remoteId, const QString& rtspUrl);
    void onUploadFinished(bool success, const QString& fileId, const QString& fileName);
    void onLyricsReady(const QString& remoteId, const QByteArray& lrcData);
    void onLyricsStatus(const QString& remoteId, const QString& status);
    void onCoverReady(const QString& remoteId, const QByteArray& imageData);
    void onApiErrorOccurred(const QString& errorMsg);

private:
    Ui::MainWindow *ui;
    QAudioOutput *audioOutput;
    QMediaPlayer *player;
    QList<Playlist*> playlists;  // 历史播放列表（用于标签页）
    Playlist* localPlaylist = nullptr;   // 本地模式专用播放列表
    Playlist* serverPlaylist = nullptr;  // 服务器模式专用播放列表
    PlayMode currentPlayMode;
    bool isUpdatingProgress;
    bool wasPlayingBeforeSeek = false;  // 拖动进度条前是否正在播放

    // 音源模式
    SourceMode currentMode = SourceMode::Local;

    // 播放失败保护
    int consecutivePlayFailures = 0;  // 连续播放失败次数
    const int MAX_CONSECUTIVE_FAILURES = 3;  // 最大连续失败次数

    // 搜索控件
    QLineEdit* searchLineEdit;
    QListWidget* searchResultList;
    bool searchResultVisible = false;

    // 服务器状态
    bool serverConnected = false;
    QList<RemoteSongInfo> serverFileCache;

    // 歌词（仅用于显示，生成由服务端完成）
    QTimer *lyricsTimer;
    LyricsParser lyricsParser;
    QList<LyricLine> currentLyrics;
    QString currentLyricsFilePath;
    int prevLyricHighlight = -1;      // 上一次高亮的歌词行索引

    // 移除未使用的 RTSP 播放器
    // RtspPlayer* m_rtspPlayer = nullptr;

    // 当前播放的服务器歌曲 URL（HTTP）
    QString currentRtspUrl;
    QString currentRemoteId;

    // ---- 辅助方法 ----
    void addFilesToPlaylist(const QStringList &fileNames);
    void scanFolderForFiles(const QString &folderPath);
    void updatePlaylistView();
    void updatePlaylistSelection();  // 更新播放列表选中项
    QString formatTime(qint64 ms);
    int getCurrentPlaylistIndex();
    Playlist* getCurrentPlaylist();
    void playCurrentMedia();
    void playRemoteSong(const QString& remoteId, const QString& rtspUrl);

    // 持久化
    void saveAllPlaylists();
    void loadAllPlaylists();
    QString getPlaylistsDirPath();

    // 显示名转换（本地文件 → 纯文件名，服务器歌曲 → 🌐 + song name）
    QString filePathToDisplayName(const QString& filePath) const;
    // 根据显示名反查 filePath
    QString findPathByDisplayName(const Playlist* playlist, const QString& displayName) const;

    // 歌词
    void loadLyricsFromFile(const QString &lyricsFilePath);
    void updateLyricsDisplay(qint64 position);
    void clearLyricsDisplay();

    // 模式切换
    void setSourceMode(SourceMode mode);
    void updateModeUI();

    // 搜索
    void showSearchResults(const QList<RemoteSongInfo>& serverResults,
                          const QStringList& localResults);
    void hideSearchResults();

    // UI 初始化
    void initUI();
    void initApiClient();
    void connectSignals();

    // 自定义标题栏按钮
    QPushButton* minButton;
    QPushButton* maxButton;
    QPushButton* closeButton;

    // 窗口拖动
    QPoint dragPosition;
    bool isDragging;

    // 事件过滤器（用于菜单栏拖动）
    bool eventFilter(QObject* obj, QEvent* event) override;

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

    class FileSelectionDialog : public QDialog {
    public:
        FileSelectionDialog(const QStringList &files, QWidget *parent = nullptr);
        QStringList getSelectedFiles() const;
    private:
        QStandardItemModel *model;
    };
};

#endif // MAINWINDOW_H
