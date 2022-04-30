#include "rfc_1662_octet_stuffed_framing.h"
/* frame escaping scheme is ppp-like, as described in rfc 1662 sections 4-4.2: frames end with 0x7E,
 and any instances of 0x7E or 0x7D are preceded with an extra 0x7D byte, and then xor'd with 0x20 */

#define END_BYTE 0x7E
#define ESC_BYTE 0x7D
#define ESC_MASK 0x20

/* out must be a buffer of at least 2 * plain_size + 1 bytes */
size_t escape_packet(void * out, const void * in, const size_t plain_size) {
    const unsigned char * restrict  const plain = in;
    unsigned char * restrict const escaped = out;
    size_t escaped_size = 0;

    /* loop over plain bytes */
    for (size_t iplain = 0; iplain < plain_size; iplain++) {
        const unsigned char plain_byte = plain[iplain];

        if (END_BYTE == plain_byte || ESC_BYTE == plain_byte) {
            escaped[escaped_size++] = ESC_BYTE;
            escaped[escaped_size++] = plain_byte ^ ESC_MASK;
        }
        else
            escaped[escaped_size++] = plain_byte;
    }

    /* add unescaped frame-end marker */
    escaped[escaped_size++] = END_BYTE;

    return escaped_size;

}