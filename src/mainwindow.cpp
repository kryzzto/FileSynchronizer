#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QDateTime>
#include <QFileInfo>

// UI and system tray setup
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Set window icon
    this->setWindowIcon(QIcon(":/icon.png"));

    // Inicialize app icon for tray
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QIcon(":/icon.png"));
    trayIcon->setToolTip("File Synchronizer");

    // Set app tray functionality
    trayMenu = new QMenu(this);
    trayMenu->addAction("Show", this, [this]() { this->show(); });
    trayMenu->addAction("Start", this, &MainWindow::OnStartButtonClicked);
    trayMenu->addAction("Stop", this, &MainWindow::OnStopButtonClicked);
    trayMenu->addSeparator();
    trayMenu->addAction("Quit", qApp, &QApplication::quit);

    // Set app icon for tray
    trayIcon->setContextMenu(trayMenu);
    trayIcon->setIcon(QIcon(":/icon.png"));
    trayIcon->show();

    // Connect tray icon to show main window
    connect(trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason)
    {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            this->showNormal();
    });

    // Timer check for copy scheduling
    checkTimer = new QTimer(this);
    connect(checkTimer, &QTimer::timeout, this, &MainWindow::CheckScheduledCopy);

    // Manual copy button
    connect(ui->manualCopyButton, &QPushButton::clicked, this, &MainWindow::OnManualCopyButton);

    // Set copy times (default 12:00, 18:00)
    ui->timeEdit1->setTime(QTime(12, 0));
    ui->timeEdit2->setTime(QTime(18, 0));
}

MainWindow::~MainWindow()
{
    delete ui;
}

// Start button - validate paths, start scheduled checking
void MainWindow::OnStartButtonClicked()
{
    // Set system file paths
    QString sourcePath = ui->sourceLineEdit->text();
    QString destPath = ui->destinationLineEdit->text();

    // Check for missing paths
    if (sourcePath.isEmpty() || destPath.isEmpty())
    {
        QMessageBox::warning(this, "Warning", "Please enter both source and destination paths");
        return;
    }

    QDir sourceDir(sourcePath);
    QDir destDir(destPath);

    // Validate paths
    if (!sourceDir.exists())
    {
        QMessageBox::warning(this, "Warning", "Source directory does not exist");
        return;
    }

    if (!destDir.exists())
    {
        if (!destDir.mkpath("."))
        {
            QMessageBox::warning(this, "Warning", "Could not create destination directory");
            return;
        }
    }

    // Start checking the time for scheduled data sync
    checkTimer->start(60000);           // 1 minute
    LogMessage("Synchronization started. Next check at scheduled times.");
}

// Stop button - stop data synchronization
void MainWindow::OnStopButtonClicked()
{
    checkTimer->stop();
    LogMessage("Synchronization stopped.");
}

// Check system time for scheduled copy
void MainWindow::CheckScheduledCopy()
{
    QTime currentTime = QTime::currentTime();
    QTime scheduledTime1 = ui->timeEdit1->time();
    QTime scheduledTime2 = ui->timeEdit2->time();

    // Execute copy if current time is within one minute of scheduled time
    if ((currentTime >= scheduledTime1 && currentTime <= scheduledTime1.addSecs(60)) ||
        (currentTime >= scheduledTime2 && currentTime <= scheduledTime2.addSecs(60)))
    {
        CopyNewAndChangedFiles();
    }
}

// File synchronization
void MainWindow::CopyNewAndChangedFiles()
{
    QString sourcePath = ui->sourceLineEdit->text();
    QString destPath = ui->destinationLineEdit->text();

    QDir sourceDir(sourcePath);
    QDir destDir(destPath);

    // Copy files and directories recursively
    ProcessDirectory(sourceDir, destDir, sourcePath);

    LogMessage(QString("Synchronization completed at %1").arg(QDateTime::currentDateTime().toString()));
}

// Manual data copying
void MainWindow::OnManualCopyButton()
{
    CopyNewAndChangedFiles();
    LogMessage("Manual copy triggered.");
}

// Copy single file or directory
void MainWindow::CopyFile(const QString &sourcePath, const QString &destPath)
{
    QFileInfo sourceInfo(sourcePath);

    if (sourceInfo.isDir())
    {
        // Create destination path for directories
        QDir().mkpath(destPath);
        return;
    }

    QFile sourceFile(sourcePath);
    QFile destFile(destPath);

    // Ensure destination directory exists
    QFileInfo destInfo(destPath);
    QDir().mkpath(destInfo.absolutePath());

    // Remove existing destination file
    if (destFile.exists())
    {
        if (!destFile.remove())
        {
            LogMessage(QString("Failed to remove existing file: %1").arg(destPath));
            return;
        }
    }

    if (!sourceFile.copy(destPath))
        LogMessage(QString("Failed to copy file: %1").arg(sourcePath));
    else
        LogMessage(QString("Copied: %1 to %2").arg(sourcePath, destPath));
}

// Logging functionality for UI text box
void MainWindow::LogMessage(const QString &message)
{
    ui->logTextEdit->append(QString("[%1] %2")
                                .arg(QDateTime::currentDateTime().toString())
                                .arg(message));
}

// Recursively check files and directories for change and copy detection
void MainWindow::ProcessDirectory(const QDir &sourceDir, const QDir &destDir, const QString &relativePath)
{
    // Ensure destination directory exists
    if (!destDir.exists() && !destDir.mkpath("."))
    {
        LogMessage(QString("Failed to create directory: %1").arg(destDir.path()));
        return;
    }

    // Process files in the directory
    QStringList files = sourceDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    foreach (const QString &file, files)
    {
        QString sourceFilePath = sourceDir.filePath(file);
        QString destFilePath = destDir.filePath(file);

        QFileInfo fileInfo(sourceFilePath);
        QDateTime currentModified = fileInfo.lastModified();

        bool shouldCopy = false;

        // Determine whether file is new or modified
        if (!lastModifiedTimes.contains(sourceFilePath))
        {
            shouldCopy = true;
            LogMessage(QString("New file detected: %1").arg(sourceDir.relativeFilePath(sourceFilePath)));
        }
        else if (lastModifiedTimes[sourceFilePath] < currentModified)
        {
            shouldCopy = true;
            LogMessage(QString("Modified file detected: %1").arg(sourceDir.relativeFilePath(sourceFilePath)));
        }

        // Perform copy and update timestamp map to avoid copying files again
        if (shouldCopy)
        {
            CopyFile(sourceFilePath, destFilePath);
            lastModifiedTimes[sourceFilePath] = currentModified;
        }
    }

    // Process subdirectories recursively
    QStringList dirs = sourceDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    foreach (const QString &dir, dirs)
    {
        QDir newSourceDir(sourceDir.filePath(dir));
        QDir newDestDir(destDir.filePath(dir));
        ProcessDirectory(newSourceDir, newDestDir, dir);
    }
}

// Override close event to minimize to tray insted of exiting app
void MainWindow::closeEvent(QCloseEvent *event)
{
    if (trayIcon->isVisible())
    {
        this->hide();
        event->ignore();
        trayIcon->showMessage("File Synchronizer",
                              "Running in the background. Right-click tray icon for options.",
                              QSystemTrayIcon::Information, 3000);
    }
}

