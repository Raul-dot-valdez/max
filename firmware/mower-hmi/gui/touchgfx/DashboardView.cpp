/*
 * Example screen implementation. Widget names (rpmText, fuelGauge, ...) must
 * match the TouchGFX Designer project; DashboardViewBase is generated.
 * Each text widget uses a wildcard buffer sized in the Designer.
 */
#include <gui/dashboard_screen/DashboardView.hpp>
#include <texts/TextKeysAndLanguages.hpp>

DashboardView::DashboardView() {}

void DashboardView::setupScreen() { DashboardViewBase::setupScreen(); }
void DashboardView::tearDownScreen() { DashboardViewBase::tearDownScreen(); }

static void setNa(touchgfx::Unicode::UnicodeChar* buf, uint16_t size)
{
    touchgfx::Unicode::strncpy(buf, "--", size);
}

void DashboardView::update(const ui_model_t& m)
{
    using touchgfx::Unicode::snprintf;

    if (m.rpm_na) setNa(rpmTextBuffer, RPMTEXT_SIZE);
    else snprintf(rpmTextBuffer, RPMTEXT_SIZE, "%u", m.rpm);
    rpmText.invalidate();

    if (m.fuel_na) setNa(fuelTextBuffer, FUELTEXT_SIZE);
    else snprintf(fuelTextBuffer, FUELTEXT_SIZE, "%u", m.fuel_pct);
    fuelText.invalidate();
    fuelGauge.setValue(m.fuel_na ? 0 : m.fuel_pct);
    lowFuelIcon.setVisible(m.low_fuel);
    lowFuelIcon.invalidate();

    /* "Runtime left 3 h 25 min" — the headline number operators asked for */
    if (m.runtime_na) setNa(runtimeTextBuffer, RUNTIMETEXT_SIZE);
    else snprintf(runtimeTextBuffer, RUNTIMETEXT_SIZE, "%u h %02u", m.runtime_left_min / 60u, m.runtime_left_min % 60u);
    runtimeText.invalidate();
    estimatedTag.setVisible(m.burn_estimated); /* "est." when derived from tank level slope */
    estimatedTag.invalidate();

    snprintf(hoursTextBuffer, HOURSTEXT_SIZE, "%u.%u", (unsigned)(m.engine_h_x10 / 10u), (unsigned)(m.engine_h_x10 % 10u));
    hoursText.invalidate();

    if (m.coolant_na) setNa(coolantTextBuffer, COOLANTTEXT_SIZE);
    else snprintf(coolantTextBuffer, COOLANTTEXT_SIZE, "%d", m.coolant_c);
    coolantText.invalidate();

    snprintf(areaTextBuffer, AREATEXT_SIZE, "%u.%02u", (unsigned)(m.job_area_m2 / 10000u), (unsigned)(m.job_area_m2 % 10000u / 100u));
    areaText.invalidate(); /* hectares, 2 decimals */

    bladeIcon.setVisible(m.blade_on);
    bladeIcon.invalidate();
    faultIcon.setVisible(m.dtc_count > 0 || m.anomaly_alert);
    faultIcon.invalidate();
    gpsIcon.setVisible(m.gps_fix);
    gpsIcon.invalidate();
    bleIcon.setVisible(m.ble_connected);
    bleIcon.invalidate();
}
