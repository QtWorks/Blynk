#include <QFile>
#include <QTextStream>
#include <QDesktopServices>
#include <QUrl>

// Application:
#include "aboutwindow.h"
#include "ui_aboutwindow.h"
#define VISION_AIDS_OVERSEA_URL "http://getblynk.org/visionaid-overseas-blynk"
#define FACEBOOK_URL "http://facebook.com/blynktech"

// Constructor:
AboutWindow::AboutWindow(const QString &sTitle, QWidget *parent) :
    CustomWindow(sTitle, parent),
    ui(new Ui::AboutWindow)
{
    // Setup UI:
    ui->setupUi(this);

    // Set image:
    ui->wLogoArea->setImage(":/icons/ico-blynklogo.png");

    // Facebook:
    connect(ui->wFaceBookButton, &QPushButton::clicked, this, &AboutWindow::onShowFaceBook);

    // Vision aid overseas:
    connect(ui->wVisionAidOverseasButton, &QPushButton::clicked, this, &AboutWindow::onShowVisionAidOverseas);

    // Done:
    connect(ui->wAboutDoneButton, &QPushButton::clicked, this, &AboutWindow::onDone);
}

// Done:
void AboutWindow::onDone()
{
    onCloseButtonClicked();
}

// Show vision aid overseas dialog:
void AboutWindow::onShowVisionAidOverseas()
{
    QDesktopServices::openUrl(QUrl(VISION_AIDS_OVERSEA_URL));
}

// Show facebook:
void AboutWindow::onShowFaceBook()
{
    QDesktopServices::openUrl(QUrl(FACEBOOK_URL));
}
