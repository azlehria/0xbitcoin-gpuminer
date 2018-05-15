#include "BigIntegerUtils.hh"
#include "BigUnsignedInABase.hh"

std::string bigUnsignedToString(const BigUnsigned &x) {
	return std::string(BigUnsignedInABase(x, 10));
}

BigUnsigned stringToBigUnsigned(const std::string &s) {
	return BigUnsigned(BigUnsignedInABase(s, 10));
}

std::ostream &operator <<(std::ostream &os, const BigUnsigned &x) {
	BigUnsignedInABase::Base base;
	long osFlags = os.flags();
	if (osFlags & os.dec)
		base = 10;
	else if (osFlags & os.hex) {
		base = 16;
		if (osFlags & os.showbase)
			os << "0x";
	} else if (osFlags & os.oct) {
		base = 8;
		if (osFlags & os.showbase)
			os << '0';
	} else
		throw "std::ostream << BigUnsigned: Could not determine the desired base from output-stream flags";
	std::string s = std::string(BigUnsignedInABase(x, base));
	os << s;
	return os;
}
