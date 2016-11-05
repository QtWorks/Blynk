// Qt:
#include <QAction>
#include <QSettings>
#include <QMenu>
#include <QDebug>
#include <QtGui>
#include <QSystemTrayIcon>
#include <QApplication>
#include <QDesktopWidget>
#include <QPainter>

// Application:
#include "controller.h"
#include "dimmerwidget.h"
#include "preferencedialog.h"
#include "utils.h"
#include "blynk.h"

// Defines:
#define PARAMETERS_FILE "parameters.xml"
#define ONE_HOUR 3600
#define THREE_HOURS 10800
#define ONE_DAY 86400

// Constructor:
Controller::Controller(QObject *parent) : QObject(parent),
    // Create tray icon:
    m_pTrayIcon(new QSystemTrayIcon()),
    m_pTrayIconMenu(new QMenu()),
    m_pParameters(new Parameters()),
    m_pDimmerWidget(new DimmerWidget(":/icons/ico-bigeye.gif")),
    m_pPreferenceDialog(new PreferenceDialog()),
    m_iBlynkCursorElapsedTime(0),
    m_iScreenBreakElapsedTime(0),
    m_iBlueLightReducerElapsedTime(0),
    m_bBlynkCursorRandomModeOn(false),
    m_iDelay(0)
{
    m_pDimmerWidget->setParameters(m_pParameters);
    connect(m_pParameters, &Parameters::parameterChanged, m_pDimmerWidget, &DimmerWidget::updateUI);

    m_pPreferenceDialog->setParameters(m_pParameters);
    connect(m_pParameters, &Parameters::parameterChanged, m_pPreferenceDialog, &PreferenceDialog::updateUI);

    m_tApplicationTimer.setInterval(1000);
    connect(&m_tApplicationTimer, &QTimer::timeout, this, &Controller::onApplicationTimerTimeOut);

    // Context menu about to show:
    connect(m_pTrayIconMenu, &QMenu::aboutToShow, this, &Controller::onContextMenuAboutToShow);
}

// Destructor:
Controller::~Controller()
{
    qDebug() << "DESTROY CONTROLLER";
    releaseAll();
}

// Release all:
void Controller::releaseAll()
{
    // Tray icon:
    delete m_pTrayIcon;

    // Tray icon menu:
    delete m_pTrayIconMenu;

    // Parameters:
    delete m_pParameters;

    // Dimmer widget:
    delete m_pDimmerWidget;

    // Preferences dialog:
    delete m_pPreferenceDialog;
}

// Startup:
bool Controller::startup()
{
    // Load parameters:
    loadParameters();

    // Start GUI:
    startGUI();

    // Start application timer:
    m_tApplicationTimer.start();

    return true;
}

// Shutdown:
void Controller::shutdown()
{
    qDebug() << "SHUTDOWN";

    // Reset dimmer:
    m_pDimmerWidget->setStrength(Parameters::NO_STRENGTH);

    // Save parameters:
    saveParameters();
}

// Create actions:
void Controller::createMenu(const CXMLNode &menuNode, QMenu *pRootMenu)
{
    // Exclusive menu?
    bool bIsExclusiveMenu = false;
    QActionGroup *pActionGroup = NULL;
    if (pRootMenu)
    {
        bIsExclusiveMenu = pRootMenu->property("exclusiveMenu").toBool();
        if (bIsExclusiveMenu)
        {
            pActionGroup = new QActionGroup(this);
            pActionGroup->setExclusive(true);
        }
    }

    for (auto node : menuNode.nodes())
    {
        QString sTag = node.tag();
        QString sEnabled = node.attributes()["enabled"].simplified();
        bool bEnabled = sEnabled.isEmpty() ? true : (bool)sEnabled.toInt();
        QString sIsChecked = node.attributes()["checked"].simplified();
        bool bIsChecked = sIsChecked.isEmpty() ? false : (bool)sIsChecked.toInt();
        if (sTag == "MenuItem")
        {
            QString sActionName = node.attributes()["name"];
            QString sObjectName = node.attributes()["objectName"];
            QAction *pAction = new QAction(sActionName, this);
            m_mActions[sObjectName] = pAction;
            pAction->setEnabled(bEnabled);
            connect(pAction, &QAction::triggered, this, &Controller::onActionTriggered);
            pAction->setObjectName(node.attributes()["objectName"]);
            pRootMenu->addAction(pAction);

            if (pActionGroup)
            {
                pAction->setCheckable(true);
                pAction->setChecked(bIsChecked);
                pActionGroup->addAction(pAction);
            }
        }
        else
        if (sTag == "Menu")
        {
            bool bIsExclusive = (bool)node.attributes()["exclusive"].toInt();
            QMenu *pSubMenu = new QMenu(node.attributes()["name"]);
            pSubMenu->setEnabled(bEnabled);
            pSubMenu->setProperty("exclusiveMenu", bIsExclusive);
            pSubMenu->setEnabled(bEnabled);
            pRootMenu->addMenu(pSubMenu);
            createMenu(node, pSubMenu);
        }
    }
}

// Start GUI:
void Controller::startGUI()
{
    /*** TRAY ICON ***/

    initializeTrayIcon();

    /*** TOOLTIP ***/

    createTooltip();

    /*** BLYNK CURSOR ***/

    // Check blynk cursor random mode:
    bool bBlynkCursorRandomMode = (bool)m_pParameters->parameter(Parameters::BLYNK_CURSOR_RANDOM_MODE).toInt();
    if (bBlynkCursorRandomMode)
        enterBlynkCursorRandomMode();
    else
        enterBlynCursorRegularMode();

    // Show dimmer widget:
    m_pDimmerWidget->showFullScreen();
}

// Create tray icon:
void Controller::initializeTrayIcon()
{
    // Create actions:
    CXMLNode rootNode = CXMLNode::loadXMLFromFile(":/xml/BlynkMenu.xml");
    CXMLNode menuNode = rootNode.getNodeByTagName("Menu");
    createMenu(menuNode, m_pTrayIconMenu);

    // Set context menu:
    m_pTrayIcon->setContextMenu(m_pTrayIconMenu);

    // Set icon:
    m_pTrayIcon->setIcon(QIcon(":/icons/ico-splash.png"));

    // Set tooltip:
    m_pTrayIcon->setToolTip(tr("TEST"));

    // Show tray icon:
    m_pTrayIcon->setVisible(true);
}

// Create tooltip:
void Controller::createTooltip()
{
    // Create actions:
    CXMLNode rootNode = CXMLNode::loadXMLFromFile(":/xml/BlynkMenu.xml");
    CXMLNode tooltipNode = rootNode.getNodeByTagName("Tooltip");
    QVector<CXMLNode> vTooltipItems = tooltipNode.getNodesByTagName("TooltipItem");
    QMap<QString, QString> mTooltipValues;
    for (auto node : vTooltipItems)
    {
        QString sTooltipName = node.attributes()["name"];
        QString sTooltipValue = node.attributes()["value"];
        mTooltipValues[sTooltipName] = sTooltipValue;
    }
    m_pPreferenceDialog->setTooltips(mTooltipValues);
}

// Action triggered:
void Controller::onActionTriggered()
{
    QAction *pSender = dynamic_cast<QAction *>(sender());
    if (pSender)
    {
        QString sObjectName = pSender->objectName();

        // Show preferences:
        if (sObjectName == "preferences")
        {
            // Show preferences:
            m_pPreferenceDialog->raise();
            m_pPreferenceDialog->updateUI();
            m_pPreferenceDialog->exec();

            // Set parameters:
            saveParameters();

            // Check blynk cursor random mode:
            bool bBlynkCursorEnabled = (bool)m_pParameters->parameter(Parameters::BLYNK_CURSOR_ENABLED).toInt();
            bool bBlynkCursorRandomMode = (bool)m_pParameters->parameter(Parameters::BLYNK_CURSOR_RANDOM_MODE).toInt();
            if (bBlynkCursorEnabled)
            {
                if (bBlynkCursorRandomMode)
                    enterBlynkCursorRandomMode();
                else
                    enterBlynCursorRegularMode();
            }
        }
        else
        // Screen break disabled for an hour:
        if (sObjectName == "screenBreakDisabledForOneHour")
        {
            m_iDelay = ONE_HOUR;
            m_pParameters->setParameter(Parameters::SCREEN_BREAK_STATE, SCREEN_BREAK_DISABLED_FOR_ONE_HOUR);
            m_iScreenBreakElapsedTime = 0;
        }
        else
        // Screen break disabled for three hour:
        if (sObjectName == "screenBreakDisabledForThreeHours")
        {
            m_iDelay = THREE_HOURS;
            m_pParameters->setParameter(Parameters::SCREEN_BREAK_STATE, SCREEN_BREAK_DISABLED_FOR_THREE_HOURS);
            m_iScreenBreakElapsedTime = 0;
        }
        else
        // Screen break disabled until tomorrow:
        if (sObjectName == "screenBreakDisabledUntilTomorrow")
        {
            m_iDelay = ONE_DAY;
            m_pParameters->setParameter(Parameters::SCREEN_BREAK_STATE, SCREEN_BREAK_DISABLED_UNTIL_TOMORROW);
            m_iScreenBreakElapsedTime = 0;
        }
        else
        // Quit:
        if (sObjectName == "quitBlynk")
        {
            qApp->quit();
        }
    }
}

// Load parameters:
void Controller::loadParameters()
{
    QString sParametersFile = Utils::appDir().absoluteFilePath(PARAMETERS_FILE);
    if (QFile::exists(sParametersFile))
    {
        CXMLNode xRootNode = CXMLNode::loadXMLFromFile(sParametersFile);
        m_pParameters->deserialize(xRootNode);
        m_bBlynkCursorRandomModeOn = (bool)m_pParameters->parameter(Parameters::BLYNK_CURSOR_RANDOM_MODE).toInt();
    }
}

// Save parameters:
void Controller::saveParameters()
{
    CXMLNode rootNode = m_pParameters->serialize();
    QString sParametersFile = Utils::appDir().absoluteFilePath(PARAMETERS_FILE);
    rootNode.saveXMLToFile(sParametersFile);
}

// Enter blynk cursor random mode:
void Controller::enterBlynkCursorRandomMode()
{
    m_tApplicationTimer.stop();
    m_iBlynkCursorElapsedTime = 0;
    int iBlynkPerMinuteRandom = m_pParameters->parameter(Parameters::BLYNK_PER_MINUTE_RANDOM).toInt();
    m_lBlynkCursorSequence = Utils::randomSequence(iBlynkPerMinuteRandom, 5, 50);
    m_tApplicationTimer.start();
}

// Enter blynk cursor regular mode:
void Controller::enterBlynCursorRegularMode()
{
    m_tApplicationTimer.stop();
    m_iBlynkCursorElapsedTime = 0;
    m_tApplicationTimer.start();
}

// Timeout:
void Controller::onApplicationTimerTimeOut()
{
    bool bBlynkCursorEnabled = (bool)m_pParameters->parameter(Parameters::BLYNK_CURSOR_ENABLED).toInt();
    bool bBlynkCursorRandomMode = (bool)m_pParameters->parameter(Parameters::BLYNK_CURSOR_RANDOM_MODE).toInt();
    if (m_bBlynkCursorRandomModeOn != bBlynkCursorRandomMode)
    {
        if (bBlynkCursorRandomMode)
            enterBlynkCursorRandomMode();
        else
            enterBlynCursorRegularMode();
        m_bBlynkCursorRandomModeOn = bBlynkCursorRandomMode;
    }

    QString sScreenBreakState = m_pParameters->parameter(Parameters::SCREEN_BREAK_STATE);
    bool bScreenBreakEnabled = (sScreenBreakState == SCREEN_BREAK_ENABLED);

    // Read screen break state:
    if ((m_iDelay > 0) && (m_iScreenBreakElapsedTime > m_iDelay))
        m_pParameters->setParameter(Parameters::SCREEN_BREAK_STATE, SCREEN_BREAK_ENABLED);

    bScreenBreakEnabled &= (m_iScreenBreakElapsedTime >= m_iDelay);

    // Blue light reducer enabled?
    bool bBlueLightReducerEnabled = (bool)m_pParameters->parameter(Parameters::BLUE_LIGHT_REDUCER_ENABLED).toInt();

    // Blynk cursor only:
    if (m_iBlynkCursorElapsedTime > 0)
    {
        // Blynk cursor enabled:
        if (bBlynkCursorEnabled)
        {
            // Regular mode:
            if (!bBlynkCursorRandomMode)
            {
                int iBlynkCursorRegularity = m_pParameters->parameter(Parameters::BLYNK_CURSOR_REGULARITY).toInt();
                if (m_iBlynkCursorElapsedTime%iBlynkCursorRegularity == 0) {
                    m_pDimmerWidget->playCursor();
                }
            }
            // Random mode:
            else
            {
                if (!m_lBlynkCursorSequence.isEmpty())
                {
                    if (m_iBlynkCursorElapsedTime%m_lBlynkCursorSequence.first() == 0)
                    {
                        m_pDimmerWidget->playCursor();
                        m_lBlynkCursorSequence.takeFirst();

                        if (m_lBlynkCursorSequence.isEmpty())
                        {
                            int iBlynkPerMinuteRandom = m_pParameters->parameter(Parameters::BLYNK_PER_MINUTE_RANDOM).toInt();
                            m_lBlynkCursorSequence = Utils::randomSequence(iBlynkPerMinuteRandom, 5, 50);
                        }
                    }
                }
            }
        }
    }

    // Screen break:
    if (bScreenBreakEnabled && (m_iScreenBreakElapsedTime > 0))
    {
        // Regularity:
        int iScreenBreakRegularity = m_pParameters->parameter(Parameters::SCREEN_BREAK_REGULARITY).toInt();

        // Strength:
        Parameters::Strength eScreenBreakStrength = (Parameters::Strength)m_pParameters->parameter(Parameters::SCREEN_BREAK_STRENGTH).toInt();
        if (m_iScreenBreakElapsedTime%iScreenBreakRegularity == 0) {
            m_pDimmerWidget->playBigEye(eScreenBreakStrength);
        }
    }

    // Blue light reducer:
    if (m_iBlueLightReducerElapsedTime > 0)
    {
        // Blynk light reducer:
        if (bBlueLightReducerEnabled)
        {
            QTime tTriggerTime = QTime::fromString(m_pParameters->parameter(Parameters::BLUE_LIGHT_REDUCER_START_TIME));
            Parameters::Strength eBlueLightReducerStrength = (Parameters::Strength)m_pParameters->parameter(Parameters::BLUE_LIGHT_REDUCER_STRENGTH).toInt();

            if (QTime::currentTime() < tTriggerTime)
                m_pDimmerWidget->setStrength(Parameters::NO_STRENGTH);
            else
                m_pDimmerWidget->setStrength(eBlueLightReducerStrength);
        }
        else m_pDimmerWidget->setStrength(Parameters::NO_STRENGTH);
    }

    m_iBlynkCursorElapsedTime++;
    m_iScreenBreakElapsedTime++;
    m_iBlueLightReducerElapsedTime++;
}

// Context menu about to show:
void Controller::onContextMenuAboutToShow()
{
    QString sScreenBreakState = m_pParameters->parameter(Parameters::SCREEN_BREAK_STATE);
    if (m_mActions["screenBreakDisabledForOneHour"])
        m_mActions["screenBreakDisabledForOneHour"]->setChecked(sScreenBreakState == SCREEN_BREAK_DISABLED_FOR_ONE_HOUR);
    if (m_mActions["screenBreakDisabledForThreeHours"])
        m_mActions["screenBreakDisabledForThreeHours"]->setChecked(sScreenBreakState == SCREEN_BREAK_DISABLED_FOR_THREE_HOURS);
    if (m_mActions["screenBreakDisabledUntilTomorrow"])
        m_mActions["screenBreakDisabledUntilTomorrow"]->setChecked(sScreenBreakState == SCREEN_BREAK_DISABLED_UNTIL_TOMORROW);
}

