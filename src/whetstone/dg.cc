/*
  WhetStone, version 2.0
  Release name: naka-to.

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Author: Konstantin Lipnikov (lipnikov@lanl.gov)

  Discontinuous Galerkin methods.
*/

#include "Point.hh"

#include "dg.hh"
#include "DenseMatrix.hh"
#include "WhetStoneDefs.hh"

namespace Amanzi {
namespace WhetStone {

/* ******************************************************************
* Mass matrix for Taylor basis functions. 
****************************************************************** */
int DG::TaylorMassMatrix(int c, int order, DenseMatrix& M)
{
  // calculate monomials
  int n(1), m((2 * order + 1) * (2 * order + 2) / 2);
  DenseVector monomials(m);

  monomials.PutScalar(0.0);
  monomials(0) = mesh_->cell_volume(c);

  for (int k = 1; k <= 2 * order; ++k) {
    IntegrateMonomialsCell_(c, k, &(monomials(n)));
    n += k + 1;
  }
   
  // copy integrals to mass matrix
  int nrows = M.NumRows();

  for (int k = 0; k < nrows; ++k) {
    for (int l = k; l < nrows; ++l) {
      M(k, l) = M(l, k) = monomials(k + l);
    }
  }

  return 0;
}


/* ******************************************************************
* Integrate all monomials of order k in cell c.
* The cell must be star-shape w.r.t. to its centroid.
****************************************************************** */
void DG::IntegrateMonomialsCell_(int c, int k, double* monomials)
{
  Entity_ID_List faces, nodes;
  std::vector<int> dirs;

  mesh_->cell_get_faces_and_dirs(c, &faces, &dirs);
  int nfaces = faces.size();

  const AmanziGeometry::Point& xc = mesh_->cell_centroid(c);

  for (int n = 0; n < nfaces; ++n) {
    int f = faces[n];
    const AmanziGeometry::Point& xf = mesh_->face_centroid(f);
    const AmanziGeometry::Point& normal = mesh_->face_normal(f);
    double tmp = dirs[n] * ((xf - xc) * normal) / (k + d_);
    
    if (d_ == 3) {
      IntegrateMonomialsFace_(f, k, tmp, monomials);
    } else if (d_ == 2) {
      mesh_->face_get_nodes(f, &nodes);

      AmanziGeometry::Point x1(d_), x2(d_);
      mesh_->node_get_coordinates(nodes[0], &x1);
      mesh_->node_get_coordinates(nodes[1], &x2);

      x1 -= xc;
      x2 -= xc;
      IntegrateMonomialsEdge_(x1, x2, k, tmp, monomials);
    }
  }
}


/* ******************************************************************
* Integrate all monomials of order k on face.
****************************************************************** */
void DG::IntegrateMonomialsFace_(int c, int k, double factor, double* monomials)
{
}


/* ******************************************************************
* Integrate all monomials of order k on edge via quadrature rules.
****************************************************************** */
void DG::IntegrateMonomialsEdge_(
    const AmanziGeometry::Point& x1, const AmanziGeometry::Point& x2,
    int k, double factor, double* monomials)
{
  double a1, a2; 
  AmanziGeometry::Point xm(d_);

  if (d_ == 2) {
    int m = k / 2;  // calculate quadrature rule

    for (int i = 0; i <= k; ++i) {
      for (int n = 0; n <= m; ++n) { 
        xm = x1 * q1d_points[m][n] + x2 * (1.0 - q1d_points[m][n]);
        a1 = std::pow(xm[0], i) * std::pow(xm[1], k - i);
        monomials[i] += factor * a1 * q1d_weights[m][n];      
      }
    }
  }
}

}  // namespace WhetStone
}  // namespace Amanzi

