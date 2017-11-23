// Author: Eric Kalosa-Kenyon
//
// This program performs least squares polynomial fitting using BLAS and LAPACK
//
// Takes:
//      d - command line input, degree of polynomial to fit
//      data.dat - x and y values to fit polynomial to
// Returns:
//      StdIO - fit and intermediate results
//
// Sources:
// 1. https://www.cs.bu.edu/teaching/c/file-io/intro/
// 2. http://www.netlib.org/clapack/cblas/dgemm.c
// 3. https://www.math.utah.edu/software/lapack/lapack-blas/dgemm.html
// 4  http://www.math.utah.edu/software/lapack/lapack-d/dpotrf.html
// 5. https://stackoverflow.com/questions/3521209/making-c-code-plot-a-graph-automatically

#include "stdio.h"
#include "stdlib.h"
#include "math.h"
#include "Accelerate/Accelerate.h"
// #include "cblas.h"

// BEGIN PROTOTYPES
void dgemm_(char * transa, char * transb, int * m, int * n, int * k,
    double * alpha, double * A, int * lda,
    double * B, int * ldb, double * beta,
    double *, int * ldc);

void dgemv_(char * trans, int * m, int * n,
    double * alpha, double * A, int * lda,
    double * x, int * incx, double * beta,
    double * y, int * incy);

int dpotrf_(char * uplo, int * n,
    double * A, int * lda, int * info);

void dtrsm_(char * side, char * uplo, char * transa, char * diag,
    int * m, int * n, double * alpha, double * A,
    int * lda, double * B, int * ldb);
// END PROTOTYPES

// BEGIN MAIN
int main(int argc, char** argv)
{

    // initialize constants and variables
    /* const char data_fn[] = "../data/data.dat"; */
    const char data_fn[] = "../data/test.dat";
    int d = atoi(argv[1]); // degree of fit polynomial from commandline
    float x, y; // for reading x and y from data_fn
    int n = 0; // size of the input data (n by 2 i.e. n xs and n ys)

    // read data into memory from disk
    FILE *ifp; // setup variables
    char *mode = "r";
    ifp = fopen(data_fn, mode);
    if (ifp == NULL) {
        fprintf(stderr, "Cannot open file\n"); // error if it can't open
        exit(1);
    }

    // print input contents back out to stdIO and count lines
    while (fscanf(ifp, "%f %f", &x, &y) != EOF) { // for each line in the file
        n++; // count the number of lines
    }
    fclose(ifp);

    // allocate memory for the input data
    double *X = (double*) malloc(n*d* sizeof(double));
    // NOTE: X is col-major indexed i.e. X[i + n*j] = X_(i,j)
    double *Y = (double*) malloc(n* sizeof(double));
    if ((X == NULL) || (Y == NULL)) {
        fprintf(stderr, "malloc failed\n");
        return(1);
    }

    // read input data into the X and Y arrays
    int i = 0, j = 0; // loop counter
    ifp = fopen(data_fn, mode);
    while (fscanf(ifp, "%f %f", &x, &y) != EOF) { // for each line in the file
        for(j = 0; j < d+1; j++){
            X[i + j*n] = pow(x, j); // each col of X is ((x_i)^j)_{i=1..n}, j<=d
        }
        Y[i] = y;
        /* printf("x=%f,\ty=%f\n", x, y); */
        i++;
    }
    fclose(ifp);

    // print the X and Y matrices
    printf("Y\t\t"); // BEGIN formatting printing header
    for(j = 0; j < d; j++){
        printf("X^%d\t\t", j);
    }
    printf("\n"); // END formatting printing header

    for(i = 0; i < n; i++){ // BEGIN print X and Y
        printf("%f\t", Y[i]);
        for(j = 0; j < d; j++){
            printf("%f\t", X[i + j*n]);
        }
        printf("\n");
    } // END print X and Y


    // compute A=X^T X using BLAS::dgemm()
    // initialize
    char TRANSA = 'T'; // transpose the matrix X for (X^T X)
    char TRANSB = 'N';
    double ALPHA = 1.0; // dgemm(A,B,C) : C <- alpha*AB + beta*C
    double BETA = 0.0;
    int M = d; // rows of X^T
    int N = d; // columns of X
    int K = n; // columns of X^T and rows of X
    int LDA = n;
    int LDB = n;
    int LDC = d;

    // allocate memory for A
    double *A = (double*) malloc(d*d* sizeof(double));
    if (A == NULL) {
        fprintf(stderr, "malloc failed\n");
        return(1);
    }

    // compute A = X^T X
    dgemm_(&TRANSA, &TRANSB,
        &M, &N, &K,
        &ALPHA, X, &LDA, X, &LDB, &BETA,
        A, &LDC);

    // print A
    printf("\nA = X^T X\n");
    for(i = 0; i < d; i++){
        for(j = 0; j < d; j++){
            printf("%f\t", A[i + j*d]);
        }
        printf("\n");
    }

    // Compute P = Xt y using BLAS::dgemm()
    // initialize
    char TRANS = 'T'; // transpose the matrix X for (X^T y)
    int LDX = n;
    int INCY = 1; // increment for the input vector
    int INCP = 1; // increment for the input vector

    // allocate memory for P = X^T y
    double *P = (double*) malloc(d* sizeof(double));
    if (P == NULL) {
        fprintf(stderr, "malloc failed\n");
        return(1);
    }

    // compute P = X^T y using dgemv
    dgemv_(&TRANS,
        &M, &K,
        &ALPHA, X, &LDX, Y, &INCY, &BETA,
        P, &INCP);

    // print P
    printf("\nP = X^T y\n");
    for(i = 0; i < d; i++){
        printf("%f\t", P[i]);
    printf("\n");
    }

    // Compute Choelsky decomposition XTX = A = LTL i.e. get L
    // allocate memory for L
    double *L = (double*) malloc(d*d* sizeof(double));
    if (L == NULL) {
        fprintf(stderr, "malloc failed\n");
        return(1);
    }

    // set up L = A for subsequent diagonalization
    for(i=0; i<d*d; i++){
        L[i] = A[i];
    }

    // set up the diagonalization and run it
    char UPLO = 'L'; // lower triangular
    int ORD = d; // L is order d
    int LDD = d; // leading dimension of matrix
    int INFO = 0; // output for diagonalization
    dpotrf_(&UPLO, &ORD, L, &LDD, &INFO);

    if(INFO != 0){
        fprintf(stderr, "Choelsky decomosition failed\n");
        return(1);
    }

    // set the above-diagonal entries to 0 - this is just an aesthetic thing
    for(i=0; i<d; i++){
        for(j=0; j<d; j++){
            if(j>i){
                L[i + d*j] = 0.0;
            }
        }
    }

    // print resultant diagonalized matrix L
    printf("\nL = Chol(A)\n");
    for(i = 0; i < d; i++){
        for(j = 0; j < d; j++){
            printf("%f\t", L[i + j*d]);
        }
        printf("\n");
    }

    // Solve for b (Xb=y) using BLAS::dtrsm()
    //  first, some algebra:
    //  Xb = y -> XtX b = Xt y -> Ab = p -> LLt b = p
    //  let Ltb = q, then solve L q = p using dtrsm_() for q
    //  upon solving for q, solve Ltb = q for b using dtrsm_()

    // set up the first triangular solve for L Q = P and run it
    char SIDE = 'L'; // left, i.e. L q = p rather than q L = p
    UPLO = 'L'; // L is lower triangular
    TRANSA = 'N'; // solving L q = b not Lt
    char DIAG = 'N'; // L is not unit diagonal (using Choelsky not LDL decomp)
    M = d; // rows of resultant vector P
    N = 1; // columns of resultant vector P
    ALPHA = 1.0; // coef for the rhs vector P
    LDA = d; // leading dimension on the lhs multiplier L
    LDB = d; // leading dimension of the rhs vector P

    // allocate a copy of P for the solution Q
    double *Q = (double*) malloc(d* sizeof(double));
    if (Q == NULL) {
        fprintf(stderr, "malloc failed\n");
        return(1);
    }
    for(i=0; i<d; i++){ // copy P -> Q
        Q[i] = P[i];
    }

    dtrsm_(&SIDE, &UPLO, &TRANSA, &DIAG, // calculate Q = L^{-1} P
            &M, &N, &ALPHA, L, &LDA, Q, &LDB);

    // print the solution Q = L^{-1} P
    printf("\nQ = L^{-1} P\n");
    for(i = 0; i < d; i++){
        printf("%f\n", Q[i]);
    }

    // solve the second system
    SIDE = 'L'; // left, i.e. Lt
    UPLO = 'L'; // L is lower triangular
    TRANSA = 'T'; // solving Lt b = q
    DIAG = 'N'; // L is not unit diagonal (using Choelsky not LDL decomp)
    M = d; // rows of rhs vector Q
    N = 1; // columns of rhs vector Q
    ALPHA = 1.0; // coef for the rhs vector Q
    LDA = d; // leading dimension on the lhs multiplier Lt
    LDB = d; // leading dimension of the rhs vector Q

    // allocate a copy of Q for the solution B
    double *B = (double*) malloc(d* sizeof(double));
    if (B == NULL) {
        fprintf(stderr, "malloc failed\n");
        return(1);
    }
    for(i=0; i<d; i++){ // copy B <- Q; Lt B = Q
        B[i] = Q[i]; // B is overwritten with result Lt^{-1}Q
    }

    dtrsm_(&SIDE, &UPLO, &TRANSA, &DIAG, // calculate B = Lt^{-1}Q
            &M, &N, &ALPHA, L, &LDA, B, &LDB);

    // print the solution B = Lt^{-1} Q
    printf("\nB = L^T^{-1} Q\n");
    for(i = 0; i < d; i++){
        printf("%f\n", B[i]);
    }

    // Generate predictions using the fitted polynomial represented by B
    // allocate space for the predictions
    double *Yhat = (double*) malloc(n* sizeof(double));
    if (Yhat == NULL) {
        fprintf(stderr, "malloc failed\n");
        return(1);
    }

    TRANS = 'N'; // do not 'N' transpose the matrix X for (XB = yhat)
    M = n; // X is n rows by d columns
    N = d;
    LDX = n; // leading dimension of X
    ALPHA = 1.0; // coefficient for Y in Y := alpha AX + beta y
    BETA = 0.0;
    int INCB = 1; // increment for the vector B
    INCY = 1; // increment for the resultant vector Yhat

    dgemv_(&TRANS, &M, &N,
        &ALPHA, X, &LDX, B, &INCB, &BETA,
        Yhat, &INCY);

    // print the solution Yhat = XB
    printf("\nYhat = XB\n");
    for(i = 0; i < n; i++){
        printf("%f\n", Yhat[i]);
    }

    // Write output to disk for subsequent analysis
    FILE * out_raw = fopen("poly_raw.dat", "w");
    FILE * out_fit = fopen("poly_fit.dat", "w");
    FILE * out_coef = fopen("poly_coef.dat", "w");
    FILE * out_designMx = fopen("poly_designMx.dat", "w");
    for (i=0; i < n; i++) {
        fprintf(out_raw, "%lf %lf\n", X[i + n], Y[i]);
        fprintf(out_fit, "%lf %lf\n", X[i + n], Yhat[i]);
        for(j=0; j<d; j++){
            fprintf(out_designMx, "%lf ", X[i + j*n]);
        }
        fprintf(out_designMx, "\n");
    }
    for (i=0; i < d; i++){
        fprintf(out_coef, "%lf\n", B[i]);
    }
    fclose(out_raw);
    fclose(out_fit);
    fclose(out_coef);

    // Gnuplot the resulting fit and the raw data

    FILE * gnuplotPipe = popen("gnuplot", "w");
    fprintf(gnuplotPipe, "set terminal jpeg\n");
    fprintf(gnuplotPipe, "set output 'plot.jpeg'\n");

    fprintf(gnuplotPipe, "set grid\n" );
    fprintf(gnuplotPipe, "set title 'Raw observations and fitted polynomial'\n" );
    fprintf(gnuplotPipe, "set xlabel 'X'\n" );
    fprintf(gnuplotPipe, "set ylabel 'Y'\n" );
    fprintf(gnuplotPipe, "set style data points\n" );
    fprintf(gnuplotPipe, "set pointsize 2\n" );
    fprintf(gnuplotPipe, "plot 'poly_raw.dat' title 'Input', ");
    fprintf(gnuplotPipe, "'poly_fit.dat' title 'Fit'\n");

    pclose(gnuplotPipe);

    return 0;
}
// END MAIN
