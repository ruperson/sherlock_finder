#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <memory>

namespace Ui {
class MainWindow;
}

class main_window : public QMainWindow
{
    Q_OBJECT

public:
    explicit main_window(QWidget *parent = 0);
    ~main_window();

private slots:
    void select_directory();
    void scan_directory(QString const& dir);
    void show_about_dialog();
    void delete_selected();
    void display_duplicates(int num_grp, qint64 fsize, QVector<QString> duplicates);
    void onUpdateStatusBar(QString);

signals:
    void found_duplicates(int num_grp, qint64 fsize, QVector<QString> duplicates);
    void updateStatusBar(QString);

private:
    std::unique_ptr<Ui::MainWindow> ui;
};

#endif // MAINWINDOW_H
