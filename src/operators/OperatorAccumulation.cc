/*
  This is the Operator component of the Amanzi code.

  Copyright 2010-2013 held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Author: Konstantin Lipnikov (lipnikov@lanl.gov)
          Ethan Coon (ecoon@lanl.gov)
*/

#include "Operator_Cell.hh"
//#include "Operator_Node.hh"
#include "Op_Node_Node.hh"
#include "Op_Cell_Cell.hh"

#include "OperatorAccumulation.hh"

namespace Amanzi {
namespace Operators {

// update methods
// -- update method for just adding to PC
void
OperatorAccumulation::AddAccumulationTerm(const Epetra_MultiVector& du)
{
  std::vector<double>& diag = local_op_->vals;
  ASSERT(diag.size() == du.MyLength());
  for (int i=0; i!=du.MyLength(); ++i) {
    diag[i] = du[0][i];
  }
}

// -- linearized update methods with storage terms
void
OperatorAccumulation::AddAccumulationTerm(const CompositeVector& u0,
        const CompositeVector& s0, const CompositeVector& ss,
        double dT, const std::string& name)
{
  AmanziMesh::Entity_ID_List nodes;
  CompositeVector entity_volume(ss);

  if (name == "cell" && ss.HasComponent("cell")) {
    Epetra_MultiVector& volume = *entity_volume.ViewComponent(name); 

    for (int c=0; c!=ncells_owned; ++c) {
      volume[0][c] = mesh_->cell_volume(c); 
    }
  } else if (name == "face" && ss.HasComponent("face")) {
    // Missing code.
    ASSERT(false);
  } else if (name == "node" && ss.HasComponent("node")) {
    Epetra_MultiVector& volume = *entity_volume.ViewComponent(name, true); 
    volume.PutScalar(0.0);

    for (int c=0; c!=ncells_owned; ++c) {
      mesh_->cell_get_nodes(c, &nodes);
      int nnodes = nodes.size();

      for (int i = 0; i < nnodes; i++) {
        volume[0][nodes[i]] += mesh_->cell_volume(c) / nnodes; 
      }
    }

    entity_volume.GatherGhostedToMaster(name);
  } else {
    ASSERT(false);
  }

  const Epetra_MultiVector& u0c = *u0.ViewComponent(name);
  const Epetra_MultiVector& s0c = *s0.ViewComponent(name);
  const Epetra_MultiVector& ssc = *ss.ViewComponent(name);

  Epetra_MultiVector& volume = *entity_volume.ViewComponent(name); 
  std::vector<double>& diag = local_op_->vals;
  Epetra_MultiVector& rhs = *global_operator()->rhs()->ViewComponent(name);

  int n = u0c.MyLength();
  ASSERT(diag.size() == n);
  for (int i = 0; i < n; i++) {
    double factor = volume[0][i] / dT;
    diag[i] = factor * ssc[0][i];
    rhs[0][i] += factor * s0c[0][i] * u0c[0][i];
  }
}

void
OperatorAccumulation::AddAccumulationTerm(const CompositeVector& u0,
        const CompositeVector& ss, double dT, const std::string& name)
{
  AmanziMesh::Entity_ID_List nodes;

  CompositeVector entity_volume(ss);

  if (name == "cell" && ss.HasComponent("cell")) {
    Epetra_MultiVector& volume = *entity_volume.ViewComponent(name); 

    for (int c=0; c!=ncells_owned; ++c) {
      volume[0][c] = mesh_->cell_volume(c); 
    }
  } else if (name == "face" && ss.HasComponent("face")) {
    // Missing code.
    ASSERT(false);
  } else if (name == "node" && ss.HasComponent("node")) {
    Epetra_MultiVector& volume = *entity_volume.ViewComponent(name, true); 
    volume.PutScalar(0.0);

    for (int c=0; c!=ncells_owned; ++c) {
      mesh_->cell_get_nodes(c, &nodes);
      int nnodes = nodes.size();

      for (int i = 0; i < nnodes; i++) {
        volume[0][nodes[i]] += mesh_->cell_volume(c) / nnodes; 
      }
    }

    entity_volume.GatherGhostedToMaster(name);
  } else {
    ASSERT(false);
  }

  const Epetra_MultiVector& u0c = *u0.ViewComponent(name);
  const Epetra_MultiVector& ssc = *ss.ViewComponent(name);

  Epetra_MultiVector& volume = *entity_volume.ViewComponent(name); 
  std::vector<double>& diag = local_op_->vals;
  Epetra_MultiVector& rhs = *global_operator()->rhs()->ViewComponent(name);

  int n = u0c.MyLength();
  ASSERT(diag.size() == n);
  for (int i = 0; i < n; i++) {
    double factor = volume[0][i] * ssc[0][i] / dT;
    diag[i] = factor;
    rhs[0][i] += factor * u0c[0][i];
  }
}

void
OperatorAccumulation::AddAccumulationTerm(const CompositeVector& u0,
        const CompositeVector& ss, const std::string& name)
{
  if (!ss.HasComponent(name)) ASSERT(false);

  const Epetra_MultiVector& u0c = *u0.ViewComponent(name);
  const Epetra_MultiVector& ssc = *ss.ViewComponent(name);

  std::vector<double>& diag = local_op_->vals;
  Epetra_MultiVector& rhs = *global_operator()->rhs()->ViewComponent(name);

  int n = u0c.MyLength();
  ASSERT(diag.size() == n);
  for (int i = 0; i < n; i++) {
    diag[i] = ssc[0][i];
    rhs[0][i] += ssc[0][i] * u0c[0][i];
  }
}

void
OperatorAccumulation::InitAccumulation_(AmanziMesh::Entity_kind entity)
{
  if (global_op_ == Teuchos::null) {
    // constructor was given a mesh
    Teuchos::ParameterList plist;

    if (entity == AmanziMesh::CELL) {
      global_op_schema_ = OPERATOR_SCHEMA_DOFS_CELL;
      Teuchos::RCP<CompositeVectorSpace> cvs = Teuchos::rcp(new CompositeVectorSpace());
      cvs->SetMesh(mesh_)->AddComponent("cell", AmanziMesh::CELL, 1);
      global_op_ = Teuchos::rcp(new Operator_Cell(cvs, plist, global_op_schema_));

      local_op_schema_ = OPERATOR_SCHEMA_BASE_CELL | OPERATOR_SCHEMA_DOFS_CELL;
      std::string name("CELL_CELL");
      local_op_ = Teuchos::rcp(new Op_Cell_Cell(name, mesh_));

    // } else if (entity == AmanziMesh::FACE) {
      // no local op FACE_FACE.  Could be made if this is needed, but I doubt it is --etc
      
    //   global_op_schema_ = OPERATOR_SCHEMA_DOFS_FACE;
    //   Teuchos::RCP<CompositeVectorSpace> cvs = Teuchos::rcp(new CompositeVectorSpace());
    //   cvs->SetMesh(mesh_)->AddComponent("face", AmanziMesh::FACE, 1);
    //   global_op_ = Teuchos::rcp(new Operator_Face(cvs, plist, global_op_schema_));

    } else if (entity == AmanziMesh::NODE) {
      ASSERT(0);
      // global_op_schema_ = OPERATOR_SCHEMA_DOFS_NODE;
      // Teuchos::RCP<CompositeVectorSpace> cvs = Teuchos::rcp(new CompositeVectorSpace());
      // cvs->SetMesh(mesh_)->AddComponent("node", AmanziMesh::NODE, 1);
      // global_op_ = Teuchos::rcp(new Operator_Node(cvs, plist, global_op_schema_));

      local_op_schema_ = OPERATOR_SCHEMA_BASE_CELL | OPERATOR_SCHEMA_DOFS_CELL;
      std::string name("NODE_NODE");
      local_op_ = Teuchos::rcp(new Op_Node_Node(name, mesh_));

    } else {
      Errors::Message msg;
      msg << "Operators: Invalid Accumulation entity " << entity << " not implemented.\n";
      Exceptions::amanzi_throw(msg);
    }

  } else {
    // constructor was given an Operator
    global_op_schema_ = global_op_->schema();
    mesh_ = global_op_->DomainMap().Mesh();

    if (entity == AmanziMesh::CELL) {
      local_op_schema_ = OPERATOR_SCHEMA_BASE_CELL | OPERATOR_SCHEMA_DOFS_CELL;
      std::string name("CELL_CELL");
      local_op_ = Teuchos::rcp(new Op_Cell_Cell(name, mesh_));

    // } else if (entity == AmanziMesh::FACE) {
      // no local op FACE_FACE.  Could be made if this is needed, but I doubt it is --etc

    } else if (entity == AmanziMesh::NODE) {
      local_op_schema_ = OPERATOR_SCHEMA_BASE_CELL | OPERATOR_SCHEMA_DOFS_CELL;
      std::string name("NODE_NODE");
      local_op_ = Teuchos::rcp(new Op_Node_Node(name, mesh_));

    } else {
      Errors::Message msg;
      msg << "Operators: Invalid Accumulation entity " << entity << " not implemented.\n";
      Exceptions::amanzi_throw(msg);
    }
  }

  // register the accumulation Op
  global_op_->OpPushBack(local_op_);

  // mesh info
  ncells_owned = mesh_->num_entities(AmanziMesh::CELL, AmanziMesh::OWNED);
  nfaces_owned = mesh_->num_entities(AmanziMesh::FACE, AmanziMesh::OWNED);
  nnodes_owned = mesh_->num_entities(AmanziMesh::NODE, AmanziMesh::OWNED);
}

}  // namespace Operators
}  // namespace Amanzi


