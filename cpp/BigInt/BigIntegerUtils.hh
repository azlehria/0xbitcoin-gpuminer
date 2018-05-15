#ifndef BIGINTEGERUTILS_H
#define BIGINTEGERUTILS_H

#include "BigUnsigned.hh"
#include <string>
#include <iostream>

/* This file provides:
 * - Convenient std::string <-> BigUnsigned/BigInteger conversion routines
 * - std::ostream << operators for BigUnsigned/BigInteger */

// std::string conversion routines.  Base 10 only.
std::string bigUnsignedToString(const BigUnsigned &x);
BigUnsigned stringToBigUnsigned(const std::string &s);

// Outputs x to os, obeying the flags `dec', `hex', `bin', and `showbase'.
std::ostream &operator <<(std::ostream &os, const BigUnsigned &x);

#endif
