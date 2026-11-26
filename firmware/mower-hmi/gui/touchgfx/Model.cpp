#include <gui/model/Model.hpp>
#include <gui/model/ModelListener.hpp>

extern "C" {
#include "app_tasks.h"
}

Model::Model() : modelListener(0), data(), version(0) {}

void Model::tick()
{
    app_heartbeat(APP_HB_GUI); /* GUI task liveness for the watchdog supervisor */

    if (ui_model_fetch(&data, &version) && modelListener)
    {
        modelListener->vehicleDataChanged(data);
    }
}
