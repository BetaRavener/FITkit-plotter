/*******************************************************************************
   hilbert: Algorithms for calculation of points for Hilbert's curve.
   Author(s): Ivan Sevcik <xsevci50 AT stud.fit.vutbr.cz>
              code from wikipedia
*******************************************************************************/
#include "hilbert.h"
// <Copyright> Code taken from http://en.wikipedia.org/wiki/Hilbert_curve

//rotate/flip a quadrant appropriately
void rot(int32_t n, int32_t *x, int32_t *y, int32_t rx, int32_t ry) {
    if (ry == 0) {
        if (rx == 1) {
            *x = n-1 - *x;
            *y = n-1 - *y;
        }
 
        //Swap x and y
        int32_t t  = *x;
        *x = *y;
        *y = t;
    }
}

//convert (x,y) to d
int32_t Hilbert_xy2d (int32_t n, int32_t x, int32_t y) {
    int32_t rx, ry, s, d=0;
    for (s=n/2; s>0; s/=2) {
        rx = (x & s) > 0;
        ry = (y & s) > 0;
        d += s * s * ((3 * rx) ^ ry);
        rot(s, &x, &y, rx, ry);
    }
    return d;
}
 
//convert d to (x,y)
void Hilbert_d2xy(int32_t n, int32_t d, int32_t *x, int32_t *y) {
    int32_t rx, ry, s, t=d;
    *x = *y = 0;
    for (s=1; s<n; s*=2) {
        rx = 1 & (t/2);
        ry = 1 & (t ^ rx);
        rot(s, x, y, rx, ry);
        *x += s * rx;
        *y += s * ry;
        t /= 4;
    }
}
// </Copyright>

// Log2(x), if not integer, lower controls whether lower or higher result is returned
int32_t log2(int32_t x, uint8_t lower)
{
    if (x <= 0)
        return -1;

    int32_t a = 1;
    int32_t r = 0;
    
    while (a < x)
    {
        a = a << 1;
        r++;
    }
    
    if (a != x && lower)
        return r - 1;
    
    return r;
}

int32_t pow2(int32_t n)
{
    if (n < 0)
        return -1;

    int32_t r = 1;
    int32_t c = 0;
    
    while (c < n)
    {
        r = r << 1;
        c++;
    }
    
    return r;
}

// returns length of Hilbert line
int32_t Hilbert_length(int32_t n)
{
    return n*n;
}

// convert recursion number (r) to number of tiles in dimension (n) 
int32_t Hilbert_r2n(int32_t r)
{
    return pow2(r);
}

// convert number of tiles in dimension (n) to recursion number (r)
int32_t Hilbert_n2r(int32_t n)
{
    return log2(n, 1);
}

// Image size from number of tiles and step size
double Hilbert_n2size(int32_t n, double stepSize)
{
    return n * stepSize;
}

// Step size from image size and number of tiles
double Hilbert_n2step(int32_t n, double size)
{
    return size / n;
}
