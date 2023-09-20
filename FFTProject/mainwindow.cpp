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

        int width = ui->label_2->width();
        int height = ui->label_2->height();

        qDebug() << "Input fileImage dimensions: Width = " << width << ", Height = " << height;


        ui->label_2->setPixmap(pix.scaled(width, height, Qt::KeepAspectRatio));
    }
}

QImage performFFTAndGaussianBlur(const QImage &grayImage) {
    int width = grayImage.width();
    int height = grayImage.height();

    qDebug() << "Input image dimensions: Width = " << width << ", Height = " << height;


    // Prepare data for FFTW
    fftw_complex *in, *out;
    fftw_plan p;
    in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * width * height);
    out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * width * height);

    // Fill in array with image data
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            QRgb pixel = grayImage.pixel(x, y);
            int grayValue = qGray(pixel);
            in[y * width + x][0] = static_cast<double>(grayValue);
            in[y * width + x][1] = 0.0;
        }
    }

    // Forward FFT
    qDebug() << "Performing Forward FFT...";
    p = fftw_plan_dft_2d(height, width, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_execute(p);
    qDebug() << "Forward FFT complete.";

    // Gaussian blur in frequency domain
    // (simplified; for demonstration purposes)
    double sigma = 0.2;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            double exponent = -((x - width / 2.0) * (x - width / 2.0) + (y - height / 2.0) * (y - height / 2.0)) / (2.0 * sigma * sigma);
            double factor = std::exp(exponent);
            out[y * width + x][0] *= factor;
            out[y * width + x][1] *= factor;
        }
    }

    // Inverse FFT
    fftw_plan invPlan = fftw_plan_dft_2d(height, width, out, in, FFTW_BACKWARD, FFTW_ESTIMATE);
    fftw_execute(invPlan);

    // Normalize and construct new image
    QImage resultImage(width, height, QImage::Format_Grayscale8);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int value = static_cast<int>(in[y * width + x][0] / (width * height));
            value = std::clamp(value, 0, 255);
            resultImage.setPixel(x, y, qRgb(value, value, value));
        }
    }

    // Clean up FFTW
    fftw_destroy_plan(p);
    fftw_destroy_plan(invPlan);
    fftw_free(in);
    fftw_free(out);

    return resultImage;
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

    // Perform FFT and apply Gaussian blur using FFTW (You should implement this function)
    QImage blurredImage = performFFTAndGaussianBlur(grayImage);


    int width = ui->label_3->width();
    int height = ui->label_3->height();
    QPixmap pix = QPixmap::fromImage(blurredImage);
    // Update UI
    ui->label_3->setPixmap(pix.scaled(width, height, Qt::KeepAspectRatio));
}


