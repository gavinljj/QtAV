#include "ScreenSaver.h"
#include <QtCore/QTimerEvent>
#include <QtCore/QLibrary>
#ifdef Q_OS_LINUX
//#include <X11/Xlib.h>
#ifndef Success
#define Success 0
#endif
struct _XDisplay;
typedef struct _XDisplay Display;
typedef Display* (*fXOpenDisplay)(const char*/* display_name */);
typedef int (*fXCloseDisplay)(Display*/* display */);
typedef int (*fXSetScreenSaver)(Display*, int /* timeout */, int /* interval */,int /* prefer_blanking */, int /* allow_exposures */);
typedef int (*fXGetScreenSaver)(Display*, int* /* timeout_return */, int* /* interval_return */, int* /* prefer_blanking_return */, int* /* allow_exposures_return */);
typedef int (*fXResetScreenSaver)(Display*/* display */);
static fXOpenDisplay XOpenDisplay = 0;
static fXCloseDisplay XCloseDisplay = 0;
static fXSetScreenSaver XSetScreenSaver = 0;
static fXGetScreenSaver XGetScreenSaver = 0;
static fXResetScreenSaver XResetScreenSaver = 0;
static QLibrary xlib;
#endif //Q_OS_LINUX
#ifdef Q_OS_MAC
//http://www.cocoachina.com/macdev/cocoa/2010/0201/453.html
#include <CoreServices/CoreServices.h>
#endif //Q_OS_MAC
#include <QAbstractEventDispatcher>
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <QAbstractNativeEventFilter>
#endif

#ifdef Q_OS_WIN
#include <windows.h>

class ScreenSaverEventFilter
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
        : public QAbstractNativeEventFilter
#endif
{
public:
    //screensaver is global
    static ScreenSaverEventFilter& instance() {
        static ScreenSaverEventFilter sSSEF;
        return sSSEF;
    }
    void enable(bool yes = true) {
        if (!yes) {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
            mLastEventFilter = QAbstractEventDispatcher::instance()->setEventFilter(eventFilter);
#else
            QAbstractEventDispatcher::instance()->installNativeEventFilter(this);
#endif
        } else {
            if (!QAbstractEventDispatcher::instance())
                return;
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
            mLastEventFilter = QAbstractEventDispatcher::instance()->setEventFilter(mLastEventFilter);
#else
            QAbstractEventDispatcher::instance()->removeNativeEventFilter(this);
#endif
        }
    }
    void disable(bool yes = true) {
        enable(!yes);
    }

    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) {
        Q_UNUSED(eventType);
        MSG* msg = static_cast<MSG*>(message);
        //qDebug("ScreenSaverEventFilter: %p", msg->message);
        if (WM_DEVICECHANGE == msg->message) {
            qDebug("~~~~~~~~~~device event");
            /*if (msg->wParam == DBT_DEVICEREMOVECOMPLETE) {
                qDebug("Remove device");
            }*/

        }
        if (msg->message == WM_SYSCOMMAND
                && ((msg->wParam & 0xFFF0) == SC_SCREENSAVE
                    || (msg->wParam & 0xFFF0) == SC_MONITORPOWER)
        ) {
            //qDebug("WM_SYSCOMMAND SC_SCREENSAVE SC_MONITORPOWER");
            if (result) {
                //*result = 0; //why crash?
            }
            return true;
        }
        return false;
    }
private:
    ScreenSaverEventFilter() {}
    ~ScreenSaverEventFilter() {}
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    static QAbstractEventDispatcher::EventFilter mLastEventFilter;
    static bool eventFilter(void* message) {
        return ScreenSaverEventFilter::instance().nativeEventFilter("windows_MSG", message, 0);
    }
#endif
};
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
QAbstractEventDispatcher::EventFilter ScreenSaverEventFilter::mLastEventFilter = 0;
#endif
#endif //Q_OS_WIN


ScreenSaver& ScreenSaver::instance()
{
    static ScreenSaver sSS;
    return sSS;
}

ScreenSaver::ScreenSaver()
{
    state_saved = false;
    modified = false;
#ifdef Q_OS_WIN
    lowpower = poweroff = screensaver = 0;
#endif
#ifdef Q_OS_LINUX
    timeout = 0;
    interval = 0;
    preferBlanking = 0;
    allowExposures = 0;
    xlib.setFileName("libX11.so");
    isX11 = xlib.load();
    if (!isX11) {
        qDebug("open X11 so failed: %s", xlib.errorString().toUtf8().constData());
    } else {
        XOpenDisplay = (fXOpenDisplay)xlib.resolve("XOpenDisplay");
        XCloseDisplay = (fXCloseDisplay)xlib.resolve("XCloseDisplay");
        XSetScreenSaver = (fXSetScreenSaver)xlib.resolve("XSetScreenSaver");
        XGetScreenSaver = (fXGetScreenSaver)xlib.resolve("XGetScreenSaver");
        XResetScreenSaver = (fXResetScreenSaver)xlib.resolve("XResetScreenSaver");
    }
    isX11 = XOpenDisplay && XCloseDisplay && XSetScreenSaver && XGetScreenSaver && XResetScreenSaver;
#endif //Q_OS_LINUX
    ssTimerId = 0;
    retrieveState();
}

ScreenSaver::~ScreenSaver()
{
    restoreState();
#ifdef Q_OS_LINUX
    if (xlib.isLoaded())
        xlib.unload();
#endif
}

//http://msdn.microsoft.com/en-us/library/windows/desktop/ms724947%28v=vs.85%29.aspx
//http://msdn.microsoft.com/en-us/library/windows/desktop/aa373208%28v=vs.85%29.aspx
/* TODO:
 * SystemParametersInfo will change system wild settings. An application level solution is better. Use native event
 * SPI_SETSCREENSAVETIMEOUT?
 * SPI_SETLOWPOWERTIMEOUT, SPI_SETPOWEROFFTIMEOUT for 32bit
 */
bool ScreenSaver::enable(bool yes)
{
    bool rv = false;
#ifdef Q_OS_WIN
    ScreenSaverEventFilter::instance().enable(yes);
    modified = true;
    rv = true;
    return true;
#if 0
    //TODO: ERROR_OPERATION_IN_PROGRESS
    if (yes)
        rv = SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, 1, NULL, 0);
    else
        rv = SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, 0, NULL, 0);
#else
    /*
    static EXECUTION_STATE sLastState = 0;
    if (yes) {
        sLastState = SetThreadExecutionState(ES_DISPLAY_REQUIRED);
    } else {
        if (sLastState)
            sLastState = SetThreadExecutionState(sLastState);
    }
    rv = sLastState != 0;
    */
    if (yes) {
        rv = restoreState();
    } else {
        //if (QSysInfo::WindowsVersion < QSysInfo::WV_VISTA) {
            // Not supported on Windows Vista
            SystemParametersInfo(SPI_SETLOWPOWERTIMEOUT, 0, NULL, 0);
            SystemParametersInfo(SPI_SETPOWEROFFTIMEOUT, 0, NULL, 0);
        //}
        rv = SystemParametersInfo(SPI_SETSCREENSAVETIMEOUT, 0, NULL, 0);
        modified = true;
    }
#endif
#endif //Q_OS_WIN
#ifdef Q_OS_LINUX
    if (isX11) {
        Display *display = XOpenDisplay(0);
        // -1: restore default. 0: disable
        int ret = 0;
        if (yes)
            ret = XSetScreenSaver(display, -1, interval, preferBlanking, allowExposures);
        else
            ret = XSetScreenSaver(display, 0, interval, preferBlanking/*DontPreferBlanking*/, allowExposures);
        //TODO: why XSetScreenSaver return 1? now use XResetScreenSaver to workaround
        ret = XResetScreenSaver(display);
        XCloseDisplay(display);
        rv = ret==Success;
        qDebug("ScreenSaver::enable %d, ret %d timeout origin: %d", yes, ret, timeout);
    }
    modified = true;
    if (!yes) {
        if (ssTimerId <= 0) {
            ssTimerId = startTimer(1000 * 60);
        }
    } else {
        if (ssTimerId)
            killTimer(ssTimerId);
    }
    rv = true;
    modified = true;
#endif //Q_OS_LINUX
#ifdef Q_OS_MAC
    if (!yes) {
        if (ssTimerId <= 0) {
            ssTimerId = startTimer(1000 * 60);
        }
    } else {
        if (ssTimerId)
            killTimer(ssTimerId);
    }
    rv = true;
    modified = true;
#endif //Q_OS_MAC
    if (!rv) {
        qWarning("Failed to enable screen saver (%d)", yes);
    } else {
        qDebug("Succeed to enable screen saver (%d)", yes);
    }
    return rv;
}

void ScreenSaver::enable()
{
    enable(true);
}

void ScreenSaver::disable()
{
    enable(false);
}

bool ScreenSaver::retrieveState() {
    bool rv = false;
    qDebug("ScreenSaver::retrieveState");
    if (!state_saved) {
#ifdef Q_OS_WIN
        //if (QSysInfo::WindowsVersion < QSysInfo::WV_VISTA) {
            // Not supported on Windows Vista
            SystemParametersInfo(SPI_GETLOWPOWERTIMEOUT, 0, &lowpower, 0);
            SystemParametersInfo(SPI_GETPOWEROFFTIMEOUT, 0, &poweroff, 0);
        //}
        rv = SystemParametersInfo(SPI_GETSCREENSAVETIMEOUT, 0, &screensaver, 0);
        state_saved = true;
        qDebug("ScreenSaver::retrieveState: lowpower: %d, poweroff: %d, screensaver: %d", lowpower, poweroff, screensaver);
#endif //Q_OS_WIN
#ifdef Q_OS_LINUX
        if (isX11) {
            Display *display = XOpenDisplay(0);
            XGetScreenSaver(display, &timeout, &interval, &preferBlanking, &allowExposures);
            XCloseDisplay(display);
            qDebug("ScreenSaver::retrieveState timeout: %d, interval: %d, preferBlanking:%d, allowExposures:%d", timeout, interval, preferBlanking, allowExposures);
            state_saved = true;
            rv = true;
        }
#endif //Q_OS_LINUX
    } else {
        qDebug("ScreenSaver::retrieveState: state already saved previously, doing nothing");
    }
    return rv;
}

bool ScreenSaver::restoreState() {
    bool rv = false;
    if (!modified) {
        qDebug("ScreenSaver::restoreState: state did not change, doing nothing");
        return true;
    }
    if (state_saved) {
#ifdef Q_OS_WIN
#if 0
        //if (QSysInfo::WindowsVersion < QSysInfo::WV_VISTA) {
            // Not supported on Windows Vista
            SystemParametersInfo(SPI_SETLOWPOWERTIMEOUT, lowpower, NULL, 0);
            SystemParametersInfo(SPI_SETPOWEROFFTIMEOUT, poweroff, NULL, 0);
        //}
        rv = SystemParametersInfo(SPI_SETSCREENSAVETIMEOUT, screensaver, NULL, 0);
        qDebug("WinScreenSaver::restoreState: lowpower: %d, poweroff: %d, screensaver: %d", lowpower, poweroff, screensaver);
#else
        ScreenSaverEventFilter::instance().enable();
        rv = true;
#endif //0
#endif //Q_OS_WIN
#ifdef Q_OS_LINUX
        if (isX11) {
            Display *display = XOpenDisplay(0);
            // -1: restore default. 0: disable
            XSetScreenSaver(display, timeout, interval, preferBlanking, allowExposures);
            XCloseDisplay(display);
            rv = true;
        }
#endif //Q_OS_LINUX
    } else {
        qWarning("ScreenSaver::restoreState: no data, doing nothing");
    }
    return rv;
}

void ScreenSaver::timerEvent(QTimerEvent *e)
{
    if (e->timerId() != ssTimerId)
        return;
#ifdef Q_OS_MAC
    UpdateSystemActivity(OverallAct);
    return;
#endif //Q_OS_MAC
#ifdef Q_OS_LINUX
    if (!isX11)
        return;
    Display *display = XOpenDisplay(0);
    XResetScreenSaver(display);
    XCloseDisplay(display);
#endif //Q_OS_LINUX
}