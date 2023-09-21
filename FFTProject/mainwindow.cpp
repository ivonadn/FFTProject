#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QPixmap>
#include <QImage>
#include <QVector>
#include <QQueue>
#include <fftw3.h>


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
        originalImage.load(fileName);
        QPixmap pix = QPixmap::fromImage(originalImage);

        int width = ui->label_loaded->width();
        int height = ui->label_loaded->height();

        qDebug() << "Input fileImage dimensions: Width = " << width << ", Height = " << height;

        ui->label_loaded->setPixmap(pix.scaled(width, height, Qt::KeepAspectRatio));
    }
}


QImage blurChannel(const QImage &channelImage) {
    int width = channelImage.width();
    int height = channelImage.height();

    fftw_complex *in = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * width * height);
    fftw_complex *out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * width * height);

    // Populate the 'in' array with the image data
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            in[y * width + x][0] = (double)qRed(channelImage.pixel(x, y));
            in[y * width + x][1] = 0.0;
        }
    }

    // Forward FFT
    fftw_plan forwardPlan = fftw_plan_dft_2d(height, width, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_execute(forwardPlan);
    fftw_destroy_plan(forwardPlan);

    //this returns a black image
//    double cutoff = 0.1;
//    for (int y = 0; y < height; y++) {
//        for (int x = 0; x < width; x++) {
//            double dist = sqrt((x - width / 2) * (x - width / 2) + (y - height / 2) * (y - height / 2));
//            if (dist > cutoff) {
//                out[y * width + x][0] = 0.0;
//                out[y * width + x][1] = 0.0;
//            }
//        }
//    }

    // Low-pass filter
    int percent = 50;
    int s = 5;
    int cX = width/2;
    int cY = height/2;

    qDebug() <<"TEST "<<((double)percent/(double)100)*height;

    for(int i = 0; i<(width*height);i++){
        int row = i/s;
        int col = i%s;
        int distance = sqrt((col-cX)*(col-cX)+(row-cY)*(row-cY));
        double min = std::min(cX,cY);
        if(distance<((double)percent/(double)100)*min){
            out[i][0] = 0.0;
            out[i][1] = 0.0;
        }
    }

    // Inverse FFT
    fftw_plan inversePlan = fftw_plan_dft_2d(height, width, out, in, FFTW_BACKWARD, FFTW_ESTIMATE);
    fftw_execute(inversePlan);
    fftw_destroy_plan(inversePlan);

    // Convert back to image
    QImage result(width, height, QImage::Format_Grayscale8);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            double value = in[y * width + x][0] / (width * height);  // normalize after inverse FFT
            result.setPixel(x, y, qRgb(value, value, value));
        }
    }

    fftw_free(in);
    fftw_free(out);

    return result;
}

QImage blur(const QImage &image) {
    int width = image.width();
    int height = image.height();

    // Split the image into R, G, B channels
    QImage rChannel(width, height, QImage::Format_Grayscale8);
    QImage gChannel(width, height, QImage::Format_Grayscale8);
    QImage bChannel(width, height, QImage::Format_Grayscale8);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            QColor color(image.pixel(x, y));
            rChannel.setPixel(x, y, qRgb(color.red(), color.red(), color.red()));
            gChannel.setPixel(x, y, qRgb(color.green(), color.green(), color.green()));
            bChannel.setPixel(x, y, qRgb(color.blue(), color.blue(), color.blue()));
        }
    }

    // Apply FFT and blur on each channel
    rChannel = blurChannel(rChannel);
    gChannel = blurChannel(gChannel);
    bChannel = blurChannel(bChannel);

    // Merge the channels to get the blurred color image
    QImage result(width, height, QImage::Format_RGB32);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            QColor rColor(rChannel.pixel(x, y));
            QColor gColor(gChannel.pixel(x, y));
            QColor bColor(bChannel.pixel(x, y));
            result.setPixel(x, y, qRgb(rColor.red(), gColor.green(), bColor.blue()));
        }
    }

    return result;
}

QImage sharpenChannel(const QImage &channelImage) {
    int width = channelImage.width();
    int height = channelImage.height();

    fftw_complex *in = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * width * height);
    fftw_complex *out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * width * height);

    // Populate the 'in' array with the image data
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            in[y * width + x][0] = (double)qRed(channelImage.pixel(x, y));
            in[y * width + x][1] = 0.0;
        }
    }

    // Forward FFT
    fftw_plan forwardPlan = fftw_plan_dft_2d(height, width, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_execute(forwardPlan);
    fftw_destroy_plan(forwardPlan);

    // High-pass filter
    //the higher cutoff value, the higher sharpening effect
    double cutoff = 200.0;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            double dist = sqrt((x - width / 2) * (x - width / 2) + (y - height / 2) * (y - height / 2));
            if (dist < cutoff) {
                out[y * width + x][0] = 0.0;
                out[y * width + x][1] = 0.0;
            }
        }
    }

    // Inverse FFT
    fftw_plan inversePlan = fftw_plan_dft_2d(height, width, out, in, FFTW_BACKWARD, FFTW_ESTIMATE);
    fftw_execute(inversePlan);
    fftw_destroy_plan(inversePlan);

    // Convert back to image
    QImage result(width, height, QImage::Format_Grayscale8);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            double value = in[y * width + x][0] / (width * height);  // normalize after inverse FFT
            result.setPixel(x, y, qRgb(value, value, value));
        }
    }

    fftw_free(in);
    fftw_free(out);

    return result;
}

QImage sharpen(const QImage &image) {
    int width = image.width();
    int height = image.height();

    // Split the image into R, G, B channels
    QImage rChannel(width, height, QImage::Format_Grayscale8);
    QImage gChannel(width, height, QImage::Format_Grayscale8);
    QImage bChannel(width, height, QImage::Format_Grayscale8);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            QColor color(image.pixel(x, y));
            rChannel.setPixel(x, y, qRgb(color.red(), color.red(), color.red()));
            gChannel.setPixel(x, y, qRgb(color.green(), color.green(), color.green()));
            bChannel.setPixel(x, y, qRgb(color.blue(), color.blue(), color.blue()));
        }
    }

    // Apply FFT and sharpening on each channel
    rChannel = sharpenChannel(rChannel);
    gChannel = sharpenChannel(gChannel);
    bChannel = sharpenChannel(bChannel);

    // Merge the channels to get the sharpened color image
    QImage result(width, height, QImage::Format_RGB32);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            QColor rColor(rChannel.pixel(x, y));
            QColor gColor(gChannel.pixel(x, y));
            QColor bColor(bChannel.pixel(x, y));
            result.setPixel(x, y, qRgb(rColor.red(), gColor.green(), bColor.blue()));
        }
    }

    return result;
}



QImage convertToGrayScale(const QImage &image) {
    int width = image.width();
    int height = image.height();

    qDebug() << "Input originalImage dimensions: Width = " << width << ", Height = " << height;


    QImage grayImage(width, height, QImage::Format_Grayscale8);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            QRgb pixel = image.pixel(x, y);
            int grayValue = qGray(pixel);
            grayImage.setPixel(x, y, qRgb(grayValue, grayValue, grayValue));
        }
    }

    return grayImage;
}

void MainWindow::on_pushButton_2_clicked()
{
    if (originalImage.isNull()) {
        qDebug() << "No image loaded.";
        return;
    }

    //QImage grayImage = convertToGrayScale(originalImage);

    // Perform FFT and apply Gaussian blur using FFTW (You should implement this function)
    QImage blurredImage = blur(originalImage);
    int width = ui->label_blur->width();
    int height = ui->label_blur->height();
    QPixmap pix = QPixmap::fromImage(blurredImage);
    // Update UI
    ui->label_blur->setPixmap(pix.scaled(width, height, Qt::KeepAspectRatio));
}



void MainWindow::on_pushButton_3_clicked()
{
    if (originalImage.isNull()) {
        qDebug() << "No image loaded.";
        return;
    }

    QImage sharpenedImage = sharpen(originalImage);
    int width = ui->label_sharpen->width();
    int height = ui->label_sharpen->height();
    QPixmap pix = QPixmap::fromImage(sharpenedImage);
    // Update UI
    ui->label_sharpen->setPixmap(pix.scaled(width, height, Qt::KeepAspectRatio));
}

