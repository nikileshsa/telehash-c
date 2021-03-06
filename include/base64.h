#ifndef b64_h
#define b64_h

#include <stddef.h>
#include <stdint.h>

// min buffer size for encoding/decoding
#define base64_encode_length(x) (8 * (((x) + 2) / 6)) + 3
#define base64_decode_length(x) ((((x) + 2) * 6) / 8)

// encode str of len into out (must be at least base64_encode_length(len) big), return actual encoded len
size_t base64_encoder(const uint8_t *str, size_t len, char *out);

// decode str of len into out (must be base64_decode_length(len) bit), return actual decoded len
size_t base64_decoder(const char *str, size_t len, uint8_t *out);

#endif

