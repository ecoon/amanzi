/*
  This is the operators component of the Amanzi code. 

  Copyright 2010-2012 held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Author: Konstantin Lipnikov (lipnikov@lanl.gov)
*/

#include <vector>

#include "Epetra_Vector.h"
#include "Epetra_FECrsGraph.h"

#include "exceptions.hh"
#include "errors.hh"
#include "mfd3d_diffusion.hh"

#include "PreconditionerFactory.hh"
#include "OperatorDefs.hh"
#include "OperatorDiffusionSurface.hh"


namespace Amanzi {
namespace Operators {

/* ******************************************************************
* Initialization of the operator.                                           
****************************************************************** */
void OperatorDiffusionSurface::InitOperator(std::vector<WhetStone::Tensor>& K)
{
  CreateMassMatrices_(K);
}


/* ******************************************************************
* Basic routine of each operator: creation of matrices.
****************************************************************** */
void OperatorDiffusionSurface::UpdateMatrices()
{
  // find location of matrix blocks
  int m(0), nblocks = blocks_.size();
  bool flag(false);

  for (int n = 0; n < nblocks; n++) {
    int schema = blocks_schema_[n];
    if (schema & OPERATOR_DOFS_FACE && schema & OPERATOR_DOFS_CELL) {
      m = n;
      flag = true;
      break;
    }
  }

  if (flag == false) { 
    m = nblocks++;
    blocks_schema_.push_back(OPERATOR_DOFS_FACE + OPERATOR_DOFS_CELL);
    blocks_.push_back(Teuchos::rcp(new std::vector<WhetStone::DenseMatrix>));
  }
  std::vector<WhetStone::DenseMatrix>& matrix = *blocks_[m];

  // update matrix blocks
  AmanziMesh::Entity_ID_List faces;
  std::vector<int> dirs;

  for (int c = 0; c < ncells_owned; c++) {
    mesh_->cell_get_faces_and_dirs(c, &faces, &dirs);
    int nfaces = faces.size();

    WhetStone::DenseMatrix& Wff = Wff_cells_[c];
    WhetStone::DenseMatrix Acell(nfaces + 1, nfaces + 1);

    double matsum = 0.0;  // elimination of mass matrix
    for (int n = 0; n < nfaces; n++) {
      double rowsum = 0.0;
      for (int m = 0; m < nfaces; m++) {
        double tmp = Wff(n, m);
        rowsum += tmp;
        Acell(n, m) = tmp;
      }

      Acell(n, nfaces) = -rowsum;
      Acell(nfaces, n) = -rowsum;
      matsum += rowsum;
    }
    Acell(nfaces, nfaces) = matsum;

    if (flag) {
      matrix[c] += Acell;
    } else {
      matrix.push_back(Acell);
    }
  }
}


/* ******************************************************************
* Calculate elemental inverse mass matrices.                                           
****************************************************************** */
void OperatorDiffusionSurface::CreateMassMatrices_(std::vector<WhetStone::Tensor>& K)
{
  WhetStone::MFD3D_Diffusion mfd(mesh_);

  AmanziMesh::Entity_ID_List faces;
  std::vector<int> dirs;

  Wff_cells_.clear();

  for (int c = 0; c < ncells_owned; c++) {
    mesh_->cell_get_faces_and_dirs(c, &faces, &dirs);
    int nfaces = faces.size();

    WhetStone::DenseMatrix Wff(nfaces, nfaces);
    int ok = mfd.MassMatrixInverseSurface(c, K[c], Wff);

    Wff_cells_.push_back(Wff);

    if (ok == WhetStone::WHETSTONE_ELEMENTAL_MATRIX_FAILED) {
      Errors::Message msg("OperatorDiffusionSurface: unexpected failure in WhetStone.");
      Exceptions::amanzi_throw(msg);
    }
  }
}


/* ******************************************************************
* Special assemble of elemental face-based matrices. 
****************************************************************** */
void OperatorDiffusionSurface::AssembleMatrix(int schema)
{
  if (!(schema & OPERATOR_DOFS_FACE && schema & OPERATOR_DOFS_CELL)) {
    std::cout << "Schema " << schema << " is not supported" << endl;
    ASSERT(0);
  }

  // find location of face-based matrices
  int m, nblocks = blocks_.size();
  for (int n = 0; n < nblocks; n++) {
    if ((blocks_schema_[n] & schema) == schema) {
      m = n;
      break;
    }
  }
  std::vector<WhetStone::DenseMatrix>& matrix = *blocks_[m];

  // populate the matrix
  A_->PutScalar(0.0);

  const Epetra_Map& map = mesh_->face_map(false);
  const Epetra_Map& map_wghost = mesh_->face_map(true);

  AmanziMesh::Entity_ID_List faces;
  std::vector<int> dirs;

  int faces_LID[OPERATOR_MAX_FACES];
  int faces_GID[OPERATOR_MAX_FACES];

  for (int c = 0; c < ncells_owned; c++) {
    mesh_->cell_get_faces_and_dirs(c, &faces, &dirs);
    int nfaces = faces.size();

    for (int n = 0; n < nfaces; n++) {
      faces_LID[n] = faces[n];
      faces_GID[n] = map_wghost.GID(faces_LID[n]);
    }
    A_->SumIntoGlobalValues(nfaces, faces_GID, matrix[c].Values());
  }
  A_->GlobalAssemble();

  // Add diagonal
  diagonal_->GatherGhostedToMaster("face", Add);
  Epetra_MultiVector& diag = *diagonal_->ViewComponent("face");

  Epetra_Vector tmp(A_->RowMap());
  A_->ExtractDiagonalCopy(tmp);
  tmp.Update(1.0, diag, 1.0);
  A_->ReplaceDiagonalValues(tmp);

  // Assemble all right-hand sides
  rhs_->GatherGhostedToMaster("face", Add);
}


/* ******************************************************************
* Assembles four matrices: diagonal Acc_, two off-diagonal blocks
* Acf_ and Afc_, and the Schur complement Sff_.
****************************************************************** */
void OperatorDiffusionSurface::InitPreconditioner(
    const std::string& prec_name, const Teuchos::ParameterList& plist,
    std::vector<int>& bc_model, std::vector<double>& bc_values)
{
  // find the block of matrices
  int m, nblocks = blocks_schema_.size();
  for (int n = 0; n < nblocks; n++) {
    int schema = blocks_schema_[n];
    if (schema & OPERATOR_DOFS_FACE && schema & OPERATOR_DOFS_CELL) {
      m = n;
      break;
    }
  }
  std::vector<WhetStone::DenseMatrix>& matrix = *blocks_[m];

  // create a face-based stiffness matrix
  Teuchos::RCP<Epetra_FECrsMatrix> S = Teuchos::rcp(new Epetra_FECrsMatrix(*A_));
  S->PutScalar(0.0);

  const Epetra_Map& fmap_wghost = mesh_->face_map(true);
  AmanziMesh::Entity_ID_List faces;
  std::vector<int> dirs;
  int faces_LID[OPERATOR_MAX_FACES];
  int faces_GID[OPERATOR_MAX_FACES];

  for (int c = 0; c < ncells_owned; c++) {
    mesh_->cell_get_faces_and_dirs(c, &faces, &dirs);
    int nfaces = faces.size();

    WhetStone::DenseMatrix Scell(nfaces, nfaces);
    WhetStone::DenseMatrix& Acell = matrix[c];

    double tmp = Acell(nfaces, nfaces);
    for (int n = 0; n < nfaces; n++) {
      for (int m = 0; m < nfaces; m++) {
        Scell(n, m) = Acell(n, m) - Acell(n, nfaces) * Acell(nfaces, m) / tmp;
      }
    }

    for (int n = 0; n < nfaces; n++) {  // Symbolic boundary conditions
      int f = faces[n];
      if (bc_model[f] == OPERATOR_BC_FACE_DIRICHLET) {
        for (int m = 0; m < nfaces; m++) Scell(n, m) = Scell(m, n) = 0.0;
        Scell(n, n) = 1.0;
      }
    }

    for (int n = 0; n < nfaces; n++) {
      faces_LID[n] = faces[n];
      faces_GID[n] = fmap_wghost.GID(faces_LID[n]);
    }
    S->SumIntoGlobalValues(nfaces, faces_GID, Scell.Values());
  }
  S->GlobalAssemble();

  // redefine (if necessary) preconditioner since only 
  // one preconditioner is allowed.
  AmanziPreconditioners::PreconditionerFactory factory;
  preconditioner_ = factory.Create(prec_name, plist);
  preconditioner_->Update(S);
}


/* ******************************************************************
* The cell-based and face-based d.o.f. are packed together into 
* the X and Y vectors.
****************************************************************** */
int OperatorDiffusionSurface::ApplyInverse(const CompositeVector& X, CompositeVector& Y) const
{
  // find the block of matrices
  int m, nblocks = blocks_.size();
  for (int n = 0; n < nblocks; n++) {
    int schema = blocks_schema_[n];
    if (schema & OPERATOR_DOFS_FACE && schema & OPERATOR_DOFS_CELL) {
      m = n;
      break;
    }
  }
  std::vector<WhetStone::DenseMatrix>& matrix = *blocks_[m];

  // apply preconditioner inversion
  const Epetra_MultiVector& Xc = *X.ViewComponent("cell");
  const Epetra_MultiVector& Xf = *X.ViewComponent("face", true);

  Epetra_MultiVector& Yc = *Y.ViewComponent("cell");
  Epetra_MultiVector& Yf = *Y.ViewComponent("face", true);

  // Temporary cell and face vectors.
  CompositeVector T(X);
  Epetra_MultiVector& Tc = *T.ViewComponent("cell");
  Epetra_MultiVector& Tf = *T.ViewComponent("face", true);

  // FORWARD ELIMINATION:  Tf = Xf - Afc inv(Acc) Xc
  // Tc.ReciprocalMultiply(1.0, diag, Xc, 0.0);
  // (*Afc_).Multiply(true, Tc, Tf);  // Afc is kept in transpose form
  // Tf.Update(1.0, Xf, -1.0);
  AmanziMesh::Entity_ID_List faces;
  std::vector<int> dirs;
  Epetra_MultiVector& diag = *diagonal_->ViewComponent("cell");

  Tf = Xf;
  for (int c = 0; c < ncells_owned; c++) {
    mesh_->cell_get_faces_and_dirs(c, &faces, &dirs);
    int nfaces = faces.size();

    WhetStone::DenseMatrix& Acell = matrix[c];

    double tmp = Xc[0][c] / (Acell(nfaces, nfaces) + diag[0][c]);
    for (int n = 0; n < nfaces; n++) {
      int f = faces[n];
      Tf[0][f] -= Acell(n, nfaces) * tmp;
    }
  }

  // Solve the Schur complement system Sff * Yf = Tf.
  T.GatherGhostedToMaster(Add);
  preconditioner_->ApplyInverse(Tf, Yf);

  // BACKWARD SUBSTITUTION:  Yc = inv(Acc) (Xc - Acf Yf)
  // (*Acf_).Multiply(false, Yf, Tc);  // It performs the required parallel communications.
  // Tc.Update(1.0, Xc, -1.0);
  // Yc.ReciprocalMultiply(1.0, diag, Tc, 0.0);
  for (int c = 0; c < ncells_owned; c++) {
    mesh_->cell_get_faces_and_dirs(c, &faces, &dirs);
    int nfaces = faces.size();

    WhetStone::DenseMatrix& Acell = matrix[c];

    double tmp = Xc[0][c];
    for (int n = 0; n < nfaces; n++) {
      int f = faces[n];
      tmp -= Acell(nfaces, n) * Yf[0][f];
    }
    Yc[0][c] = tmp / (Acell(nfaces, nfaces) + diag[0][c]);
  }

  return 0;
}

}  // namespace AmanziFlow
}  // namespace Amanzi

