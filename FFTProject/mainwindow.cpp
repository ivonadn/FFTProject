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

void fftshift(fftw_complex* data, int width, int height) {
    int halfWidth = width / 2;
    int halfHeight = height / 2;

    // Create a temporary array to hold the shifted values
    fftw_complex* temp = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * width * height);

    // Shift quadrants
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int shiftedY = (y + halfHeight) % height;
            int shiftedX = (x + halfWidth) % width;
            temp[shiftedY * width + shiftedX][0] = data[y * width + x][0];
            temp[shiftedY * width + shiftedX][1] = data[y * width + x][1];
        }
    }

    // Copy back to the original array
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            data[y * width + x][0] = temp[y * width + x][0];
            data[y * width + x][1] = temp[y * width + x][1];
        }
    }

    fftw_free(temp);
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

    //Low-pass filter
    double cutoff = 600;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            double dist = sqrt(x * x + y * y);  // Distance from the top-left corner
            if (dist > width/2) {
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

    //2d array of the image, one for the fft forward and one for the inverse
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

    //High-pass filter
    double cutoff = 60;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            double dist = sqrt(x * x + y * y);  // Distance from the top-left corner
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

    QImage grayImage = convertToGrayScale(originalImage);

    //QImage blurredImage = blurChannel(grayImage);
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

    QImage grayImage = convertToGrayScale(originalImage);

    //QImage sharpenedImage = sharpenChannel(grayImage);
    QImage sharpenedImage = sharpen(originalImage);
    int width = ui->label_sharpen->width();
    int height = ui->label_sharpen->height();
    QPixmap pix = QPixmap::fromImage(sharpenedImage);
    // Update UI
    ui->label_sharpen->setPixmap(pix.scaled(width, height, Qt::KeepAspectRatio));
}

