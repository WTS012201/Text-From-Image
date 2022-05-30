﻿#include "../headers/imageframe.h"

ImageFrame::ImageFrame(QWidget* parent, Ui::MainWindow* ui):
  scene{new QGraphicsScene(this)}, scalar{1.0}, scaleFactor{0.1}
{
  initUi(parent);
  setWidgets(ui);
  buildConnections();
}

ImageFrame::~ImageFrame(){
  delete scene;
}

void ImageFrame::changeZoom(){
  float val = (zoomEdit -> text()).toFloat();

  scalar = (val < 0.1) ? 0.1 : val;
  scalar = (scalar > 10.0) ? 10.0 : scalar;
  zoomEdit->setText(QString::number(scalar));
  resize(originalSize * scalar);
}

void ImageFrame::buildConnections(){
  connect(zoomEdit, &QLineEdit::editingFinished, this, &ImageFrame::changeZoom);
  connect(this, &ImageFrame::rawTextChanged, this, &ImageFrame::setRawText);
}

void ImageFrame::setRawText(){
  qDebug() << rawText;
}

void ImageFrame::setWidgets(Ui::MainWindow* ui){
  zoomEdit = ui->zoomFactor;
  progressBar = ui->progressBar;
  zoomLabel = ui->label;

  zoomEdit->hide();
  zoomLabel->hide();
  progressBar->hide();
}

void ImageFrame::initUi(QWidget* parent){
  parent->setContentsMargins(0,0,0,0);

  this->setParent(parent);
  this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  this->hide();
}

void ImageFrame::resize(QSize newSize){
  QPixmap image = QPixmap{currImage}.scaled(newSize);

  scene->clear();
  scene->addPixmap(image);
  scene->setSceneRect(image.rect());

  this->setScene(scene);
  this->setMinimumSize(image.size());
}

void ImageFrame::zoomIn(){
  (scalar + scaleFactor > 10.0) ? scalar = 10.0 : scalar += scaleFactor;
  zoomEdit->setText(QString::number(scalar));
  resize(originalSize * scalar);
}

void ImageFrame::zoomOut(){
  (scalar - scaleFactor < 0.1) ? scalar = 0.1 : scalar -= scaleFactor;
  zoomEdit->setText(QString::number(scalar));
  resize(originalSize * scalar);
}

void ImageFrame::mousePressEvent(QMouseEvent* event) {
  if(event->buttons() & Qt::LeftButton){
    zoomIn();
  }else if(event->buttons() & Qt::RightButton){
    zoomOut();
  }
}

cv::Mat ImageFrame::QImageToMat(QImage image){
    cv::Mat mat;

    switch(image.format()){
        case QImage::Format_ARGB32:
        case QImage::Format_RGB32:
        case QImage::Format_ARGB32_Premultiplied:
            mat = cv::Mat(
                image.height(), image.width(),
                CV_8UC4, (void*)image.constBits(),
                image.bytesPerLine()
            );
            break;
        case QImage::Format_RGB888:
            mat = cv::Mat(
                image.height(), image.width(),
                CV_8UC3, (void*)image.constBits(),
                image.bytesPerLine()
            );
            cv::cvtColor(mat, mat, cv::COLOR_BGR2RGB);
            break;
        case QImage::Format_Grayscale8:
            mat = cv::Mat(
                image.height(), image.width(),
                CV_8UC1, (void*)image.constBits(),
                image.bytesPerLine()
            );
            break;
        default:
            break;
    }
    return mat;
}

void ImageFrame::setImage(QString imageName){
  currImage = imageName;
  scalar = 1.0;

  QPixmap image{imageName};

  scene->addPixmap(image);
  scene->setSceneRect(image.rect());
  scene->update();
  this->setScene(scene);

  originalSize = image.size();
//  this->parentWidget()->setMaximumSize(image.size());
  this->setMinimumSize(image.size());
  extract();
}

void ImageFrame::showAll(){
  zoomEdit->show();
  zoomLabel->show();
  this->show();
}

QString ImageFrame::collect(cv::Mat& matrix){
  tesseract::TessBaseAPI *api = new tesseract::TessBaseAPI();

  api->Init(nullptr, "eng", tesseract::OEM_LSTM_ONLY);
  api->SetPageSegMode(tesseract::PSM_AUTO);
  api->SetImage(matrix.data, matrix.cols, matrix.rows, 4, matrix.step);
  auto text = QString{api->GetUTF8Text()};

  api->End();
  delete api;

  return text;
}

void ImageFrame::extract(){
  cv::Mat matrix;
  try{
    matrix = cv::imread(currImage.toStdString(), cv::IMREAD_COLOR);
  }catch(...){
    return;
  }

  if(matrix.empty()){
    qDebug() << "empty matrix";
    return;
  }  
  // transform matrix for darker images
  // upscale/downscale here maybe
  matrix.convertTo(matrix, -1, 2, 0);

  progressBar->show();
  auto thread = QThread::create(
    [this](cv::Mat matrix) mutable -> auto{
        rawText = collect(matrix);
        emit rawTextChanged();
      },
    matrix
  );

  thread->start();
  progressBar->hide();
  showAll();
}
