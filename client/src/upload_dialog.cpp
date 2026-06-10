#include "upload_dialog.h"
#include "ui_upload_dialog.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>

UploadDialog::UploadDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UploadDialog)
{
    ui->setupUi(this);
    
    // 固定窗口大小
    setFixedSize(size());
    
    // 使用 Qt::UniqueConnection 防止重复连接
    // Qt 的 connectSlotsByName 会自动连接 on_<objectName>_clicked() 槽函数
    // 使用 UniqueConnection 确保不会重复连接
    //connect(ui->browseFileButton, &QPushButton::clicked, this, &UploadDialog::on_browseFileButton_clicked, Qt::UniqueConnection);
    //connect(ui->browseCoverButton, &QPushButton::clicked, this, &UploadDialog::on_browseCoverButton_clicked, Qt::UniqueConnection);
    //connect(ui->uploadButton, &QPushButton::clicked, this, &UploadDialog::on_uploadButton_clicked, Qt::UniqueConnection);
    //connect(ui->cancelButton, &QPushButton::clicked, this, &UploadDialog::on_cancelButton_clicked, Qt::UniqueConnection);
    connect(ui->filePathEdit, &QLineEdit::textChanged, this, &UploadDialog::updateUploadButtonState, Qt::UniqueConnection);
    
    // 初始状态
    updateUploadButtonState();
}

UploadDialog::~UploadDialog()
{
    delete ui;
}

UploadInfo UploadDialog::getUploadInfo() const
{
    return m_uploadInfo;
}

void UploadDialog::on_browseFileButton_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("选择音乐文件"),
        QDir::homePath(),
        tr("音乐文件 (*.mp3 *.m4a *.flac *.wav *.ogg *.mp4);;所有文件 (*.*)")
    );
    
    if (!filePath.isEmpty()) {
        ui->filePathEdit->setText(filePath);
        m_uploadInfo.filePath = filePath;
        
        // 自动从文件名提取标题（不含扩展名）
        QFileInfo fi(filePath);
        QString title = fi.completeBaseName();
        if (ui->titleEdit->text().isEmpty()) {
            ui->titleEdit->setText(title);
            m_uploadInfo.title = title;
        }
    }
}

void UploadDialog::on_browseCoverButton_clicked()
{
    QString coverPath = QFileDialog::getOpenFileName(
        this,
        tr("选择封面图片"),
        QDir::homePath(),
        tr("图片文件 (*.jpg *.jpeg *.png *.gif *.bmp);;所有文件 (*.*)")
    );
    
    if (!coverPath.isEmpty()) {
        ui->coverPathEdit->setText(coverPath);
        m_uploadInfo.coverPath = coverPath;
    }
}

void UploadDialog::on_uploadButton_clicked()
{
    // 验证文件
    if (m_uploadInfo.filePath.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("请选择要上传的音乐文件"));
        return;
    }
    
    QFileInfo fi(m_uploadInfo.filePath);
    if (!fi.exists()) {
        QMessageBox::warning(this, tr("错误"), tr("所选文件不存在"));
        return;
    }
    
    // 验证文件大小（30MB 限制）
    constexpr qint64 maxSize = 30 * 1024 * 1024;
    if (fi.size() > maxSize) {
        QMessageBox::warning(this, tr("错误"), tr("文件大小超过限制（最大30MB）"));
        return;
    }
    
    // 收集信息
    m_uploadInfo.title = ui->titleEdit->text().trimmed();
    m_uploadInfo.artist = ui->artistEdit->text().trimmed();
    m_uploadInfo.album = ui->albumEdit->text().trimmed();
    
    // 如果标题为空，使用文件名
    if (m_uploadInfo.title.isEmpty()) {
        m_uploadInfo.title = fi.completeBaseName();
    }
    
    // 接受对话框
    accept();
}

void UploadDialog::on_cancelButton_clicked()
{
    reject();
}

void UploadDialog::updateUploadButtonState()
{
    bool hasFile = !ui->filePathEdit->text().isEmpty();
    ui->uploadButton->setEnabled(hasFile);
}