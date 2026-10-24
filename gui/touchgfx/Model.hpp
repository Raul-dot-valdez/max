/*
 * TouchGFX backend — replaces the Designer-generated gui/include/gui/model/Model.hpp.
 * Designer owns the screens (Dashboard, Fuel, Map/Job, Diagnostics, Settings);
 * this Model is the single place TouchGFX touches application data.
 */
#ifndef MODEL_HPP
#define MODEL_HPP

extern "C" {
#include "ui_model.h"
}

class ModelListener;

class Model
{
public:
    Model();

    void bind(ModelListener* listener) { modelListener = listener; }
    void tick(); /* called by TouchGFX every frame (60 Hz) */

    const ui_model_t& vehicle() const { return data; }

    void ackAlarm()          { ui_cmd_post(UI_CMD_ACK_ALARM, 0); }
    void resetJob()          { ui_cmd_post(UI_CMD_RESET_JOB, 0); }
    void serviceDone()       { ui_cmd_post(UI_CMD_SERVICE_DONE, 0); }
    void setBacklight(int p) { ui_cmd_post(UI_CMD_SET_BACKLIGHT, p); }

protected:
    ModelListener* modelListener;

private:
    ui_model_t data;
    uint32_t   version;
};

#endif
