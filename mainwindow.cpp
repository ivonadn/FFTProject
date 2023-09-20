#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QPixmap>
#include <QImage>
#include <QVector>
#include <QQueue>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_pushButton_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), QDir::homePath(), tr("Images (*.png *.xpm *.jpg *.bmp)"));
    if(!fileName.isEmpty()) {
        QMessageBox::information(this, "...", fileName);
        QImage img(fileName);
        QPixmap pix = QPixmap::fromImage(img);

        int w = ui->label_2->width();
        int h = ui->label_2->height();

        ui->label_2->setPixmap(pix.scaled(w, h, Qt::KeepAspectRatio));
    }
}

