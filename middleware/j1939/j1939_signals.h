/*
 * J1939-71 / J1939-73 decoders feeding the vehicle data broker.
 * Mower-specific signals (blade clutch, deck height) are not standardised;
 * they arrive on an OEM Proprietary-B PGN whose layout must be confirmed
 * against the OEM's CAN database (DBC) before release.
 */
#ifndef J1939_SIGNALS_H
#define J1939_SIGNALS_H

#include "j1939.h"

#define PGN_EEC2   61443u /* 0xF003 engine load */
#define PGN_EEC1   61444u /* 0xF004 engine speed */
#define PGN_DM1    65226u /* 0xFECA active DTCs */
#define PGN_HOURS  65253u /* 0xFEE5 engine total hours (on request) */
#define PGN_ET1    65262u /* 0xFEEE coolant temp */
#define PGN_EFLP1  65263u /* 0xFEEF oil pressure */
#define PGN_CCVS1  65265u /* 0xFEF1 wheel-based speed */
#define PGN_LFE1   65266u /* 0xFEF2 fuel rate */
#define PGN_VEP1   65271u /* 0xFEF7 battery potential */
#define PGN_DD1    65276u /* 0xFEFC fuel level */
#define PGN_VF     65128u /* 0xFE68 hydraulic oil temp */
#define PGN_OEM_DECK 0xFF10u /* Proprietary B — OEM to confirm layout */

void j1939_signals_on_rx(const j1939_msg_t *m, void *ctx);

#endif
