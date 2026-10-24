#include "j1939_signals.h"

#include "vehicle_data.h"

/* J1939 reserves the top of each range: 0xFB..0xFF (8-bit), 0xFB00.. (16-bit),
 * 0xFB000000.. (32-bit) mean error / not available. */
static bool u8(const j1939_msg_t *m, int i, uint32_t *v)
{
    if (m->len <= i) return false;
    *v = m->data[i];
    return *v <= 0xFA;
}

static bool u16(const j1939_msg_t *m, int i, uint32_t *v)
{
    if (m->len <= i + 1) return false;
    *v = m->data[i] | ((uint32_t)m->data[i + 1] << 8);
    return *v <= 0xFAFF;
}

static bool u32(const j1939_msg_t *m, int i, uint32_t *v)
{
    if (m->len <= i + 3) return false;
    *v = m->data[i] | ((uint32_t)m->data[i + 1] << 8) | ((uint32_t)m->data[i + 2] << 16) |
         ((uint32_t)m->data[i + 3] << 24);
    return *v <= 0xFAFFFFFFu;
}

static void put(vd_id_t id, bool ok, float v, uint32_t ts)
{
    if (ok)
        vd_set(id, v, ts);
    else
        vd_set_invalid(id, ts);
}

/* Decode first, then scale: function-argument evaluation order is unspecified
 * in C, so the raw read must be sequenced before the value expression uses it. */
#define PUT(id, decode, expr)                       \
    do {                                            \
        bool ok_ = (decode);                        \
        put((id), ok_, ok_ ? (float)(expr) : 0.0f, t); \
    } while (0)

static void decode_dm1(const j1939_msg_t *m)
{
    vd_dtc_t dtc[VD_MAX_DTC];
    uint8_t n = 0;
    if (m->len < 6)
        return;
    uint8_t lamps = m->data[0];
    for (uint16_t i = 2; i + 3 < m->len && n < VD_MAX_DTC; i += 4) {
        const uint8_t *p = &m->data[i];
        uint32_t spn = p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)(p[2] & 0xE0) << 11);
        uint8_t fmi = p[2] & 0x1F;
        if (spn == 0 && fmi == 0)
            continue; /* "no active faults" placeholder */
        if (spn == 0x7FFFF)
            continue; /* padding */
        dtc[n++] = (vd_dtc_t){ .sa = m->sa, .spn = spn, .fmi = fmi, .oc = p[3] & 0x7F };
    }
    vd_set_dtcs(m->sa, lamps, dtc, n, m->ts_ms);
}

void j1939_signals_on_rx(const j1939_msg_t *m, void *ctx)
{
    (void)ctx;
    uint32_t v = 0;
    uint32_t t = m->ts_ms;

    switch (m->pgn) {
    case PGN_EEC1: /* SPN 190, bytes 4-5, 0.125 rpm/bit */
        PUT(VD_ENGINE_RPM, u16(m, 3, &v), v * 0.125f);
        break;
    case PGN_EEC2: /* SPN 92, byte 3, 1 %/bit */
        PUT(VD_ENGINE_LOAD_PCT, u8(m, 2, &v), (float)v);
        break;
    case PGN_ET1: /* SPN 110, byte 1, 1 °C/bit, -40 */
        PUT(VD_COOLANT_C, u8(m, 0, &v), (float)v - 40.0f);
        break;
    case PGN_EFLP1: /* SPN 100, byte 4, 4 kPa/bit */
        PUT(VD_OIL_PRESS_KPA, u8(m, 3, &v), v * 4.0f);
        break;
    case PGN_LFE1: /* SPN 183, bytes 1-2, 0.05 L/h */
        PUT(VD_FUEL_RATE_LPH, u16(m, 0, &v), v * 0.05f);
        break;
    case PGN_DD1: /* SPN 96, byte 2, 0.4 %/bit */
        PUT(VD_FUEL_LEVEL_PCT, u8(m, 1, &v), v * 0.4f);
        break;
    case PGN_HOURS: /* SPN 247, bytes 1-4, 0.05 h/bit */
        PUT(VD_ENGINE_HOURS, u32(m, 0, &v), v * 0.05f);
        break;
    case PGN_VEP1: /* SPN 168, bytes 5-6, 0.05 V/bit */
        PUT(VD_BATTERY_V, u16(m, 4, &v), v * 0.05f);
        break;
    case PGN_CCVS1: /* SPN 84, bytes 2-3, 1/256 km/h */
        PUT(VD_WHEEL_SPEED_KMH, u16(m, 1, &v), v / 256.0f);
        break;
    case PGN_VF: /* SPN 1638, byte 1, 1 °C/bit, -40 */
        PUT(VD_HYD_OIL_C, u8(m, 0, &v), (float)v - 40.0f);
        break;
    case PGN_OEM_DECK: /* byte 1 bits 0-1: blade clutch (J1939 2-bit state); byte 2: deck height, 2 mm/bit */
        if (m->len >= 2) {
            uint8_t st = m->data[0] & 0x03;
            put(VD_BLADE_ENGAGED, st <= 1, (float)st, t);
            PUT(VD_DECK_HEIGHT_MM, u8(m, 1, &v), v * 2.0f);
        }
        break;
    case PGN_DM1:
        decode_dm1(m);
        break;
    default:
        break;
    }
}
