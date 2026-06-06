#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "Playlist.h"
#include "lyrics_parser.h"
// 移除未使用的 RTSP 播放器头文件引用
// #include "rtsp_player.h"
#include <QVBoxLayout>
#include <QDesktopServices>
#include <QMouseEvent>
#include <QHBoxLayout>

// 搜索浮层最大显示条数
static const int MAX_SEARCH_RESULTS = 20;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 设置无边框窗口（去掉标题栏）
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowMinMaxButtonsHint);
    
    // 允许最小化和最大化
    setWindowFlags(windowFlags() & ~Qt::WindowSystemMenuHint);

    // 创建自定义窗口控制按钮
    minButton = new QPushButton("─", this);
    maxButton = new QPushButton("□", this);
    closeButton = new QPushButton("✕", this);
    
    // 初始化拖动状态
    isDragging = false;
    
    // 为按钮设置对象名，让QSS能找到它们
    minButton->setObjectName("minButton");
    maxButton->setObjectName("maxButton");
    closeButton->setObjectName("closeButton");
    
    // 连接按钮信号
    connect(minButton, &QPushButton::clicked, this, &MainWindow::showMinimized);
    connect(maxButton, &QPushButton::clicked, this, [this]() {
        if (isMaximized()) {
            showNormal();
            maxButton->setText("□");
        } else {
            showMaximized();
            maxButton->setText("❐");
        }
    });
    connect(closeButton, &QPushButton::clicked, this, &MainWindow::close);

    // 创建按钮容器（用于setCornerWidget）
    QWidget* buttonContainer = new QWidget(this);
    QHBoxLayout* buttonLayout = new QHBoxLayout(buttonContainer);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(0);
    buttonLayout->addWidget(minButton);
    buttonLayout->addWidget(maxButton);
    buttonLayout->addWidget(closeButton);
    
    // 直接在菜单栏右边添加按钮容器（使用Qt的setCornerWidget）
    menuBar()->setCornerWidget(buttonContainer, Qt::TopRightCorner);
    
    // 为菜单栏安装事件过滤器，用于拖动窗口
    menuBar()->installEventFilter(this);

    // 加载配置
    ConfigManager::instance().load();

    // 初始化媒体播放器（必须在 initUI 之前，因为 initUI 中会用到 player）
    player = new QMediaPlayer(this);
    currentPlayMode = SinglePlay;
    ui->playModeButton->setText(tr("单曲播放"));
    isUpdatingProgress = false;

    // 初始化音量
    audioOutput = new QAudioOutput;
    player->setAudioOutput(audioOutput);
    audioOutput->setVolume(0.5f);

    // 初始化本地和服务器播放列表
    localPlaylist = new Playlist(tr("本地音乐"));
    serverPlaylist = new Playlist(tr("服务器音乐"));

    // 歌词定时器（仅用于同步显示，歌词由服务端生成）
    lyricsTimer = new QTimer(this);
    lyricsTimer->setInterval(100);

    // 初始化 UI（搜索栏、模式切换）
    initUI();

    // 移除未使用的 RTSP 播放器初始化
    // m_rtspPlayer = new RtspPlayer(this);
    // connect(m_rtspPlayer, &RtspPlayer::positionChanged, this, [this](qint64 pos) {
    //     isUpdatingProgress = true;
    //     qDebug() << "[MainWindow] RTSP position changed:" << pos << "ms";
    //     ui->progressBar->setValue(static_cast<int>(pos));
    //     ui->currentTimeLabel->setText(formatTime(pos));
    //     isUpdatingProgress = false;
    // });
    // connect(m_rtspPlayer, &RtspPlayer::durationChanged, this, [this](qint64 dur) {
    //     qDebug() << "[MainWindow] RTSP duration changed:" << dur << "ms";
    //     ui->progressBar->setRange(0, static_cast<int>(dur));
    //     ui->totalTimeLabel->setText(formatTime(dur));
    // });
    // connect(m_rtspPlayer, &RtspPlayer::stateChanged, this, [this](int state) {
    //     if (state == 1) {  // Playing
    //         ui->playButton->setText(tr("⏸ 暂停"));
    //         ui->progressBar->setEnabled(true);
    //         lyricsTimer->start();
    //     } else if (state == 2) {  // Paused
    //         ui->playButton->setText(tr("▶ 播放"));
    //         lyricsTimer->stop();
    //     } else {  // Stopped
    //         ui->playButton->setText(tr("▶ 播放"));
    //         ui->progressBar->setEnabled(false);
    //         lyricsTimer->stop();
    //         // 自动切下一首
    //         if (currentPlayMode == SingleLoopPlay && currentMode == SourceMode::Server) {
    //             playCurrentMedia();
    //         } else if (currentPlayMode != SinglePlay && currentMode == SourceMode::Server) {
    //             on_nextButton_clicked();
    //         }
    //     }
    // });
    // connect(m_rtspPlayer, &RtspPlayer::errorOccurred, this, [this](const QString& err) {
    //     QMessageBox::warning(this, tr("RTSP 播放错误"), err);
    // });

    // 初始化 API 客户端（自动连接服务器）
    initApiClient();

    // 加载播放列表
    loadAllPlaylists();

    // 信号槽连接
    connectSignals();

    // 设置进度条
    ui->progressBar->setValue(0);
    ui->progressBar->setEnabled(false);
    ui->progressBar->setTracking(true);

    connect(ui->playlistTabWidget, &QTabWidget::tabBarDoubleClicked,
            this, &MainWindow::onTabBarDoubleClicked);

    connect(ui->actionAbout, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, tr("关于音乐播放器"),
            tr("🎵 Music Player v2.0\n\n"
               "功能：\n"
               "• 本地 / 远程双模式播放\n"
               "• 服务器流式播放 (RTSP)\n"
               "• AI 歌词自动生成\n"
               "• 多播放列表管理\n"
               "• 深邃沉浸主题"));
    });

    // 启动时默认本地模式，等服务端心跳响应后再决定能否切换
    currentMode = SourceMode::Local;
    updateModeUI();
    statusBar()->showMessage(tr("就绪 | 本地模式 | 正在检测服务器..."));
}

MainWindow::~MainWindow()
{
    // 保存所有播放列表
    saveAllPlaylists();

    // 清理播放列表资源
    delete localPlaylist;
    delete serverPlaylist;
    qDeleteAll(playlists);
    delete player;
    delete ui;
}

void MainWindow::on_actionImport_Lyrics_triggered()
{
    // 打开文件选择对话框，让用户选择LRC歌词文件
    QString lyricsFilePath = QFileDialog::getOpenFileName(
        this,
        tr("选择歌词文件"),
        QDir::homePath(),
        tr("LRC歌词文件 (*.lrc);;所有文件 (*.*)")
    );
    
    // 如果用户选择了文件
    if (!lyricsFilePath.isEmpty()) {
        // 加载选中的歌词文件
        loadLyricsFromFile(lyricsFilePath);
        
        // 显示成功提示
        QMessageBox::information(this, tr("导入成功"), tr("歌词文件已成功导入"));
    }
}

// 获取播放列表保存目录路径
QString MainWindow::getPlaylistsDirPath()
{
    // 使用项目当前目录下的playlists文件夹存储播放列表
    QString appDir = QCoreApplication::applicationDirPath();
    QString playlistsDir = appDir + "/playlists";
    
    // 确保目录存在
    QDir dir(playlistsDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    return playlistsDir;
}

// 保存所有播放列表
void MainWindow::saveAllPlaylists()
{
    QString playlistsDir = getPlaylistsDirPath();

    // 保存本地播放列表
    if (localPlaylist) {
        QString localFile = playlistsDir + "/local_playlist.pls";
        localPlaylist->saveToFile(localFile);
    }

    // 保存历史播放列表（用于标签页）
    for (int i = 0; i < playlists.size(); i++) {
        Playlist *playlist = playlists[i];
        QString fileName = QString("%1/playlist_%2.pls").arg(playlistsDir).arg(i);
        playlist->saveToFile(fileName);
    }

    // 保存播放列表元数据（列表数量和名称）
    QString metadataFile = playlistsDir + "/metadata.json";
    QFile file(metadataFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QJsonDocument doc;
        QJsonObject root;
        QJsonArray playlistNames;

        foreach (Playlist *playlist, playlists) {
            playlistNames.append(playlist->name());
        }
        
        root["playlistCount"] = playlists.size();
        root["playlistNames"] = playlistNames;
        doc.setObject(root);
        
        file.write(doc.toJson());
        file.close();
    }
}

// 加载所有播放列表
void MainWindow::loadAllPlaylists()
{
    QString playlistsDir = getPlaylistsDirPath();

    // 加载本地播放列表
    QString localFile = playlistsDir + "/local_playlist.pls";
    if (QFile::exists(localFile) && localPlaylist) {
        localPlaylist->loadFromFile(localFile);
        qDebug() << "[MainWindow] Loaded local playlist with" << localPlaylist->mediaCount() << "items";
    }

    // 加载历史播放列表（用于标签页）
    QString metadataFile = playlistsDir + "/metadata.json";
    QFile file(metadataFile);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();

        QJsonObject root = doc.object();
        int playlistCount = root["playlistCount"].toInt();

        // 清除默认播放列表
        qDeleteAll(playlists);
        playlists.clear();

        // 加载每个播放列表
        for (int i = 0; i < playlistCount; i++) {
            QString fileName = QString("%1/playlist_%2.pls").arg(playlistsDir).arg(i);
            Playlist *playlist = new Playlist("播放列表");
            playlist->loadFromFile(fileName);
            playlists.append(playlist);
            QListWidget *newTab = new QListWidget();
            setupPlaylistListWidget(newTab);
            ui->playlistTabWidget->addTab(newTab, playlist->name());
        }
    }

    ui->playlistTabWidget->setCurrentIndex(0);
    updatePlaylistView();
}

// 播放/暂停控制
void MainWindow::on_playButton_clicked()
{
    if (currentMode == SourceMode::Server) {
        // 服务器模式：现在使用 QMediaPlayer 播放
        if (player->playbackState() == QMediaPlayer::PlayingState) {
            player->pause();
        } else if (player->playbackState() == QMediaPlayer::PausedState) {
            player->play();
        } else {  // Stopped
            playCurrentMedia();
        }
    } else {
        // 本地模式：控制 QMediaPlayer
        if (player->playbackState() == QMediaPlayer::PlayingState) {
            player->pause();
        } else {
            if (player->playbackState() == QMediaPlayer::StoppedState) {
                playCurrentMedia();
            } else {
                player->play();
            }
        }
    }
}

// 判断条目是否匹配当前模式
// 上一首
void MainWindow::on_prevButton_clicked()
{
    Playlist *currentPlaylist = getCurrentPlaylist();
    if (!currentPlaylist || currentPlaylist->mediaCount() == 0) return;

    int currentIndex = currentPlaylist->currentIndex();
    int count = currentPlaylist->mediaCount();

    if (currentPlayMode == RandomPlay) {
        // 随机选择一首
        int randomIndex = rand() % count;
        currentPlaylist->setCurrentIndex(randomIndex);
    } else {
        // 循环到上一首
        int prevIdx = (currentIndex - 1 + count) % count;
        currentPlaylist->setCurrentIndex(prevIdx);
    }

    // 更新播放列表视图中的选中项
    updatePlaylistSelection();
    playCurrentMedia();
}

// 下一首
void MainWindow::on_nextButton_clicked()
{
    Playlist *currentPlaylist = getCurrentPlaylist();
    if (!currentPlaylist || currentPlaylist->mediaCount() == 0) return;

    int currentIndex = currentPlaylist->currentIndex();
    int count = currentPlaylist->mediaCount();

    if (currentPlayMode == RandomPlay) {
        // 随机选择一首
        int randomIndex = rand() % count;
        currentPlaylist->setCurrentIndex(randomIndex);
    } else {
        // 循环到下一首
        int nextIdx = (currentIndex + 1) % count;
        currentPlaylist->setCurrentIndex(nextIdx);
    }

    // 更新播放列表视图中的选中项
    updatePlaylistSelection();
    playCurrentMedia();
}

// 切换播放模式
void MainWindow::on_playModeButton_clicked()
{
    currentPlayMode = static_cast<PlayMode>((currentPlayMode + 1) % 4);
    
    switch (currentPlayMode) {
    case SinglePlay:
        ui->playModeButton->setText(tr("单曲播放"));
        break;
    case SingleLoopPlay:
        ui->playModeButton->setText(tr("单曲循环"));
        break;
    case LoopPlay:
        ui->playModeButton->setText(tr("循环播放"));
        break;
    case RandomPlay:
        ui->playModeButton->setText(tr("随机播放"));
        break;
    }
}

// 按下进度条时的处理（暂停播放，防止位置刷新干扰拖动）
void MainWindow::on_progressBar_sliderPressed()
{
    // 记录当前播放状态（服务器和本地模式都使用 QMediaPlayer）
    wasPlayingBeforeSeek = (player->playbackState() == QMediaPlayer::PlayingState);
    
    // 暂停播放（服务器和本地模式都使用 QMediaPlayer）
    if (player->playbackState() == QMediaPlayer::PlayingState) {
        player->pause();
        qDebug() << "[MainWindow] Paused player for seek";
    }
}

// 释放进度条时的处理（用户完成拖动后跳转并恢复播放）
void MainWindow::on_progressBar_sliderReleased()
{
    int position = ui->progressBar->value();
    qDebug() << "[MainWindow] Slider released, seeking to:" << position << "ms";
    
    // 服务器和本地模式都使用 QMediaPlayer
    player->setPosition(position);
    if (wasPlayingBeforeSeek) {
        player->play();
        qDebug() << "[MainWindow] Resumed player after seek";
    }
}

// 拖动进度条（只更新显示，不实际跳转）
void MainWindow::on_progressBar_sliderMoved(int position)
{
    // 拖动时只更新时间显示，不调用 seek
    // 真正的跳转在 sliderReleased 时执行
    ui->currentTimeLabel->setText(formatTime(position));
}

// 调整音量
void MainWindow::on_volumeSlider_valueChanged(int value)
{
    qreal vol = value / 100.0;
    // 现在服务器和本地模式都使用 QMediaPlayer，统一处理
    audioOutput->setVolume(vol);
}

// 更新播放器状态
void MainWindow::updatePlayerStatus(QMediaPlayer::PlaybackState state)
{
    // 现在服务器和本地模式都使用 QMediaPlayer，统一处理
    if (state == QMediaPlayer::PlayingState) {
        ui->playButton->setText(tr("⏸ 暂停"));
        ui->progressBar->setEnabled(true);
    } else {
        ui->playButton->setText(tr("▶ 播放"));
    }

    if (state == QMediaPlayer::StoppedState) {
        ui->progressBar->setEnabled(false);
        if (currentPlayMode == SingleLoopPlay)
            playCurrentMedia();
        else if (currentPlayMode != SinglePlay)
            on_nextButton_clicked();
    }
}

// 更新进度
void MainWindow::updateProgress(qint64 position)
{
    // 设置标志，表示这是播放器更新进度条，不是用户交互
    isUpdatingProgress = true;
    ui->progressBar->setValue(position);
    ui->currentTimeLabel->setText(formatTime(position));
    isUpdatingProgress = false;
}

// 处理进度条值变化（仅用于点击跳转，不响应 setValue 更新）
void MainWindow::on_progressBar_valueChanged(int value)
{
    // 注意：setValue() 会触发此信号，但 isUpdatingProgress 标志只防止了部分情况
    // 真正的用户交互应该在 sliderReleased 中处理
    // 此方法保留用于检测用户直接点击进度条的情况
    Q_UNUSED(value)
    // 不在这里调用 seek，避免频繁跳转
}

// 更新总时长
void MainWindow::updateDuration(qint64 duration)
{
    ui->progressBar->setRange(0, duration);
    ui->totalTimeLabel->setText(formatTime(duration));
}

// 切换标签页时更新播放列表
void MainWindow::tab_currentChanged(int index)
{

    updatePlaylistView();
}

// 菜单项：添加文件 添加文件到播放列表
void MainWindow::on_actionAdd_File_triggered()
{
    // 服务器模式下禁止添加本地文件
    if (currentMode == SourceMode::Server) {
        QMessageBox::information(this, tr("提示"), tr("服务器模式下只能播放服务器音乐，无法添加本地文件"));
        return;
    }

    QStringList fileNames = QFileDialog::getOpenFileNames(this, tr("选择音频文件"), "",
                                                         tr("音频文件 (*.mp4 *.mp3 *.wav *.flac *.ogg);;所有文件 (*)"));
    if (!fileNames.isEmpty()) {
        addFilesToPlaylist(fileNames);
    }
}

// 菜单项：添加文件夹 添加文件夹递归扫描
void MainWindow::on_actionAdd_Folder_triggered()
{
    // 服务器模式下禁止添加本地文件
    if (currentMode == SourceMode::Server) {
        QMessageBox::information(this, tr("提示"), tr("服务器模式下只能播放服务器音乐，无法添加本地文件夹"));
        return;
    }

    QString folderPath = QFileDialog::getExistingDirectory(this, tr("选择文件夹"), "");
    if (!folderPath.isEmpty()) {
        scanFolderForFiles(folderPath);
    }
}

// 删除文件菜单操作
void MainWindow::on_actionDelete_File_triggered()
{
    Playlist *currentPlaylist = getCurrentPlaylist();
    if (!currentPlaylist) return;

    auto currentWidget = qobject_cast<QListWidget*>(ui->playlistTabWidget->currentWidget());
    if (!currentWidget) return;

    QModelIndexList selectedIndexes = currentWidget->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) return;

    // 从大到小删除，避免索引变化问题
    std::sort(selectedIndexes.begin(), selectedIndexes.end(), [](const QModelIndex &a, const QModelIndex &b) {
        return a.row() > b.row();
    });

    // 收集要删除的数据索引（通过 UserRole）
    QList<int> dataIndices;
    for (const QModelIndex &index : selectedIndexes) {
        QListWidgetItem* item = currentWidget->item(index.row());
        if (item) {
            dataIndices.append(item->data(Qt::UserRole).toInt());
        }
    }

    // 从大到小排序，避免删除时索引偏移
    std::sort(dataIndices.begin(), dataIndices.end(), std::greater<int>());

    // 从数据模型中删除
    for (int dataIdx : dataIndices) {
        currentPlaylist->removeMedia(dataIdx);
    }

    // 刷新视图（因为索引变了）
    saveAllPlaylists();
    updatePlaylistView();

    // 保存更新后的播放列表
    saveAllPlaylists();
}

// 新建播放列表菜单操作
void MainWindow::on_actionNew_Playlist_triggered()
{
    // 创建对话框获取新播放列表名称
    QDialog dialog(this);
    dialog.setWindowTitle(tr("新建播放列表"));

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLabel *label = new QLabel(tr("请输入播放列表名称:"), &dialog);
    QLineEdit *lineEdit = new QLineEdit(&dialog);
    lineEdit->setText(tr("播放列表 %1").arg(playlists.size() + 1));

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *okButton = new QPushButton(tr("确定"), &dialog);
    QPushButton *cancelButton = new QPushButton(tr("取消"), &dialog);

    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);

    layout->addWidget(label);
    layout->addWidget(lineEdit);
    layout->addLayout(buttonLayout);

    connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QString playlistName = lineEdit->text();
        if (playlistName.isEmpty()) {
            playlistName = tr("未命名播放列表");
        }

        // 创建新播放列表
        Playlist *newPlaylist = new Playlist(playlistName);
        playlists.append(newPlaylist);

        // 添加新标签页
        QListWidget *newTab = new QListWidget();
        setupPlaylistListWidget(newTab);
        ui->playlistTabWidget->addTab(newTab, playlistName);
    }
}

// 删除播放列表菜单操作
void MainWindow::on_actionDelete_Playlist_triggered()
{
    int currentIndex = ui->playlistTabWidget->currentIndex();
    if (currentIndex <= 0 || playlists.size() <= 1) {
        QMessageBox::warning(this, tr("提示"), tr("不能删除默认播放列表或没有可删除的播放列表"));
        return;
    }

    if (QMessageBox::question(this, tr("确认删除"), tr("确定要删除当前播放列表吗?")) == QMessageBox::Yes) {
        delete playlists.takeAt(currentIndex);
        ui->playlistTabWidget->removeTab(currentIndex);
        updatePlaylistView();
    }
}

// 退出应用程序
//void MainWindow::on_actionExit_triggered()
//{
//    QApplication::quit();
//}

// 双击标签栏修改播放列表名称
void MainWindow::onTabBarDoubleClicked(int index)
{
    if (index < 0 || index >= playlists.size()) return;
    
    // 获取当前播放列表
    Playlist *playlist = playlists[index];
    QString currentName = playlist->name();
    
    // 创建对话框获取新名称
    bool ok;
    QString newName = QInputDialog::getText(this, tr("修改播放列表名称"),
                                          tr("请输入新的播放列表名称："),
                                          QLineEdit::Normal,
                                          currentName, &ok);
    
    // 如果用户确认且名称不为空
    if (ok && !newName.trimmed().isEmpty()) {
        // 更新播放列表名称
        playlist->setName(newName.trimmed());
        
        // 更新标签页标题
        ui->playlistTabWidget->setTabText(index, newName.trimmed());
        
        // 保存更新后的播放列表信息
        saveAllPlaylists();
    }
}

// 辅助方法：设置播放列表列表控件的通用属性
void MainWindow::setupPlaylistListWidget(QListWidget* widget)
{
    static int setupCount = 0;
    setupCount++;
    qDebug() << "[MainWindow] setupPlaylistListWidget called, count:" << setupCount << ", widget:" << (void*)widget;
    
    widget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    widget->setDragDropMode(QAbstractItemView::InternalMove);
    widget->setDefaultDropAction(Qt::MoveAction);
    widget->setDragEnabled(true);
    widget->setAcceptDrops(true);
    widget->setContextMenuPolicy(Qt::CustomContextMenu);

    // 连接双击播放信号（使用 Qt::UserRole 中的真实索引）
    connect(widget, &QListWidget::doubleClicked, [this, widget](const QModelIndex& index) {
        static int doubleClickCount = 0;
        doubleClickCount++;
        qDebug() << "[MainWindow] Double-click triggered, count:" << doubleClickCount 
                 << ", widget:" << (void*)widget << ", row:" << index.row();
        
        if (!index.isValid()) return;

        QListWidgetItem* item = widget->item(index.row());
        if (!item) return;

        bool ok = false;
        int realIndex = item->data(Qt::UserRole).toInt(&ok);
        if (!ok) {
            qDebug() << "[MainWindow] Failed to get real index from item";
            return;
        }

        Playlist* currentPlaylist = getCurrentPlaylist();
        if (currentPlaylist) {
            currentPlaylist->setCurrentIndex(realIndex);
            qDebug() << "[MainWindow] Playing item, UI index:" << index.row() << "Real index:" << realIndex;
            playCurrentMedia();
        }
    });

    // 连接右键菜单信号
    connect(widget, &QListWidget::customContextMenuRequested, this, &MainWindow::showPlaylistContextMenu);

    // 拖动排序定时器
    QTimer* dragTimer = new QTimer(widget);
    dragTimer->setSingleShot(true);
    dragTimer->setInterval(100);

    connect(widget->model(), &QAbstractItemModel::rowsMoved, [dragTimer, widget, this]() {
        dragTimer->start();
    });

    connect(dragTimer, &QTimer::timeout, [this, widget]() {
        Playlist* pl = getCurrentPlaylist();
        if (pl) {
            QStringList newOrder;
            for (int i = 0; i < widget->count(); ++i) {
                QListWidgetItem* it = widget->item(i);
                if (it) {
                    QString matched = findPathByDisplayName(pl, it->text());
                    if (!matched.isEmpty()) newOrder.append(matched);
                }
            }
            if (newOrder.size() == pl->mediaCount()) {
                pl->filePathList() = newOrder;
                saveAllPlaylists();
                // 重新更新视图以确保 UserRole 正确
                updatePlaylistView();
            }
        }
    });
}

// 显示播放列表右键菜单
void MainWindow::showPlaylistContextMenu(const QPoint& pos)
{
    auto widget = qobject_cast<QListWidget*>(sender());
    if (!widget) return;

    QMenu menu(this);

    // 播放选项
    QAction* playAction = menu.addAction(tr("播放"));
    playAction->setIcon(QIcon::fromTheme("media-playback-start"));

    menu.addSeparator();

    // 删除选项
    QAction* removeAction = menu.addAction(tr("从列表删除"));
    removeAction->setIcon(QIcon::fromTheme("edit-delete"));

    menu.addSeparator();

    // 播放列表重命名
    QAction* renamePlaylistAction = menu.addAction(tr("重命名播放列表"));
    renamePlaylistAction->setIcon(QIcon::fromTheme("document-edit"));

    QAction* selected = menu.exec(widget->mapToGlobal(pos));
    if (!selected) return;

    QListWidgetItem* item = widget->itemAt(pos);

    if (selected == playAction && item) {
        // 播放该项
        bool ok = false;
        int realIndex = item->data(Qt::UserRole).toInt(&ok);
        if (ok) {
            Playlist* currentPlaylist = getCurrentPlaylist();
            if (currentPlaylist) {
                currentPlaylist->setCurrentIndex(realIndex);
                playCurrentMedia();
            }
        }
    } else if (selected == removeAction) {
        // 删除选中的项
        QList<QListWidgetItem*> selectedItems = widget->selectedItems();
        if (selectedItems.isEmpty() && item) {
            selectedItems.append(item);
        }

        if (!selectedItems.isEmpty()) {
            Playlist* currentPlaylist = getCurrentPlaylist();
            if (currentPlaylist) {
                // 从后往前删除以避免索引问题
                QList<int> indices;
                for (QListWidgetItem* it : selectedItems) {
                    bool ok = false;
                    int idx = it->data(Qt::UserRole).toInt(&ok);
                    if (ok) indices.append(idx);
                }

                std::sort(indices.begin(), indices.end(), std::greater<int>());
                for (int idx : indices) {
                    currentPlaylist->removeMedia(idx);
                }

                updatePlaylistView();
                saveAllPlaylists();
            }
        }
    } else if (selected == renamePlaylistAction) {
        // 重命名播放列表
        onTabBarDoubleClicked(ui->playlistTabWidget->currentIndex());
    }
}

// 重命名当前播放列表
void MainWindow::renameCurrentPlaylist()
{
    onTabBarDoubleClicked(ui->playlistTabWidget->currentIndex());
}

// 辅助方法：添加文件到播放列表
void MainWindow::addFilesToPlaylist(const QStringList &fileNames)
{
    Playlist *currentPlaylist = getCurrentPlaylist();
    
    // 如果没有播放列表，自动创建一个默认播放列表
    if (!currentPlaylist) {
        QString defaultName = tr("播放列表 1");
        
        // 检查是否已存在同名播放列表，如果存在则自动递增编号
        int counter = 1;
        while (true) {
            bool exists = false;
            foreach (Playlist *p, playlists) {
                if (p->name() == defaultName) {
                    exists = true;
                    break;
                }
            }
            if (!exists) break;
            counter++;
            defaultName = tr("播放列表 %1").arg(counter);
        }
        
        // 创建新播放列表
        Playlist *newPlaylist = new Playlist(defaultName);
        playlists.append(newPlaylist);

        // 添加新标签页
        QListWidget *newTab = new QListWidget();
        setupPlaylistListWidget(newTab);
        ui->playlistTabWidget->addTab(newTab, defaultName);
        
        currentPlaylist = newPlaylist;
        currentPlaylist->setCurrentIndex(0);
        
        QMessageBox::information(this, tr("提示"), 
                                tr("已创建新播放列表 \"%1\"").arg(defaultName));
    }
    
    auto currentWidget = qobject_cast<QListWidget*>(ui->playlistTabWidget->currentWidget());
    if (!currentWidget) return;
    
    int addedCount = 0;  // 记录新增的文件数量
    int duplicateCount = 0;  // 记录重复的文件数量
    
    foreach (const QString &fileName, fileNames) {
        // 检查文件是否已存在于播放列表中
        bool isDuplicate = false;
        foreach (const QString &existingFile, currentPlaylist->filePathList()) {
            // 使用QFileInfo获取规范路径进行比较，避免因路径格式不同导致的重复
            if (QFileInfo(fileName).canonicalFilePath() == QFileInfo(existingFile).canonicalFilePath()) {
                isDuplicate = true;
                duplicateCount++;
                break;
            }
        }
        
        // 如果不是重复文件，则添加到播放列表
        if (!isDuplicate) {
            currentPlaylist->addMedia(fileName);
            // 直接添加到UI列表末尾，而不是重新加载整个列表
            QString displayName = filePathToDisplayName(fileName);
            QListWidgetItem* item = new QListWidgetItem(displayName);
            item->setData(Qt::UserRole, currentPlaylist->mediaCount() - 1); // 设置真实索引
            currentWidget->addItem(item);
            addedCount++;
        }
    }
    
    // 只有在需要高亮当前播放项时才调用updatePlaylistView的相关部分
    int currentIndex = currentPlaylist->currentIndex();
    if (currentIndex >= 0 && currentIndex < currentWidget->count()) {
        currentWidget->setCurrentRow(currentIndex);
    }
    
    // 显示添加结果提示
    QString message;
    if (addedCount > 0 && duplicateCount > 0) {
        message = tr("成功添加 %1 个文件，跳过 %2 个重复文件").arg(addedCount).arg(duplicateCount);
        QMessageBox::information(this, tr("添加完成"), message);
    } else if (addedCount > 0) {
        message = tr("成功添加 %1 个文件").arg(addedCount);
        QMessageBox::information(this, tr("添加完成"), message);
    } else if (duplicateCount > 0) {
        message = tr("所有文件均已存在于播放列表中，未添加新文件");
        QMessageBox::information(this, tr("提示"), message);
    }
}

// 辅助方法：扫描文件夹获取文件
void MainWindow::scanFolderForFiles(const QString &folderPath)
{
    QStringList files;
    
    // 递归扫描文件夹
    QDirIterator it(folderPath, {"*.mp4", "*.mp3", "*.wav", "*.flac", "*.ogg"}, 
                   QDir::Files, QDirIterator::Subdirectories);
    
    while (it.hasNext()) {
        files.append(it.next());
    }
    
    if (files.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("未找到音频文件"));
        return;
    }
    
    // 显示选择对话框
    FileSelectionDialog dialog(files, this);
    if (dialog.exec() == QDialog::Accepted) {
        QStringList selectedFiles = dialog.getSelectedFiles();
        addFilesToPlaylist(selectedFiles);
    }
}

// 辅助方法：更新播放列表视图（按当前模式过滤）
void MainWindow::updatePlaylistView()
{
    Playlist *currentPlaylist = getCurrentPlaylist();
    if (!currentPlaylist) {
        qDebug() << "[MainWindow] updatePlaylistView: currentPlaylist is null";
        return;
    }

    qDebug() << "[MainWindow] updatePlaylistView: currentMode=" 
             << (currentMode == SourceMode::Local ? "Local" : "Server")
             << ", mediaCount=" << currentPlaylist->mediaCount();

    // 始终更新第一个标签页（主播放列表）
    auto currentWidget = qobject_cast<QListWidget*>(ui->playlistTabWidget->widget(0));
    if (!currentWidget) {
        qDebug() << "[MainWindow] updatePlaylistView: currentWidget is null";
        return;
    }

    // 清除当前标签页的播放列表
    currentWidget->clear();

    // 将当前播放列表的所有文件添加到视图（不再需要模式过滤）
    int highlightRow = -1;
    int dataIdx = currentPlaylist->currentIndex();

    for (int i = 0; i < currentPlaylist->mediaCount(); ++i) {
        QString filePath = currentPlaylist->filePath(i);
        QString displayName = filePathToDisplayName(filePath);
        QListWidgetItem* item = new QListWidgetItem(displayName);
        item->setData(Qt::UserRole, i);  // 保存数据层真实索引
        currentWidget->addItem(item);

        if (i == dataIdx) {
            highlightRow = currentWidget->count() - 1;
        }
    }

    // 高亮显示当前播放的项
    if (highlightRow >= 0) {
        currentWidget->setCurrentRow(highlightRow);
    }
}

// 辅助方法：更新播放列表选中项（不重新加载整个列表）
void MainWindow::updatePlaylistSelection()
{
    Playlist *currentPlaylist = getCurrentPlaylist();
    if (!currentPlaylist) return;

    auto currentWidget = qobject_cast<QListWidget*>(ui->playlistTabWidget->widget(0));
    if (!currentWidget) return;

    int currentIndex = currentPlaylist->currentIndex();
    if (currentIndex >= 0 && currentIndex < currentWidget->count()) {
        currentWidget->setCurrentRow(currentIndex);
    }
}

// 辅助方法：格式化时间
QString MainWindow::formatTime(qint64 ms)
{
    int seconds = ms / 1000;
    int minutes = seconds / 60;
    seconds %= 60;
    
    return QString("%1:%2").arg(minutes, 2, 10, QLatin1Char('0'))
                           .arg(seconds, 2, 10, QLatin1Char('0'));
}

// 辅助方法：获取当前播放列表索引
int MainWindow::getCurrentPlaylistIndex()
{
    return ui->playlistTabWidget->currentIndex();
}

// 辅助方法：获取当前播放列表（根据模式返回对应的播放列表）
Playlist* MainWindow::getCurrentPlaylist()
{
    if (currentMode == SourceMode::Local) {
        return localPlaylist;
    } else {
        return serverPlaylist;
    }
}

// 辅助方法：播放当前媒体
void MainWindow::playCurrentMedia()
{
    Playlist *currentPlaylist = getCurrentPlaylist();
    if (!currentPlaylist || currentPlaylist->mediaCount() == 0) {
        QMessageBox::warning(this, tr("提示"), tr("播放列表为空"));
        return;
    }

    int currentIndex = currentPlaylist->currentIndex();
    if (currentIndex < 0) {
        // 如果没有当前索引，从第一首开始
        currentIndex = 0;
        currentPlaylist->setCurrentIndex(currentIndex);
    }

    // 设置当前媒体并播放
    QString filePath = currentPlaylist->filePath(currentIndex);
    if (filePath.isEmpty()) return;

    // 检测服务器歌曲（server://id|name|artist 格式）
    if (filePath.startsWith("server://")) {
        QString inner = filePath.mid(9);  // 去掉 "server://"
        QStringList parts = inner.split('|');
        if (parts.size() >= 1) {
            QString songId = parts[0];
            QString songName = parts.size() >= 2 ? parts[1] : songId;
            QString artist   = parts.size() >= 3 ? parts[2] : QString();

            ui->currentSongLabel->setText(songName);
            ui->currentArtistLabel->setText(artist.isEmpty() ? tr("远程服务器") : artist);
            ui->coverLabel->setStyleSheet(
                "font-size: 48px; background-color: #13102A;"
                "border: 2px solid #7C6AEE; border-radius: 12px;"
            );
            ui->coverLabel->setText("🌐");

            // 请求服务器推流
            ApiClient::instance()->requestPlay(songId);
            return;
        }
    }

    // 本地文件播放
    player->setSource(QUrl::fromLocalFile(filePath));
    player->play();

    // 更新歌曲信息
    QFileInfo fileInfo(filePath);
    ui->currentSongLabel->setText(fileInfo.completeBaseName());
    ui->currentArtistLabel->setText("");

    // 更新封面区（显示默认图标）
    ui->coverLabel->setStyleSheet(
        "font-size: 64px; background-color: #13102A;"
        "border: 2px solid #2A2555; border-radius: 12px;"
    );
    ui->coverLabel->setText("🎵");

    // 尝试自动加载同名歌词文件
    QString lyricsFilePath = filePath;
    lyricsFilePath.replace(QRegularExpression("\\.[^.]*$"), ".lrc");
    if (QFile::exists(lyricsFilePath)) {
        loadLyricsFromFile(lyricsFilePath);
    } else {
        // 也可以尝试在歌词目录中查找
        QString musicDir = QFileInfo(filePath).absolutePath();
        QString fileNameWithoutExt = QFileInfo(filePath).baseName();
        QString altLyricsPath = musicDir + "/" + fileNameWithoutExt + ".lrc";
        if (QFile::exists(altLyricsPath)) {
            loadLyricsFromFile(altLyricsPath);
        } else {
            clearLyricsDisplay();
        }
    }
}

// 处理媒体播放器错误
// 从文件加载歌词
void MainWindow::loadLyricsFromFile(const QString &lyricsFilePath)
{
    currentLyrics.clear();
    currentLyricsFilePath = lyricsFilePath;
    prevLyricHighlight = -1;

    if (lyricsParser.loadFromFile(lyricsFilePath)) {
        // 从 parser 中获取歌词（loadFromFile 已解析）
        // 重新解析 LRC 格式
        // 直接用 parseLRC 解析文件内容
        QFile file(lyricsFilePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = file.readAll();
            file.close();
            if (lyricsParser.parseLRC(content)) {
                currentLyrics = lyricsParser.lyrics();
            }
        }

        // 用主题 HTML 格式显示
        QString html = "<html><head><style>"
                       "body { background: transparent; font-family: 'Microsoft YaHei', sans-serif; }"
                       "p { padding: 6px 0; margin: 0; text-align: center; }"
                       ".future { color: #8E88B0; font-size: 13pt; }"
                       "</style></head><body>";

        for (const auto& line : currentLyrics) {
            QString text = line.text.toHtmlEscaped();
            if (text.isEmpty()) text = "♬";
            html += "<p class=\"future\">" + text + "</p>";
        }
        html += "</body></html>";

        ui->lyricsTextBrowser->setHtml(html);
    } else {
        ui->lyricsTextBrowser->setHtml(
            "<p align='center' style='color:#8E88B0; font-size:14pt; margin-top:40px;'>"
            "📄 无法解析歌词文件</p>");
    }
}

// 更新歌词显示
void MainWindow::updateLyricsDisplay(qint64 position)
{
    if (currentLyrics.empty()) {
        return;
    }

    // 查找当前播放进度对应的歌词行
    int currentLineIndex = -1;
    for (size_t i = 0; i < currentLyrics.size(); ++i) {
        if (position >= currentLyrics[i].startTime * 1000 &&
            (i == currentLyrics.size() - 1 || position < currentLyrics[i + 1].startTime * 1000)) {
            currentLineIndex = static_cast<int>(i);
            break;
        }
    }

    if (currentLineIndex != -1) {
        // 检查是否需要更新（只在歌词行改变时更新）
        if (prevLyricHighlight == currentLineIndex) {
            return;  // 当前行没变，不需要更新
        }
        prevLyricHighlight = currentLineIndex;

        // 用 HTML 重建歌词显示（使用锚点定位当前行）
        QString html = "<html><head><style>"
                       "body { background: transparent; font-family: 'Microsoft YaHei', sans-serif; }"
                       "p { padding: 6px 0; margin: 0; text-align: center; }"
                       ".past { color: #605A80; font-size: 12pt; }"
                       ".current { color: #7C6AEE; font-size: 16pt; font-weight: bold; }"
                       ".future { color: #8E88B0; font-size: 13pt; }"
                       "</style></head><body>";

        for (size_t i = 0; i < currentLyrics.size(); ++i) {
            QString cls = (i < currentLineIndex) ? "past" :
                          (i == currentLineIndex) ? "current" : "future";
            QString text = currentLyrics[i].text.toHtmlEscaped();
            if (text.isEmpty()) text = "♬";
            // 当前行使用锚点
            if (i == currentLineIndex) {
                html += "<p id='current' class=\"" + cls + "\">" + text + "</p>";
            } else {
                html += "<p class=\"" + cls + "\">" + text + "</p>";
            }
        }
        html += "</body></html>";

        ui->lyricsTextBrowser->setHtml(html);
        
        // 滚动到当前行（使用锚点）
        QTextCursor cursor = ui->lyricsTextBrowser->textCursor();
        cursor.movePosition(QTextCursor::Start);
        for (int i = 0; i < currentLineIndex; i++) {
            cursor.movePosition(QTextCursor::NextBlock);
        }
        // 将当前行居中显示
        cursor.select(QTextCursor::BlockUnderCursor);
        ui->lyricsTextBrowser->setTextCursor(cursor);
        ui->lyricsTextBrowser->ensureCursorVisible();
    }
}

// 清除歌词显示
void MainWindow::clearLyricsDisplay()
{
    currentLyrics.clear();
    currentLyricsFilePath.clear();
    prevLyricHighlight = -1;
    ui->lyricsTextBrowser->setHtml(
        "<p align='center' style='color:#8E88B0; font-size:14pt; margin-top:40px;'>"
        "🎶 歌词将在播放时显示</p>");
}

// 根据播放进度更新歌词
void MainWindow::updateLyricsWithPosition(qint64 position)
{
    updateLyricsDisplay(position);
}

void MainWindow::on_lyricsTimer_timeout()
{
    // 定期更新歌词显示
    updateLyricsDisplay(player->position());
}

void MainWindow::handlePlayerError(QMediaPlayer::Error error)
{
    if (error != QMediaPlayer::NoError) {
        qDebug() << "Player error occurred:" << error << "Error string:" << player->errorString();

        Playlist *currentPlaylist = getCurrentPlaylist();
        if (!currentPlaylist) return;

        int currentIndex = currentPlaylist->currentIndex();
        if (currentIndex >= 0 && currentIndex < currentPlaylist->mediaCount()) {
            QString failedFilePath = currentPlaylist->filePath(currentIndex);
            QString failedFileName = QFileInfo(failedFilePath).fileName();

            if (error == QMediaPlayer::ResourceError || error == QMediaPlayer::FormatError) {
                if (!QFile::exists(failedFilePath)) {
                    qDebug() << "文件不存在，删除:" << failedFileName;
                    QMessageBox::StandardButton reply = QMessageBox::question(
                        this,
                        tr("播放失败"),
                        tr("无法播放文件: %1\n文件不存在或路径无效。\n是否从播放列表中移除?").arg(failedFileName),
                        QMessageBox::Yes | QMessageBox::No,
                        QMessageBox::No
                    );

                    if (reply == QMessageBox::Yes) {
                        int nextIndex = (currentIndex + 1) % currentPlaylist->mediaCount();
                        if (nextIndex == currentIndex) nextIndex = -1;

                        currentPlaylist->removeMedia(currentIndex);
                        updatePlaylistView();

                        saveAllPlaylists();

                        if (currentPlaylist->mediaCount() > 0) {
                            currentPlaylist->setCurrentIndex(nextIndex >= 0 ? nextIndex : 0);
                            playCurrentMedia();
                        }
                    }
                } else {
                    qDebug() << "文件存在但播放失败 (可能是设备问题):" << failedFileName;
                    qDebug() << "Error string:" << player->errorString();

                    QMessageBox::StandardButton reply = QMessageBox::question(
                        this,
                        tr("播放失败"),
                        tr("无法播放文件: %1\n\n错误信息: %2\n\n可能的原因:\n- 音频设备被占用\n- 文件格式不支持\n- 文件已损坏\n\n是否从播放列表中移除?").arg(failedFileName).arg(player->errorString()),
                        QMessageBox::Yes | QMessageBox::No,
                        QMessageBox::No
                    );

                    if (reply == QMessageBox::Yes) {
                        int nextIndex = (currentIndex + 1) % currentPlaylist->mediaCount();
                        if (nextIndex == currentIndex) nextIndex = -1;

                        currentPlaylist->removeMedia(currentIndex);
                        updatePlaylistView();

                        saveAllPlaylists();

                        if (currentPlaylist->mediaCount() > 0) {
                            currentPlaylist->setCurrentIndex(nextIndex >= 0 ? nextIndex : 0);
                            playCurrentMedia();
                        }
                    } else {
                        if (currentPlaylist->mediaCount() > 1) {
                            int nextIndex = (currentIndex + 1) % currentPlaylist->mediaCount();
                            currentPlaylist->setCurrentIndex(nextIndex);
                            playCurrentMedia();
                        }
                    }
                }
            }
        }
    }
}

// FileSelectionDialog 实现
MainWindow::FileSelectionDialog::FileSelectionDialog(const QStringList &files, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("选择要添加的文件"));
    resize(600, 400);
    
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    // 创建列表视图
    QListView *listView = new QListView(this);
    listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    
    // 创建模型
    model = new QStandardItemModel(this);
    
    // 添加所有文件到模型
    foreach (const QString &file, files) {
        QStandardItem *item = new QStandardItem(QFileInfo(file).fileName());
        item->setData(file, Qt::UserRole);
        item->setCheckable(true);
        item->setCheckState(Qt::Checked);
        model->appendRow(item);
    }
    
    listView->setModel(model);
    
    // 创建按钮布局
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *selectAllButton = new QPushButton(tr("全选"), this);
    QPushButton *selectNoneButton = new QPushButton(tr("全不选"), this);
    QPushButton *okButton = new QPushButton(tr("确定"), this);
    QPushButton *cancelButton = new QPushButton(tr("取消"), this);
    
    buttonLayout->addWidget(selectAllButton);
    buttonLayout->addWidget(selectNoneButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    
    layout->addWidget(listView);
    layout->addLayout(buttonLayout);
    
    // 连接信号
    connect(selectAllButton, &QPushButton::clicked, [this]() {
        for (int i = 0; i < model->rowCount(); i++) {
            model->item(i)->setCheckState(Qt::Checked);
        }
    });
    
    connect(selectNoneButton, &QPushButton::clicked, [this]() {
        for (int i = 0; i < model->rowCount(); i++) {
            model->item(i)->setCheckState(Qt::Unchecked);
        }
    });
    
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

// 获取选中的文件
QStringList MainWindow::FileSelectionDialog::getSelectedFiles() const
{
    QStringList selectedFiles;

    for (int i = 0; i < model->rowCount(); i++) {
        if (model->item(i)->checkState() == Qt::Checked) {
            selectedFiles.append(model->item(i)->data(Qt::UserRole).toString());
        }
    }

    return selectedFiles;
}

// ====================================================================
// Phase 3: UI 初始化、ApiClient、双模式、搜索
// ====================================================================

void MainWindow::initUI()
{
    // 模式切换按钮（从 UI 文件获取）
    // 注意：on_modeSwitchButton_clicked 会被 Qt 自动连接，不需要手动 connect
    ui->modeSwitchButton->setText(tr("🌐 服务器模式"));

    // 搜索框（从 UI 文件获取）
    // 注意：on_searchLineEdit_textChanged 和 on_searchLineEdit_returnPressed 会被 Qt 自动连接
    searchLineEdit = ui->searchLineEdit;

    // 搜索结果浮层（仍需动态创建，因为是弹出窗口）
    searchResultList = new QListWidget(this);
    searchResultList->setObjectName("searchResultList");
    searchResultList->setMaximumHeight(300);
    searchResultList->setVisible(false);
    searchResultList->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
    connect(searchResultList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        QString songId = item->data(Qt::UserRole).toString();
        QString songSource = item->data(Qt::UserRole + 1).toString();

        // 确保有当前播放列表，没有则自动创建
        Playlist* currentPlaylist = getCurrentPlaylist();
        if (!currentPlaylist) {
            // 自动创建默认播放列表
            QString defaultName = tr("播放列表 1");
            int counter = 1;
            while (true) {
                bool exists = false;
                for (Playlist* p : playlists) {
                    if (p->name() == defaultName) { exists = true; break; }
                }
                if (!exists) break;
                counter++;
                defaultName = tr("播放列表 %1").arg(counter);
            }
            currentPlaylist = new Playlist(defaultName);
            playlists.append(currentPlaylist);

            QListWidget* newTab = new QListWidget();
            setupPlaylistListWidget(newTab);
            ui->playlistTabWidget->addTab(newTab, defaultName);
            currentPlaylist->setCurrentIndex(0);
        }

        if (songSource == "server") {
            // 服务器歌曲：用 server:// 前缀存入播放列表
            QString displayName = item->text().remove(QRegularExpression("^[🌐🎤⏳💻]\\s*"));
            QString artist = tr("服务器");
            QString entry = QString("server://%1|%2|%3").arg(songId, displayName, artist);
            currentPlaylist->addMedia(entry);

            // 更新UI列表
            auto currentWidget = qobject_cast<QListWidget*>(ui->playlistTabWidget->currentWidget());
            if (currentWidget) {
                QString displayText = QString("🌐  %1").arg(displayName);
                QListWidgetItem* item = new QListWidgetItem(displayText);
                item->setData(Qt::UserRole, currentPlaylist->mediaCount() - 1); // 设置真实索引
                currentWidget->addItem(item);
            }

            // 请求服务器推流
            ApiClient::instance()->requestPlay(songId);
        } else {
            // 本地歌曲
            QString filePath = item->data(Qt::UserRole + 2).toString();
            currentPlaylist->addMedia(filePath);
            auto currentWidget = qobject_cast<QListWidget*>(ui->playlistTabWidget->currentWidget());
            if (currentWidget) {
                QString displayName = filePathToDisplayName(filePath);
                QListWidgetItem* playlistItem = new QListWidgetItem(displayName);
                playlistItem->setData(Qt::UserRole, currentPlaylist->mediaCount() - 1); // 设置真实索引
                currentWidget->addItem(playlistItem);
            }
            player->setSource(QUrl::fromLocalFile(filePath));
            player->play();
        }

        saveAllPlaylists();
        hideSearchResults();
    });

    // 状态栏
    statusBar()->showMessage(tr("就绪 | 本地模式"));
}

void MainWindow::initApiClient()
{
    auto& config = ConfigManager::instance();
    ApiClient::instance()->init(config.server.httpBaseUrl());

    // 连接信号
    connect(ApiClient::instance(), &ApiClient::connected, this, &MainWindow::onServerConnected);
    connect(ApiClient::instance(), &ApiClient::disconnected, this, &MainWindow::onServerDisconnected);
    connect(ApiClient::instance(), &ApiClient::serverFilesReceived, this, &MainWindow::onServerFilesReceived);
    connect(ApiClient::instance(), &ApiClient::searchResultReceived, this, &MainWindow::onSearchResultReceived);
    connect(ApiClient::instance(), &ApiClient::streamUrlReady, this, &MainWindow::onStreamUrlReady);
    connect(ApiClient::instance(), &ApiClient::uploadFinished, this, &MainWindow::onUploadFinished);
    connect(ApiClient::instance(), &ApiClient::lyricsReady, this, &MainWindow::onLyricsReady);
    connect(ApiClient::instance(), &ApiClient::lyricsStatus, this, &MainWindow::onLyricsStatus);
    connect(ApiClient::instance(), &ApiClient::coverReady, this, &MainWindow::onCoverReady);
    connect(ApiClient::instance(), &ApiClient::errorOccurred, this, &MainWindow::onApiErrorOccurred);
}

void MainWindow::connectSignals()
{
    connect(player, &QMediaPlayer::playbackStateChanged, this, &MainWindow::updatePlayerStatus);
    connect(player, &QMediaPlayer::positionChanged, this, &MainWindow::updateProgress);
    connect(player, &QMediaPlayer::durationChanged, this, &MainWindow::updateDuration);
    connect(player, &QMediaPlayer::errorOccurred, this, &MainWindow::handlePlayerError);
    connect(ui->playlistTabWidget, &QTabWidget::currentChanged, this, &MainWindow::tab_currentChanged);

    // 歌词
    connect(player, &QMediaPlayer::positionChanged, this, &MainWindow::updateLyricsWithPosition);
    connect(lyricsTimer, &QTimer::timeout, this, &MainWindow::on_lyricsTimer_timeout);
    connect(player, &QMediaPlayer::playbackStateChanged, [this](QMediaPlayer::PlaybackState state) {
        if (state == QMediaPlayer::PlayingState) {
            lyricsTimer->start();
        } else {
            lyricsTimer->stop();
        }
    });
    connect(player, &QMediaPlayer::sourceChanged, this, &MainWindow::clearLyricsDisplay);
}

// ---- 模式切换 ----

void MainWindow::setSourceMode(SourceMode mode)
{
    // 如果模式相同，不做任何操作
    if (currentMode == mode) return;

    qDebug() << "[MainWindow] Switching from" << (currentMode == SourceMode::Local ? "Local" : "Server") 
             << "to" << (mode == SourceMode::Local ? "Local" : "Server") << "mode";

    currentMode = mode;

    // 停止当前播放
    if (player->playbackState() == QMediaPlayer::PlayingState ||
        player->playbackState() == QMediaPlayer::PausedState) {
        player->stop();
    }
    // 移除未使用的 RtspPlayer 调用
    // m_rtspPlayer->stop();
    currentRtspUrl.clear();
    currentRemoteId.clear();

    // 重置进度条和UI
    ui->progressBar->setValue(0);
    ui->progressBar->setEnabled(false);
    ui->currentTimeLabel->setText("0:00");
    ui->totalTimeLabel->setText("0:00");
    ui->currentSongLabel->setText(tr("未播放"));
    ui->currentArtistLabel->setText("");
    ui->coverLabel->setText("🎵");
    ui->coverLabel->setStyleSheet("font-size: 48px; background-color: #13102A; border: 2px solid #7C6AEE; border-radius: 12px;");
    ui->playButton->setText(tr("▶ 播放"));

    // 重置连续播放失败计数器
    consecutivePlayFailures = 0;

    updateModeUI();
    updatePlaylistView();  // 刷新播放列表视图

    if (mode == SourceMode::Server) {
        ApiClient::instance()->checkStatus();
        ApiClient::instance()->listServerFiles();
    }
}

void MainWindow::updateModeUI()
{
    qDebug() << "[MainWindow] updateModeUI: currentMode=" 
             << (currentMode == SourceMode::Local ? "Local" : "Server")
             << ", serverConnected=" << serverConnected;

    // 使用 UI 文件中的按钮
    if (!serverConnected) {
        ui->modeSwitchButton->setText(tr("🚫 服务器离线"));
        ui->modeSwitchButton->setEnabled(false);
    } else if (currentMode == SourceMode::Local) {
        ui->modeSwitchButton->setText(tr("🌐 切换到服务器"));
        ui->modeSwitchButton->setEnabled(true);
    } else {
        ui->modeSwitchButton->setText(tr("💻 切换到本地"));
        ui->modeSwitchButton->setEnabled(true);
    }

    // 本地模式禁用搜索功能
    if (currentMode == SourceMode::Local) {
        searchLineEdit->setEnabled(false);
        searchLineEdit->setPlaceholderText(tr("本地模式不支持搜索"));
        searchResultList->hide();
    } else {
        searchLineEdit->setEnabled(true);
        searchLineEdit->setPlaceholderText(tr("搜索服务器歌曲..."));
    }

    QString modeText = (currentMode == SourceMode::Local) ? tr("本地模式") : tr("服务器模式");
    QString connText = serverConnected ? tr(" | 服务器在线") : tr(" | 服务器离线");
    statusBar()->showMessage(modeText + connText);

    qDebug() << "[MainWindow] updateModeUI: button text set to:" << ui->modeSwitchButton->text();
}

void MainWindow::on_modeSwitchButton_clicked()
{
    qDebug() << "[MainWindow] modeSwitchButton clicked, currentMode:" 
             << (currentMode == SourceMode::Local ? "Local" : "Server")
             << ", serverConnected:" << serverConnected;

    if (currentMode == SourceMode::Local) {
        // 从本地模式切换到服务器模式需要服务器已连接
        if (!serverConnected) {
            QMessageBox::information(this, tr("服务器未连接"),
                tr("服务器不可用，只能使用本地模式。\n请确保服务端已启动并检查 config.ini 配置。"));
            return;
        }
        setSourceMode(SourceMode::Server);
    } else {
        // 从服务器模式切换到本地模式不需要服务器连接
        qDebug() << "[MainWindow] Switching to Local mode";
        setSourceMode(SourceMode::Local);
    }
}

// ---- 服务器连接 ----

void MainWindow::on_actionConnect_Server_triggered()
{
    // 服务器连接是自动的，此菜单项仅用于手动刷新状态
    statusBar()->showMessage(tr("正在检测服务器..."), 2000);
    
    // 直接检查状态并根据结果显示消息
    ApiClient::instance()->checkStatus();
    
    // 延迟显示结果（等待网络请求完成）
    QTimer::singleShot(1500, this, [this]() {
        if (serverConnected) {
            statusBar()->showMessage(tr("✅ 服务器在线"), 2000);
        } else {
            statusBar()->showMessage(tr("❌ 服务器离线"), 2000);
        }
    });
}

void MainWindow::onServerConnected()
{
    serverConnected = true;
    updateModeUI();
    
    // 首次连接成功时，自动切换到服务器模式
    if (currentMode == SourceMode::Local) {
        setSourceMode(SourceMode::Server);
        statusBar()->showMessage(tr("✅ 服务器已连接，自动切换到服务器模式"), 3000);
    } else {
        statusBar()->showMessage(tr("✅ 服务器已连接"), 3000);
    }
    
    // 连接成功后拉取服务器文件列表
    ApiClient::instance()->listServerFiles();
}

void MainWindow::onServerDisconnected()
{
    serverConnected = false;
    // 断连时若正在服务器模式，强制切回本地
    if (currentMode == SourceMode::Server) {
        setSourceMode(SourceMode::Local);
    }
    updateModeUI();
    statusBar()->showMessage(tr("⚠️ 服务器未连接，仅本地模式可用"), 5000);
}

void MainWindow::onServerFilesReceived(const QList<RemoteSongInfo>& files)
{
    serverFileCache = files;
    qDebug() << "[MainWindow] Server files received:" << files.size();

    // 清空服务器播放列表并重新填充
    serverPlaylist->clear();

    // 将服务器文件添加到 serverPlaylist
    for (const auto& info : files) {
        QString serverPath = QString("server://%1|%2").arg(info.id, info.name);
        serverPlaylist->addMedia(serverPath);
    }

    // 如果当前是服务器模式，更新播放列表视图
    if (currentMode == SourceMode::Server) {
        updatePlaylistView();
    }

    statusBar()->showMessage(tr("已加载 %1 个服务器歌曲").arg(files.size()), 3000);
}

// ---- 搜索 ----

void MainWindow::on_searchLineEdit_textChanged(const QString& text)
{
    if (text.length() < 1) {
        hideSearchResults();
        return;
    }

    // 本地搜索
    QStringList localResults;
    for (auto* pl : playlists) {
        for (int i = 0; i < pl->mediaCount(); ++i) {
            QString path = pl->filePath(i);
            QFileInfo fi(path);
            QString name = fi.completeBaseName();
            if (name.contains(text, Qt::CaseInsensitive)) {
                localResults.append(path);
            }
            if (localResults.size() >= MAX_SEARCH_RESULTS) break;
        }
        if (localResults.size() >= MAX_SEARCH_RESULTS) break;
    }

    // 服务器搜索（有文本时触发）
    if (currentMode == SourceMode::Server || serverConnected) {
        ApiClient::instance()->searchServer(text);
    }

    // 先展示本地结果
    showSearchResults({}, localResults);
}

void MainWindow::on_searchLineEdit_returnPressed()
{
    QString text = searchLineEdit->text().trimmed();
    if (text.isEmpty()) return;

    // 再次触发搜索
    on_searchLineEdit_textChanged(text);
}

void MainWindow::onSearchResultReceived(const QList<RemoteSongInfo>& results)
{
    // 从本地播放列表搜索
    QStringList localResults;
    QString text = searchLineEdit->text().trimmed();

    if (!text.isEmpty() && localPlaylist) {
        for (int i = 0; i < localPlaylist->mediaCount(); ++i) {
            QString path = localPlaylist->filePath(i);
            QFileInfo fi(path);
            QString name = fi.completeBaseName();
            if (name.contains(text, Qt::CaseInsensitive)) {
                localResults.append(path);
            }
            if (localResults.size() >= MAX_SEARCH_RESULTS) break;
        }
    }

    showSearchResults(results, localResults);
}

void MainWindow::showSearchResults(const QList<RemoteSongInfo>& serverResults,
                                    const QStringList& localResults)
{
    searchResultList->clear();
    searchResultList->setVisible(true);

    // 本地结果
    for (auto& path : localResults) {
        QFileInfo fi(path);
        auto* item = new QListWidgetItem(QString("💻  %1").arg(fi.completeBaseName()));
        item->setData(Qt::UserRole, fi.completeBaseName());       // id
        item->setData(Qt::UserRole + 1, "local");                   // source
        item->setData(Qt::UserRole + 2, path);                      // local path
        searchResultList->addItem(item);
    }

    // 服务器结果
    for (auto& info : serverResults) {
        QString statusIcon = (info.lyricsStatus == "done") ? "🎤 " :
                             (info.lyricsStatus == "generating") ? "⏳ " : "";
        auto* item = new QListWidgetItem(QString("🌐  %1%2 - %3")
            .arg(statusIcon)
            .arg(info.title.isEmpty() ? info.name : info.title)
            .arg(info.artist.isEmpty() ? tr("未知艺术家") : info.artist));
        item->setData(Qt::UserRole, info.id);      // song id
        item->setData(Qt::UserRole + 1, "server");   // source
        searchResultList->addItem(item);
    }

    if (searchResultList->count() == 0) {
        auto* item = new QListWidgetItem(tr("  未找到匹配的歌曲"));
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        searchResultList->addItem(item);
    }

    // 定位浮层到搜索框下方
    QPoint globalPos = searchLineEdit->mapToGlobal(QPoint(0, searchLineEdit->height()));
    searchResultList->move(globalPos);
    searchResultList->setFixedWidth(searchLineEdit->width());
    searchResultList->raise();
}

void MainWindow::hideSearchResults()
{
    searchResultList->setVisible(false);
    searchResultList->clear();
}

// ---- 显示名转换 ----

QString MainWindow::filePathToDisplayName(const QString& filePath) const
{
    if (filePath.startsWith("server://")) {
        // server://id|name|artist → 🌐 name
        QString inner = filePath.mid(9);
        QStringList parts = inner.split('|');
        QString songName = parts.size() >= 2 ? parts[1] : parts[0];
        return QString("🌐  %1").arg(songName);
    } else {
        return QFileInfo(filePath).fileName();
    }
}

QString MainWindow::findPathByDisplayName(const Playlist* playlist, const QString& displayName) const
{
    if (!playlist) return QString();
    for (const QString& path : playlist->filePathList()) {
        if (filePathToDisplayName(path) == displayName) {
            return path;
        }
    }
    return QString();
}

// ---- 服务器上传/下载 ----

void MainWindow::on_actionUpload_File_triggered()
{
    // 自动连接模式下，如果服务器不可达，ApiClient 会通过 errorOccurred 信号反馈
    QString filePath = QFileDialog::getOpenFileName(
        this, tr("选择要上传的音乐文件"), QDir::homePath(),
        tr("音频文件 (*.mp3 *.mp4 *.m4a *.flac *.wav *.aac *.ogg *.wma *.opus);;所有文件 (*.*)"));

    if (!filePath.isEmpty()) {
        ApiClient::instance()->uploadFile(filePath);
        statusBar()->showMessage(tr("正在上传: %1").arg(QFileInfo(filePath).fileName()));
    }
}

void MainWindow::on_actionDownload_File_triggered()
{
    if (serverFileCache.isEmpty()) {
        QMessageBox::information(this, tr("下载"), tr("服务器上没有可下载的文件"));
        return;
    }

    // 创建选择对话框
    QStringList fileNames;
    for (const auto& info : serverFileCache) {
        fileNames.append(info.name);
    }

    QString selectedFile = QInputDialog::getItem(
        this, tr("选择要下载的文件"), tr("文件列表:"), fileNames, 0, false);

    if (selectedFile.isEmpty()) {
        return;
    }

    // 查找选中文件的ID
    QString fileId;
    for (const auto& info : serverFileCache) {
        if (info.name == selectedFile) {
            fileId = info.id;
            break;
        }
    }

    if (fileId.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("无法找到选中的文件"));
        return;
    }

    // 选择保存路径
    QString savePath = QFileDialog::getSaveFileName(
        this, tr("保存到"), QDir::homePath() + "/" + selectedFile,
        tr("音频文件 (*.mp4 *.mp3 *.flac);;所有文件 (*.*)"));

    if (!savePath.isEmpty()) {
        ApiClient::instance()->downloadFile(fileId, savePath);
        statusBar()->showMessage(tr("正在下载: %1").arg(selectedFile));
    }
}

void MainWindow::onUploadFinished(bool success, const QString& fileId, const QString& fileName)
{
    if (success) {
        statusBar()->showMessage(tr("上传成功: %1").arg(fileName), 5000);
        // 刷新服务器文件列表
        ApiClient::instance()->listServerFiles();
    } else {
        QMessageBox::warning(this, tr("上传失败"), tr("文件 %1 上传失败").arg(fileName));
    }
}

// ---- RTSP 播放 ----

void MainWindow::onStreamUrlReady(const QString& remoteId, const QString& streamUrl) {
    qDebug() << "[MainWindow] Stream URL ready:" << streamUrl;
    if (streamUrl.isEmpty()) {
        // 增加连续失败计数
        consecutivePlayFailures++;

        // 检查是否超过最大连续失败次数
        if (consecutivePlayFailures >= MAX_CONSECUTIVE_FAILURES) {
            QMessageBox::warning(this, tr("播放失败"), tr("连续多次播放失败，请检查网络连接或稍后重试"));
            consecutivePlayFailures = 0;  // 重置计数器
            return;
        }

        Playlist* pl = getCurrentPlaylist();
        if (pl && pl->currentIndex() >= 0) {
            pl->removeMedia(pl->currentIndex());
            updatePlaylistView();
            saveAllPlaylists();

            // 如果播放列表不为空，尝试播放下一首（不弹窗）
            if (pl->mediaCount() > 0) {
                playCurrentMedia();
            } else {
                // 播放列表为空，显示提示
                consecutivePlayFailures = 0;  // 重置计数器
                QMessageBox::warning(this, tr("播放失败"), tr("服务器上文件不存在，已从播放列表中移除"));
            }
        }
        return;
    }

    // 播放成功，重置连续失败计数
    consecutivePlayFailures = 0;
    playRemoteSong(remoteId, streamUrl);
}

void MainWindow::playRemoteSong(const QString& remoteId, const QString& streamUrl)
{
    currentRtspUrl = streamUrl;
    currentRemoteId = remoteId;

    // 移除未使用的 RtspPlayer 调用
    // m_rtspPlayer->stop();

    // 查找歌曲信息（从缓存中）
    for (const auto& info : serverFileCache) {
        if (info.id == remoteId) {
            ui->currentSongLabel->setText(info.title.isEmpty() ? info.name : info.title);
            ui->currentArtistLabel->setText(info.artist.isEmpty() ? tr("远程服务器") : info.artist);
            break;
        }
    }

    // 更新封面
    ui->coverLabel->setStyleSheet(
        "font-size: 48px; background-color: #13102A;"
        "border: 2px solid #7C6AEE; border-radius: 12px;"
    );
    ui->coverLabel->setText("🌐");

    // 使用 QMediaPlayer 播放 HTTP URL（原生支持 HTTP 流式播放）
    player->setSource(QUrl(streamUrl));
    player->play();

    // 下载歌词和封面
    ApiClient::instance()->downloadLyrics(remoteId);
    ApiClient::instance()->downloadCover(remoteId);

    statusBar()->showMessage(tr("正在播放: %1").arg(streamUrl), 3000);
}

// ---- 歌词下载回调 ----

void MainWindow::onLyricsReady(const QString& remoteId, const QByteArray& lrcData)
{
    Q_UNUSED(remoteId);

    // 保存到临时文件并加载
    QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                       + "/music_player_lyrics.lrc";
    QFile file(tempPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(lrcData);
        file.close();
        loadLyricsFromFile(tempPath);
    }
}

void MainWindow::onLyricsStatus(const QString& remoteId, const QString& status)
{
    Q_UNUSED(remoteId);
    if (status == "none" || status == "generating") {
        clearLyricsDisplay();
        if (status == "generating") {
            statusBar()->showMessage(tr("歌词生成中，请稍后..."), 3000);
        }
    } else if (status == "instrumental") {
        clearLyricsDisplay();
        ui->lyricsTextBrowser->setHtml(
            "<p align='center' style='color:#8E88B0; font-size:14pt; margin-top:40px;'>"
            "🎻 纯器乐<br><span style='font-size:10pt;'>此曲无人声歌词</span></p>");
    }
}

void MainWindow::onCoverReady(const QString& remoteId, const QByteArray& imageData)
{
    Q_UNUSED(remoteId);
    if (imageData.isEmpty()) return;

    QPixmap pixmap;
    if (pixmap.loadFromData(imageData)) {
        ui->coverLabel->setPixmap(pixmap.scaled(
            ui->coverLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        ui->coverLabel->setStyleSheet(
            "border: 2px solid #7C6AEE; border-radius: 12px;");
    }
}

void MainWindow::onApiErrorOccurred(const QString& errorMsg)
{
    qWarning() << "[MainWindow] API error occurred:" << errorMsg;
    // 可以在这里添加错误处理逻辑
}

// ---- 鼠标事件处理（无边框窗口拖动）----

bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == menuBar()) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (event->type() == QEvent::MouseButtonPress) {
            if (mouseEvent->button() == Qt::LeftButton) {
                // 检查是否点击在菜单项上
                QAction* action = menuBar()->actionAt(mouseEvent->pos());
                if (action == nullptr) {
                    // 没有点击菜单项，允许拖动
                    isDragging = true;
                    dragPosition = mouseEvent->globalPosition().toPoint() - frameGeometry().topLeft();
                    return true;
                }
            }
        } else if (event->type() == QEvent::MouseMove) {
            if (isDragging && mouseEvent->buttons() & Qt::LeftButton) {
                move(mouseEvent->globalPosition().toPoint() - dragPosition);
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            isDragging = false;
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::mousePressEvent(QMouseEvent* event)
{
    QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent* event)
{
    QMainWindow::mouseMoveEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent* event)
{
    QMainWindow::mouseReleaseEvent(event);
}

void MainWindow::mouseDoubleClickEvent(QMouseEvent* event)
{
    // 双击菜单栏区域最大化/还原
    if (event->button() == Qt::LeftButton) {
        QRect menuBarRect = menuBar()->geometry();
        if (menuBarRect.contains(event->pos())) {
            if (isMaximized()) {
                showNormal();
                maxButton->setText("□");
            } else {
                showMaximized();
                maxButton->setText("❐");
            }
            event->accept();
            return;
        }
    }
    QMainWindow::mouseDoubleClickEvent(event);
}

