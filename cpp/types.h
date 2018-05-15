#ifndef TYPES_H
#define TYPES_H

#include <array>

typedef std::array<uint8_t,  52u> prefix_t;
typedef std::array<uint8_t,  32u> hash_t;
typedef std::array<uint8_t,  84u> message_t;
typedef std::array<uint8_t, 200u> state_t;
typedef std::array<uint8_t,  20u> address_t;
typedef std::array<uint8_t,   8u> solution_t;

#endif // !TYPES_H