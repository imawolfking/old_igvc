// NOTE: the version of the paper you get from sagepub has sign errors on page 579 eqn 70.
// The second equation should NOT have the minus signs. Not sure if this is corrected in 
// other versions.


#include <iostream>
#include <iomanip>
#include <cmath>

#define WANT_STREAM                  // include.h will get <iostream> and <iomanip>
#define WANT_MATH                    // include.h will get <cmath>
#define WANT_FSTREAM                 // <fstream>
#define WANT_TIME                    // <ctime>

#include "newmatap.h"  //matrix operations
#include "newmatio.h"  // need matrix output routines

#ifdef use_namespace
using namespace NEWMAT;              // access NEWMAT namespace
#endif


#define ALF 1.0e-4   //Ensures sufficient decrease in function value.
#define TOLX 1.0e-7  //Convergence criterion on Δx.
const double PI = 3.14159265;



using std::cout;
using std::endl;
using std::setw;


/* Integrals of the form integral{(s^n)*cos(theta(s)) ds} 
   or integral{ (s^n)*sin(theta(s)) ds }
   where theta(s) the integral of kappa(s), which is a 
   polynomial spiral kappa(s) = a + bs + cs^2 + ...
   These integrals are computed using Simpson's rule with
   numIntervals (even) intervals. The results are stored in 
   the array results, which have size 2*order. The coefficients are
   passed in the array coefficients. Final distance is sf.
   Returns -1 if there is any problem
   The result array contains {x, y, F1, G1, F2, G2, F3, G3, ...}
*/
// TODO: optimize by combining loops 
int ComputeGradientIntegrals(int order, int numIntervals, double results[], int numCoeff, double coefficients[], double sf)
{
    if (numIntervals < 2 || numIntervals % 2 == 1 || order < 0)
        return -1;

    // Weights for Simpsons rule {wi} = {1, 4, 2, 4, 2, 4, 2, 4, 1} etc.
    int *weights = new int[numIntervals+1];
    weights[0] = 1;
    weights[numIntervals] = 1;
    for (int i = 1; i <= numIntervals-1; i+=2)
        weights[i] = 4;
    for (int i = 2; i <= numIntervals-2; i+=2)
        weights[i] = 2;

    // Compute the list of s[i]
    double *s = new double[numIntervals+1];
    double delta_s = sf / numIntervals;
    for (int i = 0; i <= numIntervals; ++i) 
        s[i] = i * delta_s;

    // Compute list of theta[i]
    double *theta = new double[numIntervals+1];
    for (int i = 0; i <= numIntervals; ++i) {
        theta[i] = 0;
        for (int j = 0; j < numCoeff; ++j) {
            double poly = 1;
            for (int k = 0; k < j+1; ++k) {   // compute s[i]^(j+1)
                poly *= s[i];
            }
            theta[i] += coefficients[j] * poly / double(j+1);
        }
    }

    // Compute the list of cos(theta[i]) and sin(theta[i])
    double *f = new double[numIntervals+1];
    double *g = new double[numIntervals+1];
    for (int i = 0; i <= numIntervals; ++i) {
        f[i] = cos(theta[i]);
        g[i] = sin(theta[i]);
    }

    // Compute the results 

    double *cacheX = new double[numIntervals+1];   //these will change as we calculate higher orders
    double *cacheY = new double[numIntervals+1];
    for (int i = 0; i <= numIntervals; ++i) {
        cacheX[i] = weights[i] * f[i];   
        cacheY[i] = weights[i] * g[i];
    }
  
    for (int order_idx = 0; order_idx <= order; ++order_idx) {
        // the sequence of computation is x, y, F1, G1, F2, G2, F3, G3, ...
        int x_idx = 2 * order_idx;
        int y_idx = x_idx + 1;
        results[x_idx] = 0;
        results[y_idx] = 0;
        // use cache to prevent multiplying the same things again and again
        for (int i = 0; i <= numIntervals; ++i) {
            if (order_idx > 0) {    // an extra s for each higher order
               cacheX[i] *= s[i];
               cacheY[i] *= s[i];
            }
            results[x_idx] += cacheX[i];
            results[y_idx] += cacheY[i];
        }
        results[x_idx] *= (delta_s/3.0);
        results[y_idx] *= (delta_s/3.0);
    }

    //print lists
    //for (int i = 0; i <= numIntervals; ++i) {
    //    cout << setw(3) << i 
    //         << setw(3) << weights[i]
    //         << setw(12) << theta[i]
    //         << setw(12) << f[i]
    //         << setw(12) << g[i]
    //         << setw(12) << s[i]
    //         << setw(12) << cacheX[i]
    //         << setw(12) << cacheY[i] 
    //         << endl;
    //}
    //cout << endl;

    //delete everything
    delete weights;
    delete s;
    delete theta;
    delete cacheX, cacheY;
    return 0;
}

/* From Numerical Recipes in C
   Given an n-dimensional point xold[1..n], the value of the function and gradient there, fold
   and g[1..n], and a direction p[1..n], finds a new point x[1..n] along the direction p from
   xold where the function func has decreased “sufficiently.” The new function value is returned
   in f. stpmax is an input quantity that limits the length of the steps so that you do not try to
   evaluate the function in regions where it is undefined or subject to overflow. p is usually the
   Newton direction. The output quantity check is false (0) on a normal exit. It is true (1) when
   x is too close to xold. In a minimization algorithm, this usually signals convergence and can
   be ignored. However, in a zero-finding algorithm the calling program should check whether the
   convergence is spurious. Some "difficult" problems may require double precision in this routine. 
*/
void lnsrch(int n, double xold[], double fold, double g[], double p[], double x[],
            double *f, double stpmax, int *check, double (*func)(double []))
{
    int i;
    double a,alam,alam2,alamin,b,disc,f2,rhs1,rhs2,slope,sum,temp,
        test,tmplam;
    *check=0;
    for (sum=0.0,i=1;i<=n;i++) sum += p[i]*p[i];
    sum=sqrt(sum);
    if (sum > stpmax)
        for (i=1;i<=n;i++) p[i] *= stpmax/sum;    //Scale if attempted step is too big.
    for (slope=0.0,i=1;i<=n;i++)
        slope += g[i]*p[i];
    if (slope >= 0.0) {
        //nrerror("Roundoff problem in lnsrch.");
        *check = 1;
        return;
    }
    test=0.0;    //Compute lambda_min.
    for (i=1;i<=n;i++) {
        temp=fabs(p[i])/fmax(fabs(xold[i]),1.0);
        if (temp > test) test=temp;
    }
    alamin=TOLX/test;
    alam=1.0;     //Always try full Newton step first.
    for (;;) {     //Start of iteration loop.
        for (i=1;i<=n;i++) x[i]=xold[i]+alam*p[i];
        *f=(*func)(x);
        if (alam < alamin) {     /*Convergence on delta_x. For zero finding,
                                 the calling program should
                                 verify the convergence. */
            for (i=1;i<=n;i++) x[i]=xold[i];
            *check=1;
            return;
        } else if (*f <= fold+ALF*alam*slope) return;     //Sufficient function decrease.
        else {     //Backtrack.
            if (alam == 1.0)
                tmplam = -slope/(2.0*(*f-fold-slope));      //First time.
            else {     //Subsequent backtracks.
                rhs1 = *f-fold-alam*slope;
                rhs2=f2-fold-alam2*slope;
                a=(rhs1/(alam*alam)-rhs2/(alam2*alam2))/(alam-alam2);
                b=(-alam2*rhs1/(alam*alam)+alam*rhs2/(alam2*alam2))/(alam-alam2);
                if (a == 0.0) tmplam = -slope/(2.0*b);
                else {
                    disc=b*b-3.0*a*slope;
                    if (disc < 0.0) tmplam=0.5*alam;
                    else if (b <= 0.0) tmplam=(-b+sqrt(disc))/(3.0*a);
                    else tmplam=-slope/(b+sqrt(disc));
                }
                if (tmplam > 0.5*alam)
                    tmplam=0.5*alam;   //lambda <= 0.5 lambda_1.
            }
        }
        alam2=alam;
        f2 = *f;
        alam=fmax(tmplam,0.1*alam);   //lambda >= 0.1 lambda_1.
    }     //Try again.
}


/* Use Newton's method to iteratively find the root of the necessary equation for
   optimality, derived using Lagrange multipliers.
   First array contains 5 boundary values in the order: kappa_0, kappa_f, theta_f, 
   x_f, y_f. We then give it the number of independent coefficients, which is in this
   case 3 less than the total number, since there are 3 algebraic equations as 
   constraints. We also provide it with a guess of the total distance, which can be 
   computed from a low fidelity lookup table or an approximation. If a solution exists,
   they will be returned as the second and third to last args, otherwise we return -1. 
   tolerance is an array of numIndepCoeff+1 small numbers.
*/
int FindRoot(double boundaryVals[], int numIndepCoeff, double indepCoeff_guess[], double s_f_guess, 
             double indepCoeff_soln[], double &s_f_soln, double tolerance[])
{
    double kappa_0 = boundaryVals[0];
    double kappa_f = boundaryVals[1];
    double theta_f = boundaryVals[2];
    double x_f = boundaryVals[3];
    double y_f = boundaryVals[4];

    const int integrationIntervals = 500;    // for forward integration

    // initialize with some large number... These should converge to 0 by the end
    double delta_b = 1E8;
    double delta_s = 1E8;

    // independent variables
    double b = indepCoeff_guess[0];
    double sf = s_f_guess;

    // solvable (dependent) coefficients
    Matrix solveForDep(2, 2);
    ColumnVector dep(2);
    ColumnVector indep(2);
    double a = kappa_0;
    double c;
    double d;
    double results[10];

    // G is set of constraint functions (i.e. G(q) = 0)
    Matrix Jacobian_G(2, 2);
    ColumnVector G(2);
    ColumnVector delta_q(2);

    double x_temp = 1E8;
    double y_temp = 1E8;

    int numIter = 0;
    //TODO: what if it doesn't converge?
    while ((abs(x_temp - x_f) > tolerance[0] || abs(y_temp - y_f) > tolerance[1])) {

        if (abs(b) > 1000)
            return -1;

        numIter++;

        //first solve for dependent coefficients
        solveForDep << sf*sf << sf*sf*sf
                    << sf*sf*sf/3.0 << sf*sf*sf*sf/4.0;
        indep << kappa_f - kappa_0 - b * sf
              << theta_f - kappa_0 * sf - b * sf * sf / 2.0;
        dep = solveForDep.i() * indep;
        c = dep(1);
        d = dep(2);

        int order = 4;
        int numCoeff = 4;
        double coefficients[4] = {a, b, c, d};

        ComputeGradientIntegrals(order, integrationIntervals, results, numCoeff, coefficients, sf);

        // now we can compute the delta's by solve a matrix equation
        double C2 = results[4];
        double S2 = results[5];

        //debug
        x_temp = results[0];
        y_temp = results[1];

        G << results[0] - x_f
          << results[1] - y_f;
        // NOTE: the version of the paper you get from sagepub has sign errors on page 579 eqn 70.
        // The second equation should NOT have the minus signs
        Jacobian_G << -S2/2.0 << cos(theta_f)
                   << C2/2.0 << sin(theta_f);
        delta_q = -Jacobian_G.i() * G;
        delta_b = delta_q(1);
        delta_s = delta_q(2);
        b += delta_b;
        sf += delta_s;
    }

    indepCoeff_soln[0] = b;
    s_f_soln = sf;

    cout << "a = " << a << endl
         << "b = " << b << endl
         << "c = " << c << endl
         << "d = " << d << endl
         << "sf = " << sf << endl
         << numIter << " iterations used" << endl;
        

    return 0;
}

int main()
{

    double boundaryVals[5] = {0, 0, -PI/2.41, 10, 0};
    int numIndepCoeff = 1;
    double indepCoeff_guess[1] = {0.2};
    double s_f_guess = 12.1;
    double tolerance[2] = {0.01, 0.01};

    double indepCoeff_soln[1];
    double s_f_soln;

    FindRoot(boundaryVals, numIndepCoeff, indepCoeff_guess, s_f_guess,
            indepCoeff_soln, s_f_soln, tolerance);
    

    //double results[10];
    ////int order = 5;
    //int numIntervals = 500;
    //int numCoeff = 4;
    //double coefficients[4] = {0, 0.647, -0.242, 0.0205};
    //double sf = 7.72336;

    //ComputeGradientIntegrals(4, numIntervals, results, numCoeff, coefficients, sf);

    //cout << "x = " << results[0] << ", y = " << results[1] << endl;
    //cout << "F1 = " << results[2] << ", G1 = " << results[3] << endl;
    //cout << "F2 = " << results[4] << ", G2 = " << results[5] << endl;
    //cout << "F3 = " << results[6] << ", G3 = " << results[7] << endl;
    //cout << "F4 = " << results[8] << ", G4 = " << results[9] << endl;


    return 0;
}