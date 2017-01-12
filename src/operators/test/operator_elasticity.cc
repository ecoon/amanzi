/*
  Operators

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Author: Konstantin Lipnikov (lipnikov@lanl.gov)
*/

#include <cstdlib>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

// TPLs
#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"
#include "Teuchos_ParameterXMLFileReader.hpp"
#include "EpetraExt_RowMatrixOut.h"
#include "UnitTest++.h"

// Amanzi
#include "MeshFactory.hh"
#include "GMVMesh.hh"
#include "LinearOperatorFactory.hh"
#include "Tensor.hh"

// Operators
#include "AnalyticElasticity01.hh"
#include "OperatorElasticity.hh"
#include "Verification.hh"


/* *****************************************************************
* Elasticity model: exactness test.
***************************************************************** */
TEST(OPERATOR_ELASTICITY_EXACTNESS) {
  using namespace Amanzi;
  using namespace Amanzi::AmanziMesh;
  using namespace Amanzi::AmanziGeometry;
  using namespace Amanzi::Operators;

  Epetra_MpiComm comm(MPI_COMM_WORLD);
  int MyPID = comm.MyPID();
  if (MyPID == 0) std::cout << "\nTest: 2D elasticity: exactness test" << std::endl;

  // read parameter list
  std::string xmlFileName = "test/operator_elasticity.xml";
  Teuchos::ParameterXMLFileReader xmlreader(xmlFileName);
  Teuchos::ParameterList plist = xmlreader.getParameters();
  Teuchos::ParameterList op_list = plist.get<Teuchos::ParameterList>("PK operator")
                                        .sublist("elasticity operator");

  // create an SIMPLE mesh framework
  FrameworkPreference pref;
  pref.clear();
  pref.push_back(MSTK);
  pref.push_back(STKMESH);

  MeshFactory meshfactory(&comm);
  meshfactory.preference(pref);
  Teuchos::RCP<const Mesh> mesh = meshfactory(0.0, 0.0, 1.0, 1.0, 4, 4, Teuchos::null);

  // modify diffusion coefficient
  int ncells = mesh->num_entities(AmanziMesh::CELL, AmanziMesh::OWNED);
  int nnodes = mesh->num_entities(AmanziMesh::NODE, AmanziMesh::OWNED);
  int nfaces = mesh->num_entities(AmanziMesh::FACE, AmanziMesh::OWNED);
  int nfaces_wghost = mesh->num_entities(AmanziMesh::FACE, AmanziMesh::USED);
  int nnodes_wghost = mesh->num_entities(AmanziMesh::NODE, AmanziMesh::USED);

  AnalyticElasticity01 ana(mesh);

  Teuchos::RCP<std::vector<WhetStone::Tensor> > K = Teuchos::rcp(new std::vector<WhetStone::Tensor>());
  for (int c = 0; c < ncells; c++) {
    const Point& xc = mesh->cell_centroid(c);
    const WhetStone::Tensor& Kc = ana.Tensor(xc, 0.0);
    K->push_back(Kc);
  }

  // create boundary data
  // -- on faces
  std::vector<int> bcf_model(nfaces_wghost, Operators::OPERATOR_BC_NONE);
  std::vector<double> bcf_value(nfaces_wghost, 0.0), bcf_mixed(nfaces_wghost, 0.0);

  for (int f = 0; f < nfaces_wghost; f++) {
    const Point& xf = mesh->face_centroid(f);
    const Point& normal = mesh->face_normal(f);
    double area = mesh->face_area(f);

    if (fabs(xf[0]) < 1e-6 || fabs(xf[0] - 1.0) < 1e-6 ||
        fabs(xf[1]) < 1e-6 || fabs(xf[1] - 1.0) < 1e-6) {
      bcf_model[f] = OPERATOR_BC_DIRICHLET;
      bcf_value[f] = (ana.velocity_exact(xf, 0.0) * normal) / area;
    }
  }
  Teuchos::RCP<BCs> bcf =
      Teuchos::rcp(new BCs(Operators::OPERATOR_BC_TYPE_FACE, bcf_model, bcf_value, bcf_mixed));

  // -- at nodes
  Point xv(2);
  std::vector<int> bcv_model(nnodes_wghost, Operators::OPERATOR_BC_NONE);
  std::vector<Point> bcv_value(nnodes_wghost);

  for (int v = 0; v < nnodes_wghost; ++v) {
    mesh->node_get_coordinates(v, &xv);

    if (fabs(xv[0]) < 1e-6 || fabs(xv[0] - 1.0) < 1e-6 ||
        fabs(xv[1]) < 1e-6 || fabs(xv[1] - 1.0) < 1e-6) {
      bcv_model[v] = OPERATOR_BC_DIRICHLET;
      bcv_value[v] = ana.velocity_exact(xv, 0.0);
    }
  }
  Teuchos::RCP<BCs> bcv = 
      Teuchos::rcp(new BCs(Operators::OPERATOR_BC_TYPE_NODE, bcv_model, bcv_value));

  // create diffusion operator 
  Teuchos::RCP<OperatorElasticity> op = Teuchos::rcp(new OperatorElasticity(op_list, mesh));
  op->SetBCs(bcf, bcf);
  op->AddBCs(bcv, bcv);
  const CompositeVectorSpace& cvs = op->global_operator()->DomainMap();

  // create and initialize state variables.
  CompositeVector solution(cvs);
  solution.PutScalar(0.0);

  // create source 
  CompositeVector source(cvs);
  Epetra_MultiVector& src = *source.ViewComponent("node");

  for (int v = 0; v < nnodes; v++) {
    mesh->node_get_coordinates(v, &xv);
    Point tmp(ana.source_exact(xv, 0.0));
    for (int k = 0; k < 2; ++k) src[k][v] = tmp[k];
  }

  // populate the elasticity operator
  op->SetTensorCoefficient(K);
  op->UpdateMatrices();

  // get and assmeble the global operator
  Teuchos::RCP<Operator> global_op = op->global_operator();
  global_op->UpdateRHS(source, true);
  op->ApplyBCs(true, true);
  global_op->SymbolicAssembleMatrix();
  global_op->AssembleMatrix();

  // create preconditoner using the base operator class
  Teuchos::ParameterList slist = plist.get<Teuchos::ParameterList>("preconditioners");
  global_op->InitPreconditioner("Hypre AMG", slist);

  // Test SPD properties of the matrix and preconditioner.
  Verification ver(global_op);
  ver.CheckMatrixSPD(true, true);
  ver.CheckPreconditionerSPD(true, true);

  // solve the problem
  Teuchos::ParameterList lop_list = plist.get<Teuchos::ParameterList>("solvers");
  AmanziSolvers::LinearOperatorFactory<Operator, CompositeVector, CompositeVectorSpace> factory;
  Teuchos::RCP<AmanziSolvers::LinearOperator<Operator, CompositeVector, CompositeVectorSpace> >
     solver = factory.Create("PCG", lop_list, global_op);

  CompositeVector& rhs = *global_op->rhs();
  int ierr = solver->ApplyInverse(rhs, solution);

  if (MyPID == 0) {
    std::cout << "elasticity solver (" << solver->name() 
              << "): ||r||=" << solver->residual() << " itr=" << solver->num_itrs()
              << " code=" << solver->returned_code() << std::endl;
  }

  // compute velocity error
  double unorm, ul2_err, uinf_err;
  ana.ComputeNodeError(solution, 0.0, unorm, ul2_err, uinf_err);

  if (MyPID == 0) {
    ul2_err /= unorm;
    printf("L2(u)=%12.8g  Inf(u)=%12.8g  itr=%3d\n",
        ul2_err, uinf_err, solver->num_itrs());

    CHECK(ul2_err < 0.1);
    CHECK(solver->num_itrs() < 15);
  }
}


