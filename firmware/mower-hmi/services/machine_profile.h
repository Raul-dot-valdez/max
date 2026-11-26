/*
 * Machine profile: which canonical signals a given mower + engine family can provide, and where
 * each one comes from (J1939, an analog input, a switch, or derived in the display).
 *
 * Firmware architecture 2 (docs/firmware_architecture_2.md). One firmware image runs on every
 * C1 / Hustler model; the active profile tells the services and the edge AI what to expect,
 * so a missing coolant temperature on an air-cooled engine is "not fitted", never "sensor fault".
 *
 * Sources: docs/fleet-expansion/fleet_profiles.json. Entries marked MP_CONF_VERIFY must be
 * confirmed by logging a real machine (roadmap phase 0).
 */
#ifndef MACHINE_PROFILE_H
#define MACHINE_PROFILE_H

#include "pal_types.h"
#include "vehicle_data.h"

typedef enum {
    MP_SRC_NONE = 0,  /* not fitted on this machine */
    MP_SRC_J1939,     /* broadcast by an ECU on the vehicle bus */
    MP_SRC_ANALOG,    /* measured by the display: ADC or pulse input */
    MP_SRC_SWITCH,    /* on/off input */
    MP_SRC_DERIVED,   /* computed in the display (for example hours from run time) */
} mp_source_t;

typedef enum { MP_CONF_CONFIRMED = 0, MP_CONF_INFERRED, MP_CONF_VERIFY } mp_conf_t;

typedef enum {
    MP_FAM_UNKNOWN = 0,
    MP_FAM_LIQUID_J1939,   /* liquid-cooled engine with a J1939 ECU (phase 1 synthetic machine) */
    MP_FAM_KAW_FX_EFI,     /* Kawasaki FX850V / FX1000V EFI */
    MP_FAM_KAW_EVO_EFI,    /* Kawasaki EVO781 / EVO820 EFI */
    MP_FAM_VAN_EFI_ETC,    /* Vanguard EFI / ETC 28-40 HP */
    MP_FAM_KOH_EFI,        /* Kohler Command PRO EFI */
    MP_FAM_PERKINS_1100,   /* Perkins 1100 cc diesel (Renegade Diesel) */
    MP_FAM_AIR_EFI,        /* air-cooled EFI, make not identified (auto-detect result) */
    MP_FAM_CARB,           /* carbureted, no ECU, no J1939 */
    MP_FAM_EV_80V,         /* C1 80 V battery system (E-Magnum) */
    MP_FAM_COUNT
} mp_family_t;

/* Edge-AI head: which trained model and which rule set apply. */
typedef enum { MP_HEAD_LIQUID = 0, MP_HEAD_AIR_EFI, MP_HEAD_NO_ECU, MP_HEAD_ELECTRIC } mp_head_t;

typedef struct {
    mp_family_t family;
    const char *name;
    bool        has_ecu;
    bool        liquid_cooled;
    mp_head_t   head;
    float       clog_temp_c;   /* liquid-cooled: clogged-screen threshold on coolant temperature */
    float       clog_rise_c;   /* air-cooled: rise of oil temperature above this machine's learned
                                  baseline that counts as a clogged screen */
    uint8_t     src[VD_COUNT];  /* mp_source_t per canonical signal */
    uint8_t     conf[VD_COUNT]; /* mp_conf_t per canonical signal */
} mp_profile_t;

const mp_profile_t *mp_get(mp_family_t f);   /* NULL for MP_FAM_UNKNOWN or out of range */
const char         *mp_family_name(mp_family_t f);

/* Active profile for this machine; NULL until auto-detect or the dealer sets it. */
const mp_profile_t *mp_active(void);
void                mp_set_active(mp_family_t f);
bool                mp_expects(vd_id_t id);  /* true when the active profile says the signal exists;
                                                true for every signal while no profile is set */

/*
 * Auto-detect at first engine runs. Distinguishes the classes the bus can reveal:
 * liquid-cooled ECU, air-cooled ECU, or no ECU. It cannot name the engine make (that needs the
 * dealer, or the ECU's NAME once manufacturer codes are mapped) and it cannot detect electric.
 */
typedef struct {
    uint32_t on_s;          /* seconds observed with the engine running */
    uint32_t rpm_s, coolant_s, engine_temp_s;
} mp_detect_t;

#define MP_DETECT_SECONDS 30u

void        mp_detect_reset(mp_detect_t *d);
/* Call at 1 Hz. engine_on: from rpm, or battery charging voltage when no ECU is talking.
 * Returns MP_FAM_UNKNOWN until it has seen MP_DETECT_SECONDS of engine running. */
mp_family_t mp_detect_step(mp_detect_t *d, const vd_snapshot_t *v, bool engine_on);

#endif
