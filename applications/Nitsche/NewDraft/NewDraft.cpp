#include <iostream>
#include <iomanip>
#include <vector>
#include <math.h>
#include <ctime>
#include <eigen3/Eigen/Dense>
#include <eigen3/unsupported/Eigen/KroneckerProduct>
#include </usr/include/eigen3/Eigen/src/Core/util/DisableStupidWarnings.h>
#include <eigen3/unsupported/Eigen/CXX11/Tensor>
#include <fstream>
#include<cmath>

// valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all ./executable
//


void PrintMat(std::vector< std::vector<double> >& M);
void PrintVec(std::vector<double>& v);


double a0, a1, a3, a5, a7, a9;
double get_g(const double &r, const double &T, const unsigned &n) ;
double get_r(const double &T, const unsigned &n);
void InitParticlesDisk(const unsigned &dim, const unsigned &ng, double &xL, double &xR, double &yL, double &yR,
                       const std::vector < double> &xc, const double & R, std::vector < std::vector <double> > &xp,
                       std::vector <double> &wp, std::vector <double> &dist);

void InitParticlesDiskOld(const unsigned &dim, const unsigned &ng, const double &eps, const unsigned &nbl, const bool &gradedbl,
                          const double &a, const double &b, const std::vector < double> &xc, const double & R,
                          std::vector < std::vector <double> > &xp, std::vector <double> &wp, std::vector <double> &dist);


double GetDistance(const Eigen::VectorXd &x);
void GetGaussPointsWeights(unsigned &N, Eigen::VectorXd &xg, Eigen::VectorXd &wg);

void Cheb(const unsigned & m, Eigen::VectorXd &xg, Eigen::MatrixXd &C);
void GetParticlesOnBox(const double & a, const double & b, const unsigned & n1, const unsigned& dim, Eigen::MatrixXd &x, Eigen::MatrixXd &xL);
void AssembleMatEigen(std::vector<double>& VxL, std::vector<double> &VxR, const unsigned & m, const unsigned & dim, const unsigned & np, Eigen::Tensor<double, 3, Eigen::RowMajor>  &PmX, Eigen::MatrixXd & Pg,  Eigen::VectorXd & wg, Eigen::MatrixXd & A, Eigen::VectorXd & F);
void SolWeightEigen(Eigen::MatrixXd &A, Eigen::VectorXd &F, Eigen::VectorXd &wp, Eigen::VectorXd &w_new);
void GetChebXInfo(const unsigned& m, const unsigned& dim, const unsigned& np, Eigen::MatrixXd &xL, Eigen::Tensor<double, 3, Eigen::RowMajor>& PmX);
void Testing(double &a, double &b, const unsigned &m, const unsigned &dim, Eigen::MatrixXd &x,
             Eigen::VectorXd &w_new, std::vector<double> &dist, const double &eps, double &QuadSum, double &IntSum);

void PrintMarkers(const unsigned &dim, const Eigen::MatrixXd &xP, const std::vector <double> &dist,
                  const Eigen::VectorXd wP, const Eigen::VectorXd &w_new, const unsigned & l, const unsigned & t);

void InitParticlesDisk3D(const unsigned &dim, const unsigned &ng, std::vector<double> &VxL, std::vector<double> &VxR,
                         const std::vector < double> &xc, const double & R, std::vector < std::vector <double> > &xp,
                         std::vector <double> &wp, std::vector <double> &dist);
int main(int argc, char** args) {
  unsigned dim = 3;
  unsigned NG = 5; // Gauss points
  unsigned m = 2; // Polynomial degree
  double eps = 0.001; // width of transition region
  unsigned nbl = 1; // number of bands on boundary
  bool gradedbl = false; // nobody knows what???


  double a = 0.;
  double b = 1.;
  unsigned lmax = 3; // number of refinements
  std::vector<double> EQ; // quadrature errors
  std::vector<double> EI; // integral errors
  EQ.resize(lmax + 1);
  EI.resize(lmax + 1);

  double QuadExact = pow((pow(b, m + 1) - pow(a, m + 1)) / (m + 1), dim);
  double IntExact2D = M_PI * 0.75 * 0.75 * 0.75 * 0.75 / 8.;
  double IntExact3D = 0.04970097753; // int(int(int((R^2 - rho^2)*rho^2*sin(phi), rho = 0 .. R), phi = 0 .. Pi/2), theta = 0 .. Pi/2)
  std::vector <double> IntExact = {IntExact2D, IntExact3D};
  double QuadSum; // quadrarture sum at each refinement level
  double IntSum; // integral sum at each refinement level


  for(unsigned l = 0; l < lmax + 1 ; l++) {
    unsigned NT = 0;

    std::vector<double> VxL(dim);
    std::vector<double> VxR(dim);

    std::clock_t c_start = std::clock();
    double Qsum = 0.; // quadrature sum at the subrectangles of refinement l
    double Isum = 0.; // integral sum at the subrectangles of refinement l
    double h = (b - a) / pow(2, l);
    std::vector<double> x1D(pow(2, l) + 1); // nodes in one direction at level l;
    for(unsigned i = 0; i < x1D.size(); i++) {
      x1D[i] = a + i * h;
    }

    std::vector<unsigned> I(dim);
    std::vector<unsigned> N(dim);
    for(unsigned k = 0; k < dim ; k++) {
      N[k] = pow(2, l * (dim - k - 1));
    }

    std::vector < std::vector <double> > xp;
    std::vector <double> wp;
    std::vector< double > dist;
    for(unsigned t = 0; t < pow(2, dim * l) ; t++) {
      I[0] = t / N[0];
      for(unsigned k = 1; k < dim ; k++) {
        unsigned pk = t % N[k - 1];
        I[k] = pk / N[k];
      }
      //std::cout << I[0] << " " << I[0] + 1 <<  " " << I[1] << " " << I[1]+1 <<std::endl;
      // x and y bounds of the corresponding rectangle
//       double xL = x1D[I[0]];
//       double xR = x1D[I[0] + 1];
//       double yL = x1D[I[1]];
//       double yR = x1D[I[1] + 1];

      for(unsigned k = 0; k < dim; k++) {
        VxL[k] = x1D[I[k]];
        VxR[k] = x1D[I[k] + 1];
      }

      InitParticlesDisk3D(dim, NG, VxL, VxR, {0., 0., 0.}, 0.75, xp, wp, dist);
      //InitParticlesDisk(dim, NG, xL, xR, yL, yR,{0.,0.,0.}, 0.75, xp,wp, dist);


      Eigen::VectorXd wP = Eigen::VectorXd::Map(&wp[0], wp.size());
      Eigen::MatrixXd xP(xp.size(), xp[0].size());
      for(int i = 0; i < xp.size(); ++i) {
        xP.row(i) = Eigen::VectorXd::Map(&xp[i][0], xp[0].size());
      }
      NT += xP.row(0).size();
      //std::cout << " NP = " << xP.row(0).size() << std::endl;

//
//
      unsigned nq = xP.cols();
      Eigen::MatrixXd xI;
      xI.resize(dim, nq);

      for(unsigned k = 0; k < dim; k++) {
        for(unsigned j = 0; j < nq ; j++) {
          xI(k, j) = (2. / (VxR[k] - VxL[k])) * xP(k, j) - ((VxR[k] + VxL[k]) / (VxR[k] - VxL[k])) ;
        }
      }


      Eigen::Tensor<double, 3, Eigen::RowMajor> PmX;
      GetChebXInfo(m, dim, nq, xI, PmX);

      Eigen::VectorXd xg;
      Eigen::VectorXd wg;
      GetGaussPointsWeights(NG, xg, wg);
      Eigen::MatrixXd Pg;
      Cheb(m, xg, Pg);

      Eigen::MatrixXd A;
      Eigen::VectorXd F;

      AssembleMatEigen(VxL, VxR, m, dim, nq, PmX, Pg,  wg, A, F);

      Eigen::VectorXd w_new;
      SolWeightEigen(A, F, wP, w_new);

      PrintMarkers(dim, xP, dist, wP, w_new, l, t);
      
      Testing(a, b, m, dim, xP, w_new, dist, eps, QuadSum, IntSum);
      Qsum += QuadSum;
      Isum += IntSum;
    }
    EQ[l] = fabs((Qsum - QuadExact) / QuadExact); // relative quadrarture error at level l
    EI[l] = fabs((Isum - IntExact[dim - 2]) / IntExact[dim - 2]); // relative integral error at level l

    std::cout << "Time at level" << l << " = " << 1000. * (clock() - c_start) / CLOCKS_PER_SEC << std::endl;
    std::cout << "TotalPoints at  "<< "Level " << l << " = "<< NT << std::endl;
    std::cout << std::endl;
    
  }

  std::cout << "Quadrature_Errors: " ;
  PrintVec(EQ);
  std::cout << "\nHeaviside_Errors: ";
  PrintVec(EI);

  for(unsigned j = 0; j < EQ.size() - 1 ; j++) {
    double c2 = fabs(log(EI[0] / EI[j + 1]) / log(pow(2, j + 1)));
    std::cout << "Refinement " << j << " Integral_Convergance_Rate: " << c2 << std::endl;
    std::cout << "\n" << std::endl;
  }

  return 0;
}



void Testing(double & a, double & b, const unsigned & m, const unsigned & dim, Eigen::MatrixXd & x,
             Eigen::VectorXd & w_new, std::vector<double> &dist, const double & eps, double & QuadSum, double & IntSum) {


  double deps = (b - a) * eps; // eps1

  double a0 = 0.5; // 128./256.;
  double a1 = pow(deps, -1.) * 1.23046875; // 315/256.;
  double a3 = -pow(deps, -3.) * 1.640625; //420./256.;
  double a5 = pow(deps, -5.) * 1.4765625; // 378./256.;
  double a7 = -pow(deps, -7.) * 0.703125; // 180./256.;
  double a9 = pow(deps, -9.) * 0.13671875; // 35./256.;

  QuadSum = 0.;
  IntSum = 0.;

  for(unsigned i = 0; i < w_new.size(); i++) {

    double dg1 = dist[i];
    double dg2 = dg1 * dg1;
    double xi;
    if(dg1 < -deps)
      xi = 0.;
    else if(dg1 > deps) {
      xi = 1.;
    }
    else {
      xi = (a0 + dg1 * (a1 + dg2 * (a3 + dg2 * (a5 + dg2 * (a7 + dg2 * a9)))));
    }

    double r = 0.75 * 0.75 ;
    for(unsigned k = 0; k < dim; k++) {
      r +=  - x(k, i) * x(k, i);
    }
    IntSum += xi * r * w_new(i);

    r = 1.;
    for(unsigned k = 0; k < dim; k++) {
      r *=  x(k, i);
    }
    QuadSum += pow(r, m) * w_new(i);
  }

}




void InitParticlesDisk3D(const unsigned & dim, const unsigned & ng, std::vector<double> &VxL, std::vector<double> &VxR,
                         const std::vector < double> &xc, const double & R, std::vector < std::vector <double> > &xp,
                         std::vector <double> &wp, std::vector <double> &dist) {
//  VxL = {xL,yL,zL};
//  VxR = {xR,yR,zR};


  unsigned m1 = ceil(2 * ng - 1);

  double theta0 = atan2(VxL[1] - xc[1], VxR[0] - xc[0]);
  double theta1 = atan2(VxR[1] - xc[1], VxL[0] - xc[0]);
  if(theta0 < 0) theta0 += M_PI;
  if(theta1 < 0) theta1 += M_PI;


  double phi1 = M_PI / 2 - atan2(VxL[2] - xc[2], sqrt((VxR[0] - xc[0]) * (VxR[0] - xc[0]) + (VxR[1] - xc[1]) * (VxR[1] - xc[1])));
  double phi0 = M_PI / 2 - atan2(VxR[2] - xc[2], sqrt((VxL[0] - xc[0]) * (VxL[0] - xc[0]) + (VxL[1] - xc[1]) * (VxL[1] - xc[1])));
  if(phi0 < 0) phi0 += M_PI;
  if(phi1 < 0) phi1 += M_PI;

  double R0 = 0. ;
  double R1 = 0. ;
  double dp = 1.;


  for(unsigned k = 0; k < dim; k++) {
    R0 += (VxL[k] - xc[k]) * (VxL[k] - xc[k]);
    R1 += (VxR[k] - xc[k]) * (VxR[k] - xc[k]);
    dp *= VxR[k] - VxL[k];
  }

  R0 = sqrt(R0);
  R1 = sqrt(R1);
  dp = pow(dp, 1. / dim) / m1;


  //std::cout << "theta0 = " << theta0 << " theta1 = " << theta1 << " phi0 = " << phi0 << " phi1 = " << phi1 <<  " R0 = " << R0 <<  " R1 = " << R1 << std::endl;


  unsigned nr = ceil(((R - 0.5 * dp)) / dp);
  double dr = ((R - 0.5 * dp)) / nr;
  double area = 0.;

  xp.resize(dim);
  for(unsigned k = 0; k < dim; k++) {
    xp[k].reserve(2 * pow(m1, dim));
  }
  wp.reserve(2 * pow(m1, dim));
  dist.reserve(2 * pow(m1, dim));
  unsigned cnt = 0;

  int i0 = floor(R0 / dr - 0.5);
  if(i0 < 0) i0 = 0;
  //int i1 = ceil(R1 / dr - 0.5);
  int i1 = ceil((R - 0.5 * dp) / dr - 0.5);
  if(i1 > nr - 1) i1 = nr - 1;

  //std::cout << "i0 = " << i0 << " i1 = " << i1 <<std::endl;

  std::vector<double> XP(dim);

  unsigned c = 0;
  for(unsigned i = i0; i <= i1; i++) {
    c++;
    double ri = (i + 0.5) * dr;
    if(dim == 2) {
      //unsigned nti = ceil(2 * M_PI * ri / dr);
      //double dti = 2 * M_PI / nti;
      double dti = dr / ri;
      int j0 = floor(theta0 / dti);
      int j1 = ceil(theta1 / dti);
      for(unsigned j = j0; j <= j1; j++) {
        double tj = j * dti;
        XP[0] = xc[0] + ri * cos(tj);
        XP[1] = xc[1] + ri * sin(tj);
        unsigned flag  = dim;
        for(unsigned k = 0; k < dim; k++) {
          if(VxL[k] < XP[k]  && XP[k] < VxR[k]) {
            --flag;
          }
        }
        if(!flag) {

          for(unsigned k = 0; k < dim; k++) {
            xp[k].resize(cnt + 1);
          }
          wp.resize(cnt + 1);
          dist.resize(cnt + 1);

          xp[0][cnt] = XP[0];
          xp[1][cnt] = XP[1];
          
          //std::cout << xp[0][cnt] << " " << xp[1][cnt] << std::endl;
          
          wp[cnt] = ri * dti * dr;
          dist[cnt] = (R - ri);
          area += ri * dti * dr;
          cnt++;
        }
      }
    }
    else {

      double dphi = dr / ri;
      int k0 = floor(phi0 / dphi);
      int k1 = ceil(phi1 / dphi);

      for(unsigned k = k0; k <= k1; k++) {
        double pk = (k + 0.5) * dphi;

        double dti = dr / (ri * sin(pk));
        int j0 = floor(theta0 / dti);
        int j1 = ceil(theta1 / dti);

        for(unsigned j = j0; j <= j1; j++) {
          double tj = j * dti;
          XP[0] = xc[0] + ri * sin(pk) * cos(tj);
          XP[1] = xc[1] + ri * sin(pk) * sin(tj);
          XP[2] = xc[2] + ri * cos(pk);

          unsigned flag  = dim;
          for(unsigned k = 0; k < dim; k++) {
            if(VxL[k] <= XP[k]  && XP[k] <= VxR[k]) {
              --flag;
            }
          }

          if(!flag) {
            for(unsigned k = 0; k < dim; k++) {
              xp[k].resize(cnt + 1);
            }
            wp.resize(cnt + 1);
            dist.resize(cnt + 1);

            xp[0][cnt] = XP[0];
            xp[1][cnt] = XP[1];
            xp[2][cnt] = XP[2];

            //std::cout << xp[0][cnt] << " " << xp[1][cnt] << " " << xp[2][cnt] << std::endl;

            wp[cnt] = ri * ri * sin(pk) * dr * dphi * dti;
            dist[cnt] = (R - ri);

            area += ri * ri * sin(pk) * dr * dphi * dti;  // fix the volume
            cnt++;
          }
        }

      }
    }
  }

  double ri = R;
  if(dim == 2) {
    double dti = dr / ri;
    int j0 = floor(theta0 / dti);
    int j1 = ceil(theta1 / dti);
    for(unsigned j = j0; j <= j1; j++) {
      double tj = j * dti;
      XP[0] = xc[0] + ri * cos(tj);
      XP[1] = xc[1] + ri * sin(tj);
      unsigned flag  = dim;
      for(unsigned k = 0; k < dim; k++) {
        if(VxL[k] < XP[k]  && XP[k] < VxR[k]) {
          --flag;
        }
      }
      if(!flag) {

        for(unsigned k = 0; k < dim; k++) {
          xp[k].resize(cnt + 1);
        }
        wp.resize(cnt + 1);
        dist.resize(cnt + 1);

        xp[0][cnt] = XP[0];
        xp[1][cnt] = XP[1];
        
        //std::cout << xp[0][cnt] << " " << xp[1][cnt] << std::endl;
        
        wp[cnt] = ri * dti * dr;
        dist[cnt] = (R - ri);
        area += ri * dti * dr;
        cnt++;
      }
    }
  }
  else {
    double dphi = dr / ri;
    int k0 = floor(phi0 / dphi);
    int k1 = ceil(phi1 / dphi);

    for(unsigned k = k0; k <= k1; k++) {
      double pk = (k + 0.5) * dphi;

      double dti = dr / (ri * sin(pk));
      int j0 = floor(theta0 / dti);
      int j1 = ceil(theta1 / dti);

      for(unsigned j = j0; j <= j1; j++) {
        double tj = j * dti;
        XP[0] = xc[0] + ri * sin(pk) * cos(tj);
        XP[1] = xc[1] + ri * sin(pk) * sin(tj);
        XP[2] = xc[2] + ri * cos(pk);

        unsigned flag  = dim;
        for(unsigned k = 0; k < dim; k++) {
          if(VxL[k] <= XP[k]  && XP[k] <= VxR[k]) {
            --flag;
          }
        }

        if(!flag) {
          for(unsigned k = 0; k < dim; k++) {
            xp[k].resize(cnt + 1);
          }
          wp.resize(cnt + 1);
          dist.resize(cnt + 1);

          xp[0][cnt] = XP[0];
          xp[1][cnt] = XP[1];
          xp[2][cnt] = XP[2];

          //std::cout << xp[0][cnt] << " " << xp[1][cnt] << " " << xp[2][cnt] << std::endl;

          wp[cnt] = ri * ri * sin(pk) * dr * dphi * dti;
          dist[cnt] = (R - ri);

          area += ri * ri * sin(pk) * dr * dphi * dti;  // fix the volume
          cnt++;
        }
      }

    }
  }


  i0 = floor((R0 - (R + 0.5 * dp)) / dr - 0.5);
  if(i0 < 0) i0 = 0;

  i1 = floor((R1 - (R + 0.5 * dp)) / dr - 0.5);
  if(i1 > nr - 1) i1 = nr - 1;


  for(unsigned i = i0; i <= i1; i++) {
    c++;
    double ri = (R + 0.5 * dp) + (i + 0.5) * dr;
    if(dim == 2) {
      double dti = dr / ri;
      int j0 = floor(theta0 / dti);
      int j1 = ceil(theta1 / dti);
      for(unsigned j = j0; j <= j1; j++) {
        double tj = j * dti;
        XP[0] = xc[0] + ri * cos(tj);
        XP[1] = xc[1] + ri * sin(tj);
        unsigned flag  = dim;
        for(unsigned k = 0; k < dim; k++) {
          if(VxL[k] < XP[k]  && XP[k] < VxR[k]) {
            --flag;
          }
        }
        if(!flag) {

          for(unsigned k = 0; k < dim; k++) {
            xp[k].resize(cnt + 1);
          }
          wp.resize(cnt + 1);
          dist.resize(cnt + 1);

          xp[0][cnt] = XP[0];
          xp[1][cnt] = XP[1];
          
          //std::cout << xp[0][cnt] << " " << xp[1][cnt] << std::endl;
          
          wp[cnt] = ri * dti * dr;
          dist[cnt] = (R - ri);

          area += ri * dti * dr;
          cnt++;
        }
      }
    }
    else {
     
      double dphi = dr / ri;
      int k0 = floor(phi0 / dphi);
      int k1 = ceil(phi1 / dphi);

      for(unsigned k = k0; k <= k1; k++) {
        double pk = (k + 0.5) * dphi;
        //unsigned nti = ceil(2 * M_PI * pk / dphi);
        //double dti =  M_PI / nti;
        double dti = dr / (ri * sin(pk));
        int j0 = floor(theta0 / dti);
        int j1 = ceil(theta1 / dti);

        for(unsigned j = j0; j <= j1; j++) {
          double tj = j * dti;
          XP[0] = xc[0] + ri * sin(pk) * cos(tj);
          XP[1] = xc[1] + ri * sin(pk) * sin(tj);
          XP[2] = xc[2] + ri * cos(pk);

          unsigned flag  = dim;
          for(unsigned k = 0; k < dim; k++) {
            if(VxL[k] <= XP[k]  && XP[k] <= VxR[k]) {
              --flag;
            }
          }

          if(!flag) {
            for(unsigned k = 0; k < dim; k++) {
              xp[k].resize(cnt + 1);
            }
            wp.resize(cnt + 1);
            dist.resize(cnt + 1);

            xp[0][cnt] = XP[0];
            xp[1][cnt] = XP[1];
            xp[2][cnt] = XP[2];
            
            //std::cout << xp[0][cnt] << " " << xp[1][cnt] << " " << xp[2][cnt] << std::endl;
            
            wp[cnt] = ri * ri * sin(pk) * dr * dphi * dti;
            dist[cnt] = (R - ri);

            area += ri * ri * sin(pk) * dr * dphi * dti;  // fix the volume
            cnt++;
          }
        }

      }


    }

  }


}





void GetChebXInfo(const unsigned & m, const unsigned & dim, const unsigned & np, Eigen::MatrixXd & xL, Eigen::Tensor<double, 3, Eigen::RowMajor>& PmX) {

  PmX.resize(dim, m + 1, np);
  Eigen::MatrixXd Ptemp;
  Eigen::VectorXd xtemp;
  for(unsigned k = 0; k < dim; k++) {
    xtemp = xL.row(k);
    Cheb(m, xtemp, Ptemp);
    for(unsigned i = 0; i < m + 1; i++) {
      for(unsigned j = 0; j < np; j++) {
        PmX(k, i, j) = Ptemp(i, j);
      }
    }
  }
}


void AssembleMatEigen(std::vector<double>& VxL, std::vector<double> &VxR, const unsigned & m, const unsigned & dim, const unsigned & np, Eigen::Tensor<double, 3, Eigen::RowMajor>  &PmX, Eigen::MatrixXd & Pg,  Eigen::VectorXd & wg, Eigen::MatrixXd & A, Eigen::VectorXd & F) {


  A.resize(pow(m + 1, dim), np);
  F.resize(pow(m + 1, dim));
  Eigen::VectorXi I(dim);
  Eigen::VectorXi N(dim);


  for(unsigned k = 0; k < dim ; k++) {
    N(k) = pow(m + 1, dim - k - 1);
  }

  for(unsigned t = 0; t < pow(m + 1, dim) ; t++) { // multidimensional index on the space of polynomaials
    I(0) = t / N(0);
    for(unsigned k = 1; k < dim ; k++) {
      unsigned pk = t % N(k - 1);
      I(k) = pk / N(k); // dimensional index over on the space of polynomaials
    }
    for(unsigned j = 0; j < np; j++) {
      double r = 1;

      for(unsigned k = 0; k < dim; k++) {
        r *= PmX(k, I[k], j);
      }
      A(t, j) = r ;
    }

  }

  unsigned ng = Pg.row(0).size();
  Eigen::VectorXi J(dim);
  Eigen::VectorXi NG(dim);



  for(unsigned k = 0; k < dim ; k++) {
    NG(k) = pow(ng, dim - k - 1);
  }

  for(unsigned t = 0; t < pow(m + 1, dim) ; t++) { // multidimensional index on the space of polynomaials
    I(0) = t / N(0);
    for(unsigned k = 1; k < dim ; k++) {
      unsigned pk = t % N(k - 1);
      I(k) = pk / N(k); // dimensional index over on the space of polynomaials
    }
    F(t) = 0.;
    for(unsigned g = 0; g < pow(ng, dim) ; g++) { // multidimensional index on the space of polynomaials
      J(0) = g / NG(0);
      for(unsigned k = 1; k < dim ; k++) {
        unsigned pk = g % NG(k - 1);
        J(k) = pk / NG(k); // dimensional index over on the space of polynomaials
      }
      double value = 1.;

      for(unsigned k = 0; k < dim ; k++) {
        value *= 0.5 * (VxR[k] - VxL[k]) * Pg(I(k), J(k)) * wg(J(k)) ;
      }
      F(t) += value;
    }

  }

}




void SolWeightEigen(Eigen::MatrixXd & A, Eigen::VectorXd & F, Eigen::VectorXd & wP, Eigen::VectorXd & w_new) {

  w_new.resize(wP.size());

  Eigen::VectorXd y = A.transpose() * (A * A.transpose()).partialPivLu().solve(F - A * wP);
  w_new = y + wP;


}

void PrintMarkers(const unsigned & dim, const Eigen::MatrixXd & xP, const std::vector <double> &dist,
                  const Eigen::VectorXd wP, const Eigen::VectorXd & w_new, const unsigned & l, const unsigned & t) {

  std::ofstream fout;

  char filename[100];
  sprintf(filename, "marker%d.txt", l);

  if(t == 0) {
    fout.open(filename);
  }
  else {
    fout.open(filename, std::ios::app);
  }

  for(unsigned i = 0; i < w_new.size(); i++) {

    for(unsigned k = 0; k < dim; k++) {
      fout << xP(k, i) << " ";
    }
    fout <<  dist[i] << " " << wP[i] << " " << w_new[i] << std::endl;
  }

  fout.close();

}




void Cheb(const unsigned & m, Eigen::VectorXd & xg, Eigen::MatrixXd & C) {

  C.resize(xg.size(), m + 1);
  for(unsigned i = 0; i < xg.size(); i++) {
    C(i, 0) = 1;
    C(i, 1) = xg(i);
    for(unsigned j = 2; j <= m; j++) {
      C(i, j) =  2 * xg(i) * C(i, j - 1) - C(i, j - 2);
    }
  }
  C.transposeInPlace();

}

void  GetParticlesOnBox(const double & a, const double & b, const unsigned & n1, const unsigned & dim, Eigen::MatrixXd & x, Eigen::MatrixXd & xL) {
  double h = (b - a) / n1;
  x.resize(dim, pow(n1, dim));
  Eigen::VectorXi I(dim);
  Eigen::VectorXi N(dim);

  for(unsigned k = 0; k < dim ; k++) {
    N(k) = pow(n1, dim - k - 1);
  }

  for(unsigned p = 0; p < pow(n1, dim) ; p++) {
    I(0) = 1 + p / N(0);
    for(unsigned k = 1; k < dim ; k++) {
      unsigned pk = p % N(k - 1);
      I(k) = 1 + pk / N(k);
    }
    //std::cout << I(0) << " " << I(1) << std::endl;

    for(unsigned k = 0; k < dim ; k++) {
      std::srand(std::time(0));
      double r = 2 * ((double) rand() / (RAND_MAX)) - 1;
      x(k, p) = a + h / 2 + (I(k) - 1) * h; // + 0.1 * r;
    }
  }

  xL.resize(dim, pow(n1, dim));
  Eigen::MatrixXd ID;
  ID.resize(dim, pow(n1, dim));
  ID.fill(1.);
  xL = (2. / (b - a)) * x - ((b + a) / (b - a)) * ID;


}


void GetGaussPointsWeights(unsigned & N, Eigen::VectorXd & xg, Eigen::VectorXd & wg) {
  unsigned N1 = N ;
  unsigned N2 = N + 1;
  Eigen::VectorXd xu;
  xu.setLinSpaced(N1, -1, 1);
  xg.resize(N1);
  for(unsigned i = 0; i <= N - 1; i++) {
    xg(i) = cos((2 * i + 1) * M_PI / (2 * (N - 1) + 2)) + (0.27 / N1) * sin(M_PI * xu(i) * (N - 1) / N2) ;
  }
  Eigen::MatrixXd L(N1, N2);
  L.fill(0.);
  Eigen::VectorXd Lp(N1);
  Lp.fill(0.);
  Eigen::VectorXd y0(xg.size());
  y0.fill(2);
  double eps = 1e-15;
  Eigen::VectorXd d = xg - y0;

  double max = d.cwiseAbs().maxCoeff();

  while(max > eps) {

    L.col(0).fill(1.);
    L.col(1) = xg;

    for(unsigned k = 2; k < N2; k++) {
      for(unsigned i = 0; i < N1; i++) {
        L(i, k) = ((2 * k - 1) * xg(i) * L(i, k - 1) - (k - 1) * L(i, k - 2)) / k;
      }
    }


    for(unsigned i = 0; i < N1; i++) {
      Lp(i) = N2 * (L(i, N1 - 1) - xg(i) * L(i, N2 - 1)) / (1 - xg(i) * xg(i));
    }

    y0 = xg;

    for(unsigned i = 0; i < N1; i++) {
      xg(i) =  y0(i) - L(i, N2 - 1) / Lp(i);
    }

    d = xg - y0;

    max = d.cwiseAbs().maxCoeff();
  }

  wg.resize(N1);

  for(unsigned i = 0; i < N1; i++) {
    double r = double(N2) / double(N1);
    wg(i) = (2) / ((1 - xg(i) * xg(i)) * Lp(i) * Lp(i)) * r * r;
  }
}




double GetDistance(const Eigen::VectorXd & x) {

  double radius = 0.75;
  Eigen::VectorXd xc(x.size());
  xc.fill(0.);

  double rx = 0;
  for(unsigned i = 0; i < x.size(); i++) {
    rx += (x[i] - xc[i]) * (x[i] - xc[i]);
  }
  return radius - sqrt(rx);

}


double get_g(const double & r, const double & T, const unsigned & n) {
  double rn = pow(r, n);
  return (-1. + r) * (-1 + r * rn + T - r * T) / (1. + (-1. + n * (-1 + r)) * rn);
}

double get_r(const double & T, const unsigned & n) {
  double r0 = 2.;
  double r = 0;
  while(fabs(r - r0) > 1.0e-10) {
    r0 = r;
    r = r0 - get_g(r0, T, n);
  }
  return r;
}












void InitParticlesDisk(const unsigned & dim, const unsigned & ng, double & xL, double & xR, double & yL, double & yR,
                       const std::vector < double> &xc, const double & R, std::vector < std::vector <double> > &xp,
                       std::vector <double> &wp, std::vector <double> &dist) {

  double theta0 = atan2(yL - xc[1], xR - xc[0]);
  double theta1 = atan2(yR - xc[1], xL - xc[0]);
  if(theta0 < 0) theta0 += M_PI;
  if(theta1 < 0) theta1 += M_PI;

  double R0 = sqrt((xL - xc[0]) * (xL - xc[0]) + (yL - xc[1]) * (yL - xc[1]));
  double R1 = sqrt((xR - xc[0]) * (xR - xc[0]) + (yR - xc[1]) * (yR - xc[1]));

  unsigned m1 = ceil(pow(1., 1. / dim) * (2 * ng - 1));
  double dp = sqrt((xR - xL) * (yR - yL)) / m1;
  unsigned nr = ceil(((R - 0.5 * dp)) / dp);
  double dr = ((R - 0.5 * dp)) / nr;
  double area = 0.;

  xp.resize(dim);
  for(unsigned k = 0; k < dim; k++) {
    xp[k].reserve(2 * pow(m1, dim));
  }
  wp.reserve(2 * pow(m1, dim));
  dist.reserve(2 * pow(m1, dim));
  unsigned cnt = 0;

  int i0 = floor(R0 / dr - 0.5);
  if(i0 < 0) i0 = 0;
  int i1 = ceil(R1 / dr - 0.5);
  if(i1 > nr - 1) i1 = nr - 1;

  for(unsigned i = i0; i <= i1; i++) {
    double ri = (i + 0.5) * dr;
    unsigned nti = ceil(2 * M_PI * ri / dr);
    double dti = 2 * M_PI / nti;
    int j0 = floor(theta0 / dti);
    int j1 = ceil(theta1 / dti);
    for(unsigned j = j0; j <= j1; j++) {
      double tj = j * dti;
      double x = xc[0] + ri * cos(tj);
      double y = xc[1] + ri * sin(tj);
      if(x > xL && x < xR && y > yL && y < yR) {
        for(unsigned k = 0; k < dim; k++) {
          xp[k].resize(cnt + 1);
        }
        wp.resize(cnt + 1);
        dist.resize(cnt + 1);

        xp[0][cnt] = x;
        xp[1][cnt] = y;
        wp[cnt] = ri * dti * dr;
        dist[cnt] = (R - ri);

        area += ri * dti * dr;
        cnt++;

      }
    }
  }

  {
    double ri = R;
    unsigned nti = ceil(2 * M_PI * R / dp); // controls the density of particles on the boundary
    double dti = 2 * M_PI / nti;
    int j0 = floor(theta0 / dti);
    int j1 = ceil(theta1 / dti);
    for(unsigned j = j0; j <= j1; j++) {
      double tj = j * dti;

      double x = xc[0] + ri * cos(tj);
      double y = xc[1] + ri * sin(tj);
      if(x > xL && x < xR && y > yL && y < yR) {
        for(unsigned k = 0; k < dim; k++) {
          xp[k].resize(cnt + 1);
        }
        wp.resize(cnt + 1);
        dist.resize(cnt + 1);

        xp[0][cnt] = x;
        xp[1][cnt] = y;
        wp[cnt] = ri * dti * dp;

        dist[cnt] = (R - ri);

        area += ri * dti * dp;

        cnt++;

      }
    }
  }


  i0 = floor((R0 - (R + 0.5 * dp)) / dr - 0.5);
  if(i0 < 0) i0 = 0;

  i1 = floor((R1 - (R + 0.5 * dp)) / dr - 0.5);
  if(i1 > nr - 1) i1 = nr - 1;

  for(unsigned i = i0; i <= i1; i++) {

    double ri = (R + 0.5 * dp) + (i + 0.5) * dr;
    unsigned nti = ceil(2 * M_PI * ri / dr);
    double dti = 2 * M_PI / nti;

    int j0 = floor(theta0 / dti);
    int j1 = ceil(theta1 / dti);
    for(unsigned j = j0; j <= j1; j++) {
      double tj = j * dti;
      double x = xc[0] + ri * cos(tj);
      double y = xc[1] + ri * sin(tj);
      if(x > xL && x < xR && y > yL && y < yR) {
        for(unsigned k = 0; k < dim; k++) {
          xp[k].resize(cnt + 1);
        }
        wp.resize(cnt + 1);
        dist.resize(cnt + 1);

        xp[0][cnt] = x;
        xp[1][cnt] = y;
        wp[cnt] = ri * dti * dr;
        dist[cnt] = (R - ri);
        //std::cout << x << " " << y << " " << dist[cnt] << " " << wp[cnt] << std::endl;

        area += ri * dti * dr;



        dist[cnt] = R - ri;

        cnt++;

      }
    }
  }


// std::cout << cnt << " " << pow(m1, dim) << std::endl;
// std::cout << "Area = " << area << " vs " << (b - a)*(b - a) << std::endl;





}



void PrintMat(std::vector< std::vector<double> >& M) {

  for(unsigned i = 0; i < M.size(); i++) {
    for(unsigned j = 0; j < M[i].size(); j++) {
      std::cout << M[i][j] << " ";
    }
    std::cout << std::endl;
  }
  std::cout << "\n" << std::endl;
}


void PrintVec(std::vector<double>& v) {
  for(unsigned i = 0; i < v.size(); i++) {

    std::cout << v[i] << " ";
  }
  std::cout << "\n" << std::endl;
}






void InitParticlesDiskOld(const unsigned & dim, const unsigned & ng, const double & eps, const unsigned & nbl, const bool & gradedbl,
                          const double & a, const double & b, const std::vector < double> &xc, const double & R,
                          std::vector < std::vector <double> > &xp, std::vector <double> &wp, std::vector <double> &dist) {


  unsigned m1 = ceil(pow(1., 1. / dim) * (2 * ng - 1));
  double dp = (b - a) / m1;
  double deps = (b - a) * eps;

  unsigned nr1 = ceil(((R - deps)) / dp);
  unsigned nr2 = ceil((2 * R - (R + deps)) / dp);

  double dr1 = ((R - deps)) / nr1;
  double dr2 = (2 * R - (R + deps)) / nr2;
  double dbl = (2. * deps) / nbl;

  double area = 0.;

  xp.resize(dim);
  for(unsigned k = 0; k < dim; k++) {
    xp[k].reserve(2 * pow(m1, dim));
  }
  wp.reserve(2 * pow(m1, dim));
  dist.reserve(2 * pow(m1, dim));
  unsigned cnt = 0;

  for(unsigned i = 0; i < nr1 - 2 * gradedbl; i++) {
    double ri = (i + 0.5) * dr1;
    unsigned nti = ceil(2 * M_PI * ri / dr1);
    double dti = 2 * M_PI / nti;
    for(unsigned j = 0; j < nti; j++) {
      double tj = j * dti;
      double x = xc[0] + ri * cos(tj);
      double y = xc[1] + ri * sin(tj);
      if(x > a && x < b && y > a && y < b) {
        for(unsigned k = 0; k < dim; k++) {
          xp[k].resize(cnt + 1);
        }
        wp.resize(cnt + 1);
        dist.resize(cnt + 1);

        xp[0][cnt] = x;
        xp[1][cnt] = y;
        wp[cnt] = ri * dti * dr1;
        dist[cnt] = (R - ri);
        //std::cout << x << " " << y << " " << dist[cnt] << " " << wp[cnt] << std::endl;

        area += ri * dti * dr1;
        cnt++;

      }
    }
  }

  if(gradedbl) {
    double T = 7.;

    double scale = (2.* dr1) / (T * dbl);
    unsigned n1 = 3;

    if(n1 > 1 && scale < 1.) {
      T = T - 2;
      scale = (2.* dr1) / (T * dbl);
      n1--;
    }

    double r = (n1 > 0) ? get_r(T, n1) : 1.;
    double dri = (n1 > 0) ? scale * dbl * pow(r, n1) : 2. * dr1;

    double ri = R - deps - 2. * dr1;
    for(unsigned i = 0; i <= n1; i++) {
      ri += 0.5 * dri;
      unsigned nti = ceil(2.5 * M_PI * ri / dr1);
      double dti = 2 * M_PI / nti;
      for(unsigned j = 0; j < nti; j++) {
        double tj = (i * dti) / (n1 + 1) + j * dti;
        double x = xc[0] + ri * cos(tj);
        double y = xc[1] + ri * sin(tj);
        if(x > a && x < b && y > a && y < b) {
          for(unsigned k = 0; k < dim; k++) {
            xp[k].resize(cnt + 1);
          }
          wp.resize(cnt + 1);
          dist.resize(cnt + 1);

          xp[0][cnt] = x;
          xp[1][cnt] = y;
          wp[cnt] = ri * dti * dri;
          dist[cnt] = (R - ri);
          //std::cout << x << " " << y << " " << dist[cnt] << " " << wp[cnt] << std::endl;

          area += ri * dti * dri;



          cnt++;

        }
      }
      ri += 0.5 * dri;
      dri /= r;
    }
  }


  {
    //for(unsigned j = 0; j < nti; j++) {
    for(unsigned i = 0; i < nbl; i++) {
      double ri = (R - deps) + (i + 0.5) * dbl;
      unsigned nti = ceil(4 * M_PI * R / (0.5 * (dr1 + dr2)));
      //unsigned nti = ceil(2 * M_PI * ri / (2 * eps) );
      double dti = 2 * M_PI / nti;
      for(unsigned j = 0; j < nti; j++) {
        double tj = /*(i * dti) / (nbl)*/ + j * dti;

        double x = xc[0] + ri * cos(tj);
        double y = xc[1] + ri * sin(tj);
        if(x > a && x < b && y > a && y < b) {
          for(unsigned k = 0; k < dim; k++) {
            xp[k].resize(cnt + 1);
          }
          wp.resize(cnt + 1);
          dist.resize(cnt + 1);

          xp[0][cnt] = x;
          xp[1][cnt] = y;
          wp[cnt] = ri * dti * dbl;

          dist[cnt] = (R - ri);
          //std::cout << x << " " << y << " " << dist[cnt] << " " << wp[cnt] << std::endl;

          area += ri * dti * dbl;

          cnt++;

        }
      }
    }
  }
  if(gradedbl) {
    double T = 7.;

    double scale = (2.* dr2) / (T * dbl);
    unsigned n2 = 3;

    if(n2 > 1 && scale < 1.) {
      T = T - 2;
      scale = (2.* dr1) / (T * dbl);
      n2--;
    }

    double r = (n2 > 0) ? get_r(T, n2) : 1.;
    double dri = (n2 > 0) ? scale * dbl : 2. * dr2;

    double ri = R + deps;
    for(unsigned i = 0; i <= n2; i++) {
      ri += 0.5 * dri;
      unsigned nti = ceil(2.5 * M_PI * ri / dr2);
      double dti = 2 * M_PI / nti;
      for(unsigned j = 0; j < nti; j++) {
        double tj = (i * dti) / (n2 + 1.) + j * dti;
        double x = xc[0] + ri * cos(tj);
        double y = xc[1] + ri * sin(tj);
        if(x > a && x < b && y > a && y < b) {
          for(unsigned k = 0; k < dim; k++) {
            xp[k].resize(cnt + 1);
          }
          wp.resize(cnt + 1);
          dist.resize(cnt + 1);

          xp[0][cnt] = x;
          xp[1][cnt] = y;
          wp[cnt] = ri * dti * dri;
          dist[cnt] = (R - ri);
          //std::cout << x << " " << y << " " << dist[cnt] << " " << wp[cnt] << std::endl;

          area += ri * dti * dri;

          cnt++;

        }
      }
      ri += 0.5 * dri;
      dri *= r;
    }
  }


  for(unsigned i = 2 * gradedbl; i < nr2; i++) {

    double ri = (R + deps) + (i + 0.5) * dr2;
    unsigned nti = ceil(2 * M_PI * ri / dr2);
    double dti = 2 * M_PI / nti;
    for(unsigned j = 0; j < nti; j++) {
      double tj = j * dti;
      double x = xc[0] + ri * cos(tj);
      double y = xc[1] + ri * sin(tj);
      if(x > a && x < b && y > a && y < b) {
        for(unsigned k = 0; k < dim; k++) {
          xp[k].resize(cnt + 1);
        }
        wp.resize(cnt + 1);
        dist.resize(cnt + 1);

        xp[0][cnt] = x;
        xp[1][cnt] = y;
        wp[cnt] = ri * dti * dr2;
        dist[cnt] = (R - ri);
        //std::cout << x << " " << y << " " << dist[cnt] << " " << wp[cnt] << std::endl;

        area += ri * dti * dr2;



        dist[cnt] = R - ri;

        cnt++;

      }
    }
  }


  //std::cout << cnt << " " << pow(m1, dim) << std::endl;
  //std::cout << "Area = " << area << " vs " << (b - a)*(b - a) << std::endl;





}

