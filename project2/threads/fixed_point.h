#define F (1 << 14)             /* fixed point 1 */
#define INT_MAX ((1 << 31) - 1) /* (0 11111111111111111 11111111111111)*/
#define INT_MIN (-(1 << 31))    /*  (1 00000000000000000 00000000000000) */

int int_to_fp (int n);
int fp_to_int (int x);
int fp_to_int_round(int x);
int add_fp (int x, int y);
int sub_fp (int x, int y);
int add_fp_int (int x, int n);
int sub_fp_int (int x, int n);
int mult_fp (int x, int y);
int mult_fp_int (int x, int n);
int div_fp (int x, int y);
int div_fp_int (int x, int n);

/* Convert n to fixed point  */
int int_to_fp (int n) {
    return n * F;
}

/* Convert x to integer (rounding toward zero) */
int fp_to_int (int x) {
    return x / F;
}

/* Convert x to integer (rounding to nearst) */
int fp_to_int_round(int x) {
    if (x >= 0) {
        return (x + F / 2) / F ;
    }
    else {
        return (x - F / 2) / F;
    }
}

/* Add x and y  */
int add_fp (int x, int y) {
    return x + y;
}

/* Subtract y from x  */
int sub_fp (int x, int y) {
    return x - y;
}

/* Add x and n  */
int add_fp_int (int x, int n) {
    return x + n * F;
}

/* Subtract n from x  */
int sub_fp_int (int x, int n) {
    return x - n * F;
}

/* Multiply x by y */
int mult_fp (int x, int y) {
    return ((int64_t) x) * y / F;
}

/* Multiply x by n */
int mult_fp_int (int x, int n) {
    return x * n;
}

/* Divide x by y */
int div_fp (int x, int y) {
    return ((int64_t) x) * F / y;
}

/* Divide x by n */
int div_fp_int (int x, int n) {
    return x / n;
}