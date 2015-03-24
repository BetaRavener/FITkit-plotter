#include <stdint.h>

// convert (x,y) to d
int32_t Hilbert_xy2d (int32_t n, int32_t x, int32_t y);
// convert d to (x,y)
void Hilbert_d2xy(int32_t n, int32_t d, int32_t *x, int32_t *y);
// returns length of Hilbert line
int32_t Hilbert_length(int32_t n);
// convert recursion number (r) to number of tiles in dimension (n) 
int32_t Hilbert_r2n(int32_t r);
// convert number of tiles in dimension (n) to recursion number (r)
int32_t Hilbert_n2r(int32_t n);
// Image size from number of tiles and step size
double Hilbert_n2size(int32_t n, double stepSize);
// Step size from image size and number of tiles
double Hilbert_n2step(int32_t n, double size);
