#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QCommonStyle>
#include <QDesktopWidget>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QDirIterator>
#include <QtConcurrent>

#include <unordered_map>
#include <QCryptographicHash>

main_window::main_window(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);

    setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size(), qApp->desktop()->availableGeometry()));

    ui->treeWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->treeWidget->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    QCommonStyle style;
    ui->actionScan_Directory->setIcon(style.standardIcon(QCommonStyle::SP_DialogOpenButton));
    ui->actionExit->setIcon(style.standardIcon(QCommonStyle::SP_DialogCloseButton));
    ui->actionAbout->setIcon(style.standardIcon(QCommonStyle::SP_DialogHelpButton));
    ui->actionDelete->setIcon(style.standardIcon(QCommonStyle::SP_TrashIcon));

    connect(ui->actionScan_Directory, &QAction::triggered, this, &main_window::select_directory);
    connect(ui->actionExit, &QAction::triggered, this, &QWidget::close);
    connect(ui->actionAbout, &QAction::triggered, this, &main_window::show_about_dialog);
    connect(ui->actionDelete, &QAction::triggered, this, &main_window::delete_selected);

    qRegisterMetaType<QVector<QString>>("QVector<QString>");
    connect(this, &main_window::found_duplicates, this, &main_window::display_duplicates);
    connect(this, &main_window::updateStatusBar, this, &main_window::onUpdateStatusBar);

    emit updateStatusBar("Greetings!");
}

main_window::~main_window()
{}

void main_window::onUpdateStatusBar(QString  message) {
    ui->statusBar->showMessage(message);
}

void main_window::select_directory()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Directory for Scanning",
                                                    QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty() && !dir.isNull()) {
        scan_directory(dir);
    }
}

void main_window::display_duplicates(int num_grp, qint64 fsize, QVector<QString> duplicates) {
    QTreeWidgetItem* item = new QTreeWidgetItem(ui->treeWidget);
    item->setText(0, "Group № " + QString::number(num_grp) + ". " + QString::number(duplicates.size()) + " duplicates");
    item->setText(1, QString::number(fsize * static_cast<qint64>(duplicates.size())));
    item->setFlags(Qt::ItemIsDragEnabled | Qt::ItemIsEnabled); // magic to mke topics nonselectable
    for (auto s : duplicates) {
        QTreeWidgetItem* child = new QTreeWidgetItem();
        child->setText(1, QString::number (fsize));
        child->setText(0, s);
        item->addChild(child);
    }
}

bool bytewise_equal(QString a, QString b) {
    QFile file1(a);
    file1.open(QFile::ReadOnly);

    QFile file2(b);
    file2.open(QFile::ReadOnly);


    while(!file1.atEnd() || !file2.atEnd()) {
        QByteArray part1 = file1.read(8192);
        QByteArray part2 = file2.read(8192);

        if (part1 != part2) return false;
    }

    return true;
}

QVector<QString> transport_duplicates(QVector<QString> & list) {
    QVector<QString> result, notresult;
    for (QString el : list) {
        if (bytewise_equal(el, list[0])) {
            result.push_back(el);
        }
        else {
            notresult.push_back(el);
        }
    }
    list = notresult;
    return result;
}


void main_window::scan_directory(QString const& dir) {
    ui->treeWidget->clear();
    emit updateStatusBar("Traversing and bucketing by size");
    setWindowTitle(QString("Duplicates for dir  %1").arg(dir));
    QTime timer;
    timer.start();
    QtConcurrent::run([dir, timer, this]() {
        std::unordered_map <qint64, std::vector <QString>> mp;
        QDir d(dir);
        QDirIterator it(dir, QDir::NoDotAndDotDot | QDir::Hidden | QDir::NoSymLinks | QDir::AllEntries, QDirIterator::Subdirectories);
        while(it.hasNext()) {
            it.next();
            if (it.fileInfo().isDir()) continue;
            mp[it.fileInfo().size()].push_back(it.filePath());
        }
        emit updateStatusBar("Now we are calculating hashes");
        int grp = 0;
        for (auto x : mp) {
            std::vector <QString> duplicates = x.second;
            if (duplicates.size() < 2) continue;
            QHash<QByteArray, QVector<QString>> mp2;
            for (QString name : duplicates) {
                QCryptographicHash crypto(QCryptographicHash::Sha3_256);
                QFile file(name);
                file.open(QFile::ReadOnly);

                while(!file.atEnd()) {
                    crypto.addData(file.read(8192));
                }

                mp2[crypto.result()].push_back(name);

            }
            emit updateStatusBar("Now it is duplicates searching time");
            for (QVector <QString> & remains : mp2) {
                while (true) {
                    QVector<QString> dupl = transport_duplicates(remains);
                    if (dupl.size() > 1) {
                        ++grp;

                        emit found_duplicates(grp,x.first,dupl);
                    }
                    if (remains.empty()) {
                        break;
                    }
                }
            }
        }
        emit updateStatusBar(QString("Finished in ") + QString::number(timer.elapsed() / 1000.0) + QString(" sec"));
    });
}


void main_window::delete_selected() {
    QList<QTreeWidgetItem*>  list = ui->treeWidget->selectedItems();
    if (list.size() == 0) {
        return;
    }
    QMessageBox dialog;
    dialog.setWindowTitle("Confirmation of deletion of " + QString::number(list.size()) + " files");
    dialog.setText("Are          you          sure          ???");
    dialog.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    dialog.setDefaultButton(QMessageBox::Cancel);
    dialog.setIcon(QMessageBox::Warning);
    int deletion_counter = 0;
    if (dialog.exec() == QMessageBox::Ok) {
        for (auto it = list.begin(); it != list.end(); ++it) {
            if (QFile::remove((*it)->text(0))) {
                (*it)->setSelected(false);
                (*it)->setHidden(true);
                ++deletion_counter;
            }
        }
        updateStatusBar("Deleted " + QString::number(deletion_counter) + " of " + QString::number(list.size()) + " selected files" );
        if (deletion_counter != list.size()) {/*write ifo about failed files somewhere*/}
    }
}

void main_window::show_about_dialog()
{
    QMessageBox::aboutQt(this);
}
