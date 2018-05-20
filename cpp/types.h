#ifndef _TYPES_H_
#define _TYPES_H_

#include <array>

typedef std::array<uint8_t,  52u> prefix_t;
typedef std::array<uint8_t,  32u> hash_t;
typedef std::array<uint8_t,  84u> message_t;
typedef std::array<uint8_t, 200u> state_t;
typedef std::array<uint8_t,  20u> address_t;
typedef std::array<uint8_t,   8u> solution_t;

#endif // !_TYPES_H_
