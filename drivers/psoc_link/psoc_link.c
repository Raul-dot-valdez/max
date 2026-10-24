#include "psoc_link.h"

#include <string.h>

#include "crc.h"

size_t link_encode(const link_frame_t *f, uint8_t *out, size_t out_max)
{
    uint8_t raw[LINK_MAX_RAW];
    if (f->len > LINK_MAX_PAYLOAD)
        return 0;
    size_t n = 0;
    raw[n++] = f->type;
    raw[n++] = f->seq;
    memcpy(&raw[n], f->payload, f->len);
    n += f->len;
    uint16_t crc = crc16_ccitt(raw, n, 0xFFFF);
    raw[n++] = (uint8_t)crc;
    raw[n++] = (uint8_t)(crc >> 8);

    /* COBS */
    size_t o = 0, code_at = 0;
    uint8_t code = 1;
    if (out_max < n + n / 254 + 2)
        return 0;
    out[o++] = 0; /* placeholder for first code */
    for (size_t i = 0; i < n; i++) {
        if (raw[i] == 0) {
            out[code_at] = code;
            code_at = o++;
            code = 1;
        } else {
            out[o++] = raw[i];
            if (++code == 0xFF) {
                out[code_at] = code;
                code_at = o++;
                code = 1;
            }
        }
    }
    out[code_at] = code;
    out[o++] = 0x00;
    return o;
}

void link_decoder_init(link_decoder_t *d)
{
    memset(d, 0, sizeof(*d));
}

static bool finish(link_decoder_t *d, link_frame_t *out)
{
    uint8_t raw[LINK_MAX_RAW];
    size_t r = 0, i = 0;
    while (i < d->n) {
        uint8_t code = d->buf[i++];
        if (code == 0 || i + code - 1 > d->n)
            return false;
        for (uint8_t k = 1; k < code; k++) {
            if (r >= sizeof(raw)) return false;
            raw[r++] = d->buf[i++];
        }
        if (code != 0xFF && i < d->n) {
            if (r >= sizeof(raw)) return false;
            raw[r++] = 0;
        }
    }
    if (r < 4)
        return false;
    uint16_t crc = (uint16_t)(raw[r - 2] | (raw[r - 1] << 8));
    if (crc16_ccitt(raw, r - 2, 0xFFFF) != crc) {
        d->crc_errors++;
        return false;
    }
    out->type = raw[0];
    out->seq = raw[1];
    out->len = (uint16_t)(r - 4);
    memcpy(out->payload, &raw[2], out->len);
    return true;
}

bool link_decode_byte(link_decoder_t *d, uint8_t b, link_frame_t *out)
{
    if (b == 0x00) {
        bool ok = d->n > 0 && finish(d, out);
        d->n = 0;
        return ok;
    }
    if (d->n >= sizeof(d->buf)) {
        d->overruns++;
        d->n = 0; /* resync on next delimiter */
        return false;
    }
    d->buf[d->n++] = b;
    return false;
}
