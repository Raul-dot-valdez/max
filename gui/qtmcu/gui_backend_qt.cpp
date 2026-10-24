/*
 * Qt for MCUs backend hooks for app_tasks. Use with the Qt for MCUs
 * FreeRTOS platform port for the target MCU. Image resources should be built
 * with Qt.ImageCaching / "OnStartup" so they are copied to SDRAM and the QSPI
 * is free for logging after start-up, mirroring the TouchGFX backend.
 */
#include <platforminterface/platforminterface.h>
#include <qul/application.h>
#include <qul/qul.h>

#include "Dashboard.h"     /* generated from Dashboard.qml by qmltocpp */
#include "VehicleBridge.h"

extern "C" {
#include "app_tasks.h"
}

extern "C" void gui_preload_assets(void)
{
    Qul::initHardware();
    Qul::initPlatform();
}

extern "C" void gui_task_entry(void* arg)
{
    (void)arg;
    vehicle_bridge_attach();
    static Qul::Application app;
    static struct Dashboard root;
    app.setRootItem(&root);
    /* A manual update loop instead of app.exec() so the GUI task reports its
     * liveness to the watchdog supervisor every frame. Written against the
     * Qt for MCUs 2.x platform API; not yet built — verify on first integration. */
    for (;;) {
        app_heartbeat(APP_HB_GUI);
        uint64_t next = app.update();
        Qul::Platform::getPlatformInstance()->waitUntil(next);
    }
}
