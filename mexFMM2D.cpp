// A simplefunction.cpp that takes 3 arguments
// 1) A function handle  (kernel of Q)
// 2) An integer value M (dimension of Q)
// 3) An integer value N (number of columns of H)
// 4) A matrix U of size MxN
// 5) A vector of location x of Mx1
// 6) A vector of location y of Nx1
// and returns the output of the product QU
// Usage:
//mex('-largeArrayDims','-I/Users/yueli/Dropbox/Matlab/FastKF/Final_code/mexFMM/eigen','gatewayfunction.cpp')

#include <iostream>
#include "math.h"
#include "mex.h"
#include "matrix.h"
#include "environment.hpp"
#include "BBFMM2D.hpp"
#include <Eigen/Core>
// include source file

using namespace Eigen;
using namespace std;
double pi 	=	4.0*atan(1);
extern void _main();

/*! Define user's own kernel */
class myKernel: public kernel_Base {
public:
    virtual double kernel_Func(Point r0, Point r1){
        //implement your own kernel here
        double rSquare  =   (r0.x-r1.x)*(r0.x-r1.x) + (r0.y-r1.y)*(r0.y-r1.y);
        return exp(-pow(pow(rSquare,0.5)/900.0,0.5));
    }
};

// Pass location from matlab to C
void read_location(const mxArray* x, const mxArray* y, vector<Point>& location){
    unsigned long N;
    double *xp, *yp;
    N = mxGetM(x);
    xp = mxGetPr(x);
    yp = mxGetPr(y);
    for (unsigned long i = 0; i < N; i++){
        Point new_Point;
        new_Point.x = xp[i];
        new_Point.y = yp[i];
        location.push_back(new_Point);
    }
}

void mexFunction(int nlhs,mxArray *plhs[], int nrhs, const mxArray *prhs[]) {
    // Macros for the output and input arguments
    #define Q_OUT           plhs[0]
    #define QHexact_OUT     plhs[1]
    #define QH_OUT          plhs[2]
    #define kernel          prhs[0]
    #define x_IN            prhs[1]
    #define y_IN            prhs[2]
    #define H_IN            prhs[3]

    unsigned long N;
    unsigned m;
    
    // Argument Checking:
//   // First Argument should be a Function Handle
    if( !mxIsClass( kernel , "function_handle")) {
        mexErrMsgTxt("First input argument is not a function handle.");
    }
    
//   // Second Argument is a Double Scalar
//   if (!mxIsClass(prhs[1], "double")||(mxGetM(prhs[1])>1)||(mxGetN(prhs[1])>1)) {
//     mexErrMsgTxt("Second input argument is not a double scalar.");
//   }
    // First and Second Argument are integer
    
    if( !mxIsDouble(x_IN)) {
        mexErrMsgTxt("Third input argument is not a real 2D full double array.");
    }
    if( !mxIsDouble(y_IN)) {
        mexErrMsgTxt("Third input argument is not a real 2D full double array.");
    }
    if( !mxIsDouble(H_IN)) {
        mexErrMsgTxt("Third input argument is not a real 2D full double array.");
    }
    
    //processing on input arguments
    N = mxGetM(H_IN); // get the first dimension of H
    m = mxGetN(H_IN); // get the second dimension of H
    vector<Point> location;
    read_location(x_IN,y_IN,location);
    double *charges;
    charges = mxGetPr(H_IN);
    // Load data to local array using Eigen <Map>
    MatrixXd H = Map<MatrixXd>(charges, N, m);
    // Map<MatrixXd> H(charges,N,m);

    // Compute Fast matrix vector product
    // 1. Build Tree
    clock_t startBuild  = clock();
    unsigned short nChebNodes = 5;
    H2_2D_Tree Atree(nChebNodes, charges, location, N, m); //Build the fmm tree
    clock_t endBuild = clock();

    double FMMTotalTimeBuild = double(endBuild-startBuild)/double(CLOCKS_PER_SEC);
    cout << endl << "Total time taken for FMM(build tree) is:" << FMMTotalTimeBuild <<endl;

    // Calculateing potential
    clock_t startA = clock();
    QH_OUT = mxCreateNumericMatrix(0, 0, mxDOUBLE_CLASS, mxREAL);     // Create an uninitialized numeric array for dynamic memory allocation
    double *QHp;
    QHp = (double *) mxMalloc(N * m * sizeof(double));
    myKernel A;
    A.calculate_Potential(Atree, QHp);
    clock_t endA = clock();
    double FMMTotalTimeA = double(endA-startA)/double(CLOCKS_PER_SEC);
    cout << endl << "Total time taken for FMM(calculating potential) is:" << FMMTotalTimeA <<endl;

    // Compute exact covariance Q
    cout << endl << "Starting Exact computation..." << endl;
    clock_t start = clock();
    Q_OUT = mxCreateNumericMatrix(0, 0, mxDOUBLE_CLASS, mxREAL);
    double *Qp;
    Qp = (double *) mxMalloc(N*N*sizeof(double));
    MatrixXd Q = Map<MatrixXd>(Qp, N, N);
    A.kernel_2D(N, location, N, location, Q);
    // ckernel_2D(N,location,N,location,Q);
    clock_t end = clock();
    double exactAssemblyTime = double(end-start)/double(CLOCKS_PER_SEC);
    
    // Compute exact Matrix vector product
    start = clock();
    QHexact_OUT = mxCreateNumericMatrix(0, 0, mxDOUBLE_CLASS, mxREAL);
    double *QHexactp;
    QHexactp = (double *) mxMalloc(N * m * sizeof(double));
    Map<MatrixXd> QHT(QHexactp,N,m);
    QHT = Q*H;
    end = clock();
    double exactComputingTime = double(end-start)/double(CLOCKS_PER_SEC);

    cout << "the total computation time is " << exactAssemblyTime + exactComputingTime <<endl;
    // Put the C array into the mxArray and define its dimension
    mxSetPr(Q_OUT,Qp); // Qp must be initialiezed using mxMalloc
    mxSetM(Q_OUT,N);
    mxSetN(Q_OUT,N);
    mxSetPr(QHexact_OUT,QHexactp);
    mxSetM(QHexact_OUT,N);
    mxSetN(QHexact_OUT,m);
    mxSetPr(QH_OUT,QHp);
    mxSetM(QH_OUT,N);
    mxSetN(QH_OUT,m);

    return;
}