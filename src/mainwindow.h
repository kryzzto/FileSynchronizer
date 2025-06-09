#pragma once

#include <QMainWindow>
#include <QTimer>
#include <QTime>
#include <QFileSystemWatcher>
#include <QDir>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QCloseEvent>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void OnStartButtonClicked();
    void OnStopButtonClicked();
    void CheckScheduledCopy();
    void CopyNewAndChangedFiles();
    void OnManualCopyButton();

private:
    Ui::MainWindow *ui;
    QTimer *checkTimer;
    QHash<QString, QDateTime> lastModifiedTimes;

    QSystemTrayIcon *trayIcon;
    QMenu *trayMenu;

    void CopyFile(const QString &sourcePath, const QString &destPath);
    void LogMessage(const QString &message);
    void ProcessDirectory(const QDir &sourceDir, const QDir &destDir, const QString &relativePath);

protected:
    void closeEvent(QCloseEvent *event) override;
};

