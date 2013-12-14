/*
  This is the flow component of the Amanzi code.  

  Copyright 2010-2012 held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Authors: Konstantin Lipnikov (lipnikov@lanl.gov)
           Daniil Svyatskiy (dasvyat@lanl.gov)

  The class provides a different implementation of solvers than in 
  the base class. In particular, Lagrange multipliers are elliminated
  from the DAE system and short vectors are used in the nonlinear solver.
*/

#ifndef AMANZI_MATRIX_TPFA_HH_
#define AMANZI_MATRIX_TPFA_HH_

#include <strings.h>

#include "Teuchos_RCP.hpp"
#include "Teuchos_SerialDenseMatrix.hpp"
#include "Epetra_FECrsMatrix.h"
#include "Ifpack.h" 

#include "State.hh"
#include "CompositeVector.hh"
#include "Matrix.hh"
#include "RelativePermeability.hh"


namespace Amanzi {
namespace AmanziFlow {

class Matrix_TPFA : public Matrix<CompositeVector, CompositeVectorSpace> {
 public:
  Matrix_TPFA() {};
  Matrix_TPFA(Teuchos::RCP<State> S, 
              std::vector<WhetStone::Tensor>* K, 
              Teuchos::RCP<RelativePermeability> rel_perm);
  ~Matrix_TPFA() {};

  // main members (required members)
  void Init();
  void SymbolicAssemble();
  void Assemble();

  int Apply(const CompositeVector& X, CompositeVector& Y) const;
  int ApplyInverse(const CompositeVector& X, CompositeVector& Y) const;

  void ApplyBoundaryConditions(std::vector<int>& bc_model, std::vector<bc_tuple>& bc_values); 

  void InitPreconditioner(const std::string& name, const Teuchos::ParameterList& plist) {};
  void UpdatePreconditioner() {};

  void DeriveMassFlux(const CompositeVector& solution, CompositeVector& darcy_mass_flux,
                      std::vector<int>& bc_model, std::vector<bc_tuple>& bc_values);

  const CompositeVectorSpace& DomainMap() const {
    return cvs_;
  }
  const CompositeVectorSpace& RangeMap() const {
    return cvs_;
  }

  double ComputeNegativeResidual(const CompositeVector& v, CompositeVector& r);

 private:
  void AnalyticJacobian(const Epetra_Vector& solution, 
                        std::vector<int>& bc_markers, 
                        std::vector<bc_tuple>& bc_values);

  void ComputeJacobianLocal(int mcells,
                            int face_id,
                            int Krel_method,
                            std::vector<int>& bc_markers,
                            std::vector<bc_tuple>& bc_values,
                            double *pres,
                            double *dk_dp_cell,
                            Teuchos::SerialDenseMatrix<int, double>& Jpp);

  void ComputeTransmissibilities_();
         
  void AddGravityFluxesRichards(double rho, const AmanziGeometry::Point& gravity, 
                                std::vector<int>& bc_model);

 protected:
  CompositeVectorSpace cvs_;

  Teuchos::RCP<Epetra_Vector> Dff_;
  Teuchos::RCP<Epetra_FECrsMatrix> Spp_;  // Explicit Schur complement

  std::vector<double> Acc_cells_;
  std::vector<double> Fc_cells_;

  Teuchos::RCP<FlowMatrix> preconditioner_;

 private:
  int ncells_owned, ncells_wghost;
  int nfaces_owned, nfaces_wghost;

  Teuchos::RCP<Epetra_Vector> transmissibility_;
  Teuchos::RCP<Epetra_Vector> gravity_term_;

  void operator=(const Matrix_TPFA& matrix);
};

}  // namespace AmanziFlow
}  // namespace Amanzi

#endif
