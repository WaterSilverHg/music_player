#ifndef UPLOAD_DIALOG_H
#define UPLOAD_DIALOG_H

#include <QDialog>
#include <QString>

namespace Ui {
class UploadDialog;
}

struct UploadInfo {
    QString filePath;
    QString coverPath;
    QString title;
    QString artist;
    QString album;
    QString comment;
};

class UploadDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UploadDialog(QWidget *parent = nullptr);
    ~UploadDialog();

    UploadInfo getUploadInfo() const;

private slots:
    void on_browseFileButton_clicked();
    void on_browseCoverButton_clicked();
    void on_uploadButton_clicked();
    void on_cancelButton_clicked();
    void updateUploadButtonState();

private:
    Ui::UploadDialog *ui;
    UploadInfo m_uploadInfo;
};

#endif // UPLOAD_DIALOG_H
