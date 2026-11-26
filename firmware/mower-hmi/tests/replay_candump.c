/*
 * Replay a candump log through the firmware J1939 stack and print the decoded
 * vehicle data as JSON lines, one per second of log time.
 *
 *   ./replay_candump file.log > decoded.jsonl
 *
 * Used by tools/mower_j1939_synth/validate.py to prove that the synthetic
 * dataset and the firmware decoder agree.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "j1939.h"
#include "j1939_signals.h"
#include "vehicle_data.h"

extern uint32_t g_host_ms;

static pal_status_t tx_sink(const pal_can_frame_t *f) { (void)f; return PAL_OK; }

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static const char *NAMES[VD_COUNT] = {
    "rpm", "load_pct", "coolant_c", "oil_press_kpa", "fuel_rate_lph", "fuel_level_pct",
    "engine_hours", "battery_v", "wheel_speed_kmh", "hyd_oil_c", "blade_engaged", "deck_height_mm", "ambient_c",
    "engine_temp_c", "intake_c", "baro_kpa", "oil_low_sw",
};

static void dump(double t_log)
{
    vd_snapshot_t s;
    vd_snapshot(&s, g_host_ms);
    printf("{\"t\":%.3f", t_log);
    for (int i = 0; i < VD_COUNT; i++) {
        if (s.valid[i]) printf(",\"%s\":%.4f", NAMES[i], s.value[i]);
        else printf(",\"%s\":null", NAMES[i]);
    }
    printf(",\"dtc_count\":%u,\"lamps\":%u}\n", s.dtc_count, s.lamps);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s file.log\n", argv[0]);
        return 2;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror(argv[1]); return 2; }

    char line[256];
    double t0 = -1, next_dump = 0;
    unsigned long frames = 0, bad = 0;
    vd_init();

    while (fgets(line, sizeof(line), f)) {
        double ts;
        char idhex[16], data[40];
        if (sscanf(line, "(%lf) %*s %15[0-9A-Fa-f]#%39[0-9A-Fa-f]", &ts, idhex, data) != 3) { bad++; continue; }
        if (t0 < 0) {
            t0 = ts;
            next_dump = 1.0;
            g_host_ms = 1;
            j1939_cfg_t cfg = { .name = 0x823C000000000099ull, .preferred_sa = 0x29, .arbitrary_capable = true,
                                .tx = tx_sink, .on_rx = j1939_signals_on_rx };
            j1939_init(&cfg, g_host_ms);
        }
        double rel = ts - t0;
        while (rel >= next_dump) {
            dump(next_dump);
            next_dump += 1.0;
        }
        g_host_ms = 1 + (uint32_t)(rel * 1000.0);
        j1939_tick(g_host_ms);

        pal_can_frame_t fr = { .id = (uint32_t)strtoul(idhex, NULL, 16), .ext = strlen(idhex) > 3, .ts_ms = g_host_ms };
        size_t n = strlen(data) / 2;
        fr.dlc = (uint8_t)(n > 8 ? 8 : n);
        for (size_t i = 0; i < fr.dlc; i++)
            fr.data[i] = (uint8_t)(hexval(data[2 * i]) << 4 | hexval(data[2 * i + 1]));
        j1939_on_can_frame(&fr);
        frames++;
    }
    fclose(f);
    fprintf(stderr, "replayed %lu frames (%lu unparsable)\n", frames, bad);
    return bad ? 1 : 0;
}
