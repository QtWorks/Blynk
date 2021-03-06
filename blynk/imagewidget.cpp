// Qt:
#include <QPainter>
#include <QBitmap>
#include <QDebug>

// Application:
#include "imagewidget.h"

// Constructor:
ImageWidget::ImageWidget(QWidget *parent) : QWidget(parent),
    m_sImagePath(""), m_sText(""), m_textColor(QColor(0, 0, 0))
{
}

// Set image:
void ImageWidget::setImage(const QString &sImagePath)
{
    m_sImagePath = sImagePath;
    update();
}

// Set text:
void ImageWidget::setText(const QString &sText, const QColor &textColor)
{
    m_sText = sText;
    m_textColor = textColor;
}

// Paint event:
void ImageWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), QColor(Qt::transparent));
    QImage image(m_sImagePath);
    if (!image.isNull())
    {
        // Compute image ratio:
        double ratio = qRound((double)image.width()/(double)image.height());
        int targetHeight = qRound(rect().height()*.5);
        int targetWidth = qRound(targetHeight*ratio);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        int x = (rect().width()-targetWidth)/2;
        int y = (rect().height()-targetHeight)/2;

        // Mask white pixels:
        QImage scaledImage = image.scaled(targetWidth, targetHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        // Draw:
        painter.drawImage(QPoint(x, y), scaledImage, QRect(0, 0, image.width(), image.height()), Qt::ColorOnly);
    }
}
