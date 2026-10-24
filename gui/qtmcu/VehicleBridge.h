/*
 * Qt for MCUs backend. Same ui_model_t as the TouchGFX backend; exposes it
 * to QML as a singleton with properties. ui_model_publish() runs in the app
 * task, so values cross into the Qt thread through Qul::EventQueue.
 *
 * Register in the .qmlproject: InterfaceFiles { files: ["VehicleBridge.h"] }
 */
#pragma once

#include <qul/eventqueue.h>
#include <qul/property.h>
#include <qul/singleton.h>

extern "C" {
#include "ui_model.h"
}

struct VehicleBridge : public Qul::Singleton<VehicleBridge>, public Qul::EventQueue<ui_model_t>
{
    Qul::Property<int>  rpm;
    Qul::Property<bool> rpmValid;
    Qul::Property<int>  coolantC;
    Qul::Property<int>  fuelPct;
    Qul::Property<bool> fuelValid;
    Qul::Property<bool> lowFuel;
    Qul::Property<int>  runtimeLeftMin;
    Qul::Property<bool> runtimeValid;
    Qul::Property<bool> runtimeEstimated;
    Qul::Property<int>  engineHoursX10;
    Qul::Property<int>  serviceInHoursX10;
    Qul::Property<bool> bladeOn;
    Qul::Property<bool> gpsFix;
    Qul::Property<int>  jobAreaM2;
    Qul::Property<int>  overlapPct;
    Qul::Property<bool> outsideFence;
    Qul::Property<int>  dtcCount;
    Qul::Property<bool> anomalyAlert;
    Qul::Property<bool> bleConnected;
    Qul::Property<bool> nightMode;

    void ackAlarm()    { ui_cmd_post(UI_CMD_ACK_ALARM, 0); }
    void resetJob()    { ui_cmd_post(UI_CMD_RESET_JOB, 0); }
    void serviceDone() { ui_cmd_post(UI_CMD_SERVICE_DONE, 0); }

    void onEvent(const ui_model_t &m) override
    {
        rpm.setValue(m.rpm);
        rpmValid.setValue(!m.rpm_na);
        coolantC.setValue(m.coolant_c);
        fuelPct.setValue(m.fuel_pct);
        fuelValid.setValue(!m.fuel_na);
        lowFuel.setValue(m.low_fuel);
        runtimeLeftMin.setValue(m.runtime_left_min);
        runtimeValid.setValue(!m.runtime_na);
        runtimeEstimated.setValue(m.burn_estimated);
        engineHoursX10.setValue((int)m.engine_h_x10);
        serviceInHoursX10.setValue((int)m.service_in_h_x10);
        bladeOn.setValue(m.blade_on);
        gpsFix.setValue(m.gps_fix);
        jobAreaM2.setValue((int)m.job_area_m2);
        overlapPct.setValue(m.overlap_pct);
        outsideFence.setValue(m.outside_fence);
        dtcCount.setValue(m.dtc_count);
        anomalyAlert.setValue(m.anomaly_alert);
        bleConnected.setValue(m.ble_connected);
        nightMode.setValue(m.night_mode);
    }
};

/* Hook from the C side: call once at GUI start-up. */
inline void vehicle_bridge_attach()
{
    ui_model_set_notify([](const ui_model_t *m) { VehicleBridge::instance().postEvent(*m); });
}
