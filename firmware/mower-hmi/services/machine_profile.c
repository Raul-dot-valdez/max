#include "machine_profile.h"

#include <stddef.h>

#define J  MP_SRC_J1939
#define A  MP_SRC_ANALOG
#define S  MP_SRC_SWITCH
#define DV MP_SRC_DERIVED
#define C  MP_CONF_CONFIRMED
#define I  MP_CONF_INFERRED
#define V  MP_CONF_VERIFY

/* Machine-side signals (wheel speed, hydraulic temp, blade, deck) come from the mower's own
 * controllers or OEM PGNs, not from the engine, so they are J1939 + VERIFY on every ECU profile. */
#define MACHINE_SIDE_SRC  [VD_WHEEL_SPEED_KMH] = J, [VD_HYD_OIL_C] = J, [VD_BLADE_ENGAGED] = J, [VD_DECK_HEIGHT_MM] = J
#define MACHINE_SIDE_CONF [VD_WHEEL_SPEED_KMH] = V, [VD_HYD_OIL_C] = V, [VD_BLADE_ENGAGED] = V, [VD_DECK_HEIGHT_MM] = V

/* Air-cooled EFI engine: everything an ECU may send except coolant temperature. */
#define AIR_EFI_SRC                                                                                  \
    [VD_ENGINE_RPM] = J, [VD_ENGINE_LOAD_PCT] = J, [VD_OIL_PRESS_KPA] = J, [VD_FUEL_RATE_LPH] = J,  \
    [VD_FUEL_LEVEL_PCT] = A, [VD_ENGINE_HOURS] = J, [VD_BATTERY_V] = J, [VD_AMBIENT_C] = J,        \
    [VD_ENGINE_TEMP_C] = J, [VD_INTAKE_C] = J, [VD_BARO_KPA] = J, MACHINE_SIDE_SRC

#define ALL_VERIFY                                                                                   \
    [VD_ENGINE_RPM] = V, [VD_ENGINE_LOAD_PCT] = V, [VD_COOLANT_C] = V, [VD_OIL_PRESS_KPA] = V,      \
    [VD_FUEL_RATE_LPH] = V, [VD_FUEL_LEVEL_PCT] = V, [VD_ENGINE_HOURS] = V, [VD_BATTERY_V] = V,     \
    [VD_AMBIENT_C] = V, [VD_ENGINE_TEMP_C] = V, [VD_INTAKE_C] = V, [VD_BARO_KPA] = V,               \
    [VD_OIL_LOW_SW] = V, MACHINE_SIDE_CONF

/* Clogged-screen rules. Liquid: fixed coolant threshold (thermostat holds a known working point).
 * Air-cooled: no thermostat, and the working oil temperature differs by engine, climate and
 * machine, so a fixed number fails (130 °C never fired on the synthetic fleet). The rule uses a
 * rise above the machine's own learned baseline instead; 5 °C scored best offline (VERIFY on logs). */
#define CLOG_LIQUID_C 95.0f
#define CLOG_AIR_C    0.0f
#define CLOG_AIR_RISE 5.0f

/* The tables start from ALL_VERIFY and then raise individual entries: intended overrides. */
#if defined(__clang__)
#pragma clang diagnostic ignored "-Winitializer-overrides"
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Woverride-init"
#endif

static const mp_profile_t k_profiles[MP_FAM_COUNT] = {
    [MP_FAM_LIQUID_J1939] = {
        MP_FAM_LIQUID_J1939, "liquid-cooled ECU (J1939)", true, true, MP_HEAD_LIQUID, CLOG_LIQUID_C, 0.0f,
        .src = { [VD_ENGINE_RPM] = J, [VD_ENGINE_LOAD_PCT] = J, [VD_COOLANT_C] = J, [VD_OIL_PRESS_KPA] = J,
                 [VD_FUEL_RATE_LPH] = J, [VD_FUEL_LEVEL_PCT] = J, [VD_ENGINE_HOURS] = J, [VD_BATTERY_V] = J,
                 [VD_AMBIENT_C] = J, [VD_ENGINE_TEMP_C] = J, MACHINE_SIDE_SRC },
        .conf = { [VD_ENGINE_RPM] = C, [VD_ENGINE_LOAD_PCT] = C, [VD_COOLANT_C] = C, [VD_OIL_PRESS_KPA] = C,
                  [VD_FUEL_RATE_LPH] = C, [VD_FUEL_LEVEL_PCT] = C, [VD_ENGINE_HOURS] = C, [VD_BATTERY_V] = C,
                  [VD_AMBIENT_C] = C, [VD_ENGINE_TEMP_C] = C, MACHINE_SIDE_CONF },
    },
    [MP_FAM_KAW_FX_EFI] = {
        MP_FAM_KAW_FX_EFI, "Kawasaki FX EFI", true, false, MP_HEAD_AIR_EFI, CLOG_AIR_C, CLOG_AIR_RISE,
        .src = { AIR_EFI_SRC },
        /* Kawasaki states J1939; the individual SPNs still need a log */
        .conf = { ALL_VERIFY, [VD_ENGINE_RPM] = I, [VD_ENGINE_HOURS] = I, [VD_BATTERY_V] = I },
    },
    [MP_FAM_KAW_EVO_EFI] = {
        MP_FAM_KAW_EVO_EFI, "Kawasaki EVO EFI", true, false, MP_HEAD_AIR_EFI, CLOG_AIR_C, CLOG_AIR_RISE,
        .src = { AIR_EFI_SRC }, .conf = { ALL_VERIFY },
    },
    [MP_FAM_VAN_EFI_ETC] = {
        MP_FAM_VAN_EFI_ETC, "Vanguard EFI/ETC", true, false, MP_HEAD_AIR_EFI, CLOG_AIR_C, CLOG_AIR_RISE,
        .src = { AIR_EFI_SRC },
        .conf = { ALL_VERIFY, [VD_ENGINE_RPM] = I, [VD_ENGINE_HOURS] = I, [VD_BATTERY_V] = I },
    },
    [MP_FAM_KOH_EFI] = {
        MP_FAM_KOH_EFI, "Kohler EFI", true, false, MP_HEAD_AIR_EFI, CLOG_AIR_C, CLOG_AIR_RISE,
        .src = { AIR_EFI_SRC }, .conf = { ALL_VERIFY },
    },
    [MP_FAM_PERKINS_1100] = {
        /* mechanical or electronic version not known yet: profile assumes J1939 (VERIFY) */
        MP_FAM_PERKINS_1100, "Perkins 1100 cc diesel", true, true, MP_HEAD_LIQUID, CLOG_LIQUID_C, 0.0f,
        .src = { [VD_ENGINE_RPM] = J, [VD_ENGINE_LOAD_PCT] = J, [VD_COOLANT_C] = J, [VD_OIL_PRESS_KPA] = J,
                 [VD_FUEL_RATE_LPH] = J, [VD_FUEL_LEVEL_PCT] = A, [VD_ENGINE_HOURS] = J, [VD_BATTERY_V] = J,
                 [VD_AMBIENT_C] = J, MACHINE_SIDE_SRC },
        .conf = { ALL_VERIFY, [VD_COOLANT_C] = I },
    },
    [MP_FAM_AIR_EFI] = {
        MP_FAM_AIR_EFI, "air-cooled EFI (make not set)", true, false, MP_HEAD_AIR_EFI, CLOG_AIR_C, CLOG_AIR_RISE,
        .src = { AIR_EFI_SRC }, .conf = { ALL_VERIFY },
    },
    [MP_FAM_CARB] = {
        MP_FAM_CARB, "carbureted, no ECU", false, false, MP_HEAD_NO_ECU, 0.0f, 0.0f,
        .src = { [VD_ENGINE_RPM] = A, [VD_FUEL_LEVEL_PCT] = A, [VD_ENGINE_HOURS] = DV, [VD_BATTERY_V] = A,
                 [VD_BLADE_ENGAGED] = S, [VD_OIL_LOW_SW] = S },
        .conf = { [VD_ENGINE_RPM] = I, [VD_FUEL_LEVEL_PCT] = I, [VD_ENGINE_HOURS] = I, [VD_BATTERY_V] = I,
                  [VD_BLADE_ENGAGED] = I, [VD_OIL_LOW_SW] = V },
    },
    [MP_FAM_EV_80V] = {
        MP_FAM_EV_80V, "80 V electric", false, false, MP_HEAD_ELECTRIC, 0.0f, 0.0f,
        .src = { [VD_ENGINE_HOURS] = DV, [VD_BATTERY_V] = A, [VD_BLADE_ENGAGED] = S },
        .conf = { [VD_ENGINE_HOURS] = I, [VD_BATTERY_V] = V, [VD_BLADE_ENGAGED] = V },
    },
};

static mp_family_t s_active = MP_FAM_UNKNOWN;

const mp_profile_t *mp_get(mp_family_t f)
{
    if (f <= MP_FAM_UNKNOWN || f >= MP_FAM_COUNT)
        return NULL;
    return &k_profiles[f];
}

const char *mp_family_name(mp_family_t f)
{
    const mp_profile_t *p = mp_get(f);
    return p ? p->name : "unknown";
}

const mp_profile_t *mp_active(void) { return mp_get(s_active); }

void mp_set_active(mp_family_t f) { s_active = mp_get(f) ? f : MP_FAM_UNKNOWN; }

bool mp_expects(vd_id_t id)
{
    const mp_profile_t *p = mp_active();
    return p ? p->src[id] != MP_SRC_NONE : true;
}

void mp_detect_reset(mp_detect_t *d) { *d = (mp_detect_t){ 0 }; }

mp_family_t mp_detect_step(mp_detect_t *d, const vd_snapshot_t *v, bool engine_on)
{
    if (!engine_on)
        return MP_FAM_UNKNOWN;
    d->on_s++;
    if (v->valid[VD_ENGINE_RPM]) d->rpm_s++;
    if (v->valid[VD_COOLANT_C]) d->coolant_s++;
    if (v->valid[VD_ENGINE_TEMP_C]) d->engine_temp_s++;
    if (d->on_s < MP_DETECT_SECONDS)
        return MP_FAM_UNKNOWN;
    /* majority of the window, so a few dropped frames do not flip the decision */
    uint32_t half = d->on_s / 2;
    if (d->rpm_s <= half)
        return MP_FAM_CARB;
    if (d->coolant_s > half)
        return MP_FAM_LIQUID_J1939;
    return MP_FAM_AIR_EFI;
}
