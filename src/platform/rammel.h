#ifndef _RAMMEL_H_
#define _RAMMEL_H_

typedef unsigned int uint;

#define clamp(v, i, s) ({__typeof__(v) v_ = (v); __typeof__(v) i_ = (i); __typeof__(v) s_ = (s); v_ < i_ ? i_ : v_ > s_ ? s_ : v_;})
#define min(a,b) ({ __typeof__(a) a_=(a); __typeof__(b) b_=(b); a_ < b_ ? a_ : b_;})
#define max(a,b) ({ __typeof__(a) a_=(a); __typeof__(b) b_=(b); a_ > b_ ? a_ : b_;})

static inline float sqrf(float a) {return a*a;}
     

static inline int modulo(int a, int b) {
    a = a % b;
    if (a < 0) {
        a += b;
    }
    return a;
}

#define count_of(a) (sizeof(a)/sizeof(*a))

#endif
