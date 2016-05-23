/*
  Process Kernels 

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Author: Konstantin Lipnikov (lipnikov@lanl.gov)
*/

#ifndef AMANZI_PK_DOMAIN_FUNCTION_WEIGHT_HH_
#define AMANZI_PK_DOMAIN_FUNCTION_WEIGHT_HH_

#include <string>
#include <vector>

#include "Epetra_Vector.h"
#include "Teuchos_RCP.hpp"

#include "CommonDefs.hh"
#include "Mesh.hh"
#include "PK_DomainFunction.hh"

namespace Amanzi {

template <class FunctionBase>
class PK_DomainFunctionWeight : public FunctionBase,
                                public Functions::UniqueMeshFunction {
 public:
  PK_DomainFunctionWeight(const Teuchos::RCP<const AmanziMesh::Mesh>& mesh) :
      UniqueMeshFunction(mesh) {};
  ~PK_DomainFunctionWeight() {};

  // member functions
  void Init(const Teuchos::ParameterList& plist, Teuchos::RCP<const Epetra_Vector> weight);

  // required member functions
  virtual void Compute(double t0, double t1);
  virtual std::string name() { return "weight"; }

 protected:
  using FunctionBase::value_;

 private:
  std::string submodel_;
  Teuchos::RCP<const Epetra_Vector> weight_;
};


template <class FunctionBase>
void PK_DomainFunctionWeight<FunctionBase>::Init(
    const Teuchos::ParameterList& plist, Teuchos::RCP<const Epetra_Vector> weight)
{
  ASSERT(weight != Teuchos::null);

  std::vector<std::string> regions = plist.get<Teuchos::Array<std::string> >("regions").toVector();

  Teuchos::RCP<Amanzi::MultiFunction> f;
  try {
    Teuchos::ParameterList flist = plist.sublist("sink");
    f = Teuchos::rcp(new MultiFunction(flist));
  } catch (Errors::Message& msg) {
    Errors::Message m;
    m << "error in source sublist : " << msg.what();
    Exceptions::amanzi_throw(m);
  }

  // Add this source specification to the domain function.
  Teuchos::RCP<Domain> domain = Teuchos::rcp(new Domain(regions, AmanziMesh::CELL));
  AddSpec(Teuchos::rcp(new Spec(domain, f)));

  weight_ = weight;
}


/* ******************************************************************
* Compute and distribute the result by volume.
****************************************************************** */
template <class FunctionBase>
void PK_DomainFunctionWeight<FunctionBase>::Compute(double t0, double t1)
{
  double dt = t1 - t0;
  if (dt > 0.0) dt = 1.0 / dt;
 
  // create the input tuple (time + space)
  int dim = mesh_->space_dimension();
  std::vector<double> args(1 + dim);

  int ncells_owned = mesh_->num_entities(AmanziMesh::CELL, AmanziMesh::OWNED);

  for (UniqueSpecList::const_iterator uspec = unique_specs_[AmanziMesh::CELL]->begin();
       uspec != unique_specs_[AmanziMesh::CELL]->end(); ++uspec) {

    double domain_volume = 0.0;
    Teuchos::RCP<MeshIDs> ids = (*uspec)->second;

    for (MeshIDs::const_iterator id = ids->begin(); id != ids->end(); ++id) {
      if (*id < ncells_owned) domain_volume += mesh_->cell_volume(*id) * (*weight_)[*id];
    }
    double volume_tmp = domain_volume;
    mesh_->get_comm()->SumAll(&volume_tmp, &domain_volume, 1);

    args[0] = t1;
    for (MeshIDs::const_iterator id = ids->begin(); id != ids->end(); ++id) {
      const AmanziGeometry::Point& xc = mesh_->cell_centroid(*id);
      for (int i = 0; i != dim; ++i) args[i + 1] = xc[i];
      value_[*id] = (*(*uspec)->first->second)(args)[0] * (*weight_)[*id] / domain_volume;
    }      

    if (submodel_ == "integrated source") {
      args[0] = t0;
      for (MeshIDs::const_iterator id = ids->begin(); id != ids->end(); ++id) {
        const AmanziGeometry::Point& xc = mesh_->cell_centroid(*id);
        for (int i = 0; i != dim; ++i) args[i + 1] = xc[i];
        value_[*id] -= (*(*uspec)->first->second)(args)[0] * (*weight_)[*id] / domain_volume;
        value_[*id] *= dt;
      }
    }
  }
}

}  // namespace Amanzi

#endif
