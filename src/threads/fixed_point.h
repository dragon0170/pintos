#include <stdint.h>

#define f (1 << 14)

int convert_fp (int n) {
  return n * f;
}

int convert_int (int x) {
  return x / f;
}

int convert_int_round (int x) {
  if (x >= 0) 
    return (x + f / 2) / f;
  else 
    return (x - f / 2) / f;
}

int add_ff (int x, int y) {
  return x + y;
}

int sub_ff (int x, int y) {
  return x - y;
}

int add_fi (int x, int n) {
  return x + n * f;
}

int sub_fi (int x, int n) {
  return x - n * f;
}

int mult_ff (int x, int y) {
  return ((int64_t) x) * y / f;
}

int mult_fi (int x, int n) {
  return x * n;
}

int div_ff (int x, int y) {
  return ((int64_t) x) * f / y;
}

int div_fi (int x, int n) {
  return x / n;
}