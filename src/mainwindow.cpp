#include "config.h"
#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QCloseEvent>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QMessageBox>
#include <QProcess>
#include <QRegularExpression>
#include <QStyle>
#include <QStyleFactory>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    connect(server, SIGNAL(readyReadStandardOutput()), this, SLOT(on_readoutput()));
    connect(server, SIGNAL(readyReadStandardError()), this, SLOT(on_readerror()));
    config->readSettings();
    loadSettings();
    startServer();

    // setup system tray
    tray = new QSystemTrayIcon(this);
    tray->setIcon(QIcon(":/res/icon.ico"));
    tray->setToolTip("QtUnblockNeteaseMusic");
    trayMenu = new QMenu(this);
    trayExit = new QAction(this);
    trayShow = new QAction(this);
    trayShow->setText(tr("Show"));
    trayExit->setText(tr("Exit"));
    trayMenu->addAction(trayShow);
    trayMenu->addAction(trayExit);
    tray->setContextMenu(trayMenu);
    tray->show();
    connect(trayShow, &QAction::triggered, this, &MainWindow::show);
    connect(trayExit, &QAction::triggered, this, [this]()
            {
                tray->hide();
                stopServer();
                QApplication::quit(); });
    // show MainWindow only when tray icon is left clicked
    connect(tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason)
            {
                if(reason == QSystemTrayIcon::Trigger)
                {
                    this->show();
                } });

    // setup theme menu
    styleList = QStyleFactory::keys();
    for (int i = 0; i < styleList.count(); i++)
    {
        qDebug() << "Loading theme" << i << styleList[i];
        // reference: https://stackoverflow.com/a/45265455
        connect(ui->menuTheme->addAction(styleList[i]), &QAction::triggered, this, [i, this]()
                {
                    qDebug() << "Setting theme" << i;
                    QApplication::setStyle(QStyleFactory::create(styleList[i]));
                    QApplication::setPalette(QApplication::style()->standardPalette()); });
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionExit_triggered()
{
    stopServer();
    QApplication::exit();
}

void MainWindow::on_actionAbout_triggered()
{
    QMessageBox aboutDlg(this);
    QString text = QString(tr("QtUnblockNeteaseMusic\n"
                              "Version %1\n\n"
                              "A desktop client for UnblockNeteaseMusic,\n"
                              "written in Qt.\n\n"
                              "Copyright 2022 FrzMtrsprt"))
                       .arg(QApplication::applicationVersion());
    aboutDlg.setWindowTitle("About");
    aboutDlg.setIconPixmap(QPixmap(":/res/icon.png").scaledToHeight(100, Qt::SmoothTransformation));
    aboutDlg.setText(text);
    // add an invisible cancel button so that the dialog can be closed with esc
    aboutDlg.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel | QMessageBox::Help);
    aboutDlg.button(QMessageBox::Cancel)->hide();
    aboutDlg.button(QMessageBox::Help)->setText("GitHub");
    if (aboutDlg.exec() == QMessageBox::Help)
    {
        QDesktopServices::openUrl(QUrl(QApplication::organizationDomain()));
    }
}

void MainWindow::on_startupCheckBox_stateChanged(int state)
{
    config->setStartup(state);
    updateSettings();
    config->writeSettings();
}

void MainWindow::on_readoutput()
{
    outLog(server->readAllStandardOutput().data());
}

void MainWindow::on_readerror()
{
    QMessageBox errorDlg(this);
    errorDlg.setWindowTitle(tr("Server error"));
    errorDlg.setText(tr("The UnblockNeteaseMusic server ran into an error.\n"
                        "Please change the arguments or check port usage and try again."));
    errorDlg.setDetailedText(server->readAllStandardError().data());
    errorDlg.setIcon(QMessageBox::Warning);
    errorDlg.setStandardButtons(QMessageBox::Ok);
    errorDlg.exec();
}

void MainWindow::on_restartBtn_clicked()
{
    stopServer();
    ui->outText->clear();
    startServer();
}

void MainWindow::on_exitBtn_clicked()
{
    stopServer();
    QApplication::exit();
}

void MainWindow::loadSettings()
{
    // load settings from variables into ui
    ui->portEdit->setText(config->port);
    ui->addressEdit->setText(config->address);
    ui->urlEdit->setText(config->url);
    ui->hostEdit->setText(config->host);
    ui->sourceEdit->append(config->source);
    ui->strictCheckBox->setChecked(config->strict);
    ui->startupCheckBox->setChecked(config->startup);
}

void MainWindow::updateSettings()
{
    // update settings from ui into variables
    config->port = ui->portEdit->text();
    config->address = ui->addressEdit->text();
    config->url = ui->urlEdit->text();
    config->host = ui->hostEdit->text();
    config->source = ui->sourceEdit->toPlainText();
    config->strict = ui->strictCheckBox->isChecked();
    config->startup = ui->startupCheckBox->isChecked();
}

int MainWindow::getServer()
{
    QDir appDir(QApplication::applicationDirPath());
    appDir.setFilter(QDir::Dirs | QDir::Files | QDir::NoSymLinks);
    appDir.setNameFilters({"unblock*", "server*"});
    if (appDir.count())
    {
        QFileInfo result(QApplication::applicationDirPath(), appDir[0]);
        if (result.isFile())
        {
            serverFile = result.absoluteFilePath();
            serverArgs = {};
        }
        if (result.isDir())
        {
            QDir serverDir(result.absoluteFilePath());
            if (serverDir.exists("app.js"))
            {
                serverFile = "node";
                serverArgs = {serverDir.absolutePath() + "/app.js"};
            }
            else
            {
                return -1;
            }
        }
        qDebug() << "Server File:" << serverFile.toUtf8().data();
        return 0;
    }
    else
    {
        serverFile = "";
        serverArgs = {};
        return -1;
    }
}

void MainWindow::getArgs()
{
    if (ui->portEdit->text() != "")
    {
        serverArgs << "-p" << ui->portEdit->text();
    }
    if (ui->addressEdit->text() != "")
    {
        serverArgs << "-a" << ui->addressEdit->text();
    }
    if (ui->urlEdit->text() != "")
    {
        serverArgs << "-u" << ui->urlEdit->text();
    }
    if (ui->hostEdit->text() != "")
    {
        serverArgs << "-f" << ui->hostEdit->text();
    }
    if (ui->sourceEdit->toPlainText() != "")
    {
        QString source = ui->sourceEdit->toPlainText();
        QStringList sources = source.split(QRegularExpression("[,.'，。 \n]+"));
        sources.removeAll("");
        serverArgs << "-o" << sources;
    }
    if (ui->strictCheckBox->isChecked() == true)
    {
        serverArgs << "-s";
    }
    qDebug() << "Server Arguments:" << serverArgs.join(" ").toUtf8().data();
}

void MainWindow::startServer()
{
    if (!getServer())
    {
        getArgs();
        server->start(serverFile, serverArgs);
        if (!server->waitForStarted())
        {
            outLog(server->errorString());
        }
    }
    else
    {
        outLog(tr("Server not found."));
    }
}

// reference: https://github.com/barry-ran/QtScrcpy/blob/3929ebf62ee0eb594d566e570a79ccb8efe6bb60/QtScrcpy/ui/dialog.cpp#L359
void MainWindow::outLog(const QString &log)
{
    // avoid sub thread update ui
    QString backLog = log;
    QTimer::singleShot(0, this, [this, backLog]()
                       { ui->outText->append(backLog); });
}

void MainWindow::stopServer()
{
    server->close();
    updateSettings();
    config->writeSettings();
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    this->hide();
    e->ignore();
}
