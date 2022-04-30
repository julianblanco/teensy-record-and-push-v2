#ifndef _RFC_1662_OCTET_STUFFED_FRAMING_H_
#define __RFC_1662_OCTET_STUFFED_FRAMING_H_

#include <stddef.h>

size_t escape_packet(void * out, const void * in, const size_t plain_size);

#endif