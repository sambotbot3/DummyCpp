#ifndef DPP_TEST_HEADER_VALUE_H
#define DPP_TEST_HEADER_VALUE_H

class HeaderValue {
public:
  int value;
  HeaderValue(int start) : value(start) {}
  int tag() const { return value; }
};

#endif
