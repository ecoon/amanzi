/*
  This is the transport component of Amanzi. 

  Copyright 2010-2013 held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Author: Konstantin Lipnikov (lipnikov@lanl.gov)
*/

#ifndef AMANZI_TRANSPORT_PK_HH_
#define AMANZI_TRANSPORT_PK_HH_

// TPLs
#include "Epetra_Vector.h"
#include "Epetra_IntVector.h"
#include "Epetra_Import.h"
#include "Teuchos_RCP.hpp"

// Amanzi
#include "CompositeVector.hh"
#include "DiffusionPhase.hh"
#include "Explicit_TI_FnBase.hh"
#include "MaterialProperties.hh"
#include "PK.hh"
#include "PK_Factory.hh"
#include "ReconstructionCell.hh"
#include "State.hh"
#include "Tensor.hh"
#include "VerboseObject.hh"

#ifdef ALQUIMIA_ENABLED
#include "Chemistry_State.hh"
#include "ChemistryEngine.hh"
#endif

// Transport
#include "MDMPartition.hh"
#include "MultiscaleTransportPorosityPartition.hh"
#include "TransportBoundaryFunction.hh"
#include "TransportDomainFunction.hh"
#include "TransportDefs.hh"
#include "TransportSourceFactory.hh"

/* ******************************************************************
The transport PK receives a reduced (optional) copy of a physical 
state at time n and returns a different state at time n+1. 

Unmodified physical quantaties in the returned state are the smart 
pointers to the original variables.
****************************************************************** */

namespace Amanzi {
namespace Transport {

typedef double AnalyticFunction(const AmanziGeometry::Point&, const double);

class Transport_PK : public PK, public Explicit_TI::fnBase<Epetra_Vector> {
 public:
  Transport_PK(Teuchos::ParameterList& pk_tree,
               const Teuchos::RCP<Teuchos::ParameterList>& glist,
               const Teuchos::RCP<State>& S,
               const Teuchos::RCP<TreeVector>& soln);

  Transport_PK(const Teuchos::RCP<Teuchos::ParameterList>& glist,
               Teuchos::RCP<State> S,
               const std::string& pk_list_name,
               std::vector<std::string>& component_names);

  ~Transport_PK();

  // members required by PK interface
  virtual void Setup();
  virtual void Initialize();

  virtual double get_dt();
  virtual void set_dt(double dt) {};

  virtual bool AdvanceStep(double t_old, double t_new, bool reinit=false); 
  virtual void CommitStep(double t_old, double t_new);
  virtual void CalculateDiagnostics() {};

  virtual std::string name() { return passwd_; }

  // main transport members
  // -- calculation of a stable time step needs saturations and darcy flux
  double CalculateTransportDt();

  // coupling with chemistry
#ifdef ALQUIMIA_ENABLED
  void SetupAlquimia(Teuchos::RCP<AmanziChemistry::Chemistry_State> chem_state,
                     Teuchos::RCP<AmanziChemistry::ChemistryEngine> chem_engine);
#endif

  // -- access members  
  inline double cfl() { return cfl_; }
  Teuchos::RCP<const State> state() { return S_; }
  Teuchos::RCP<CompositeVector> total_component_concentration() { return tcc_tmp; }

  // -- control members
  void CreateDefaultState(Teuchos::RCP<const AmanziMesh::Mesh>& mesh, int ncomponents);
  void Policy(Teuchos::Ptr<State> S);

  void VV_CheckGEDproperty(Epetra_MultiVector& tracer) const; 
  void VV_CheckTracerBounds(Epetra_MultiVector& tracer, int component,
                            double lower_bound, double upper_bound, double tol = 0.0) const;
  void VV_CheckInfluxBC() const;
  void VV_PrintSoluteExtrema(const Epetra_MultiVector& tcc_next, double dT_MPC);
  double VV_SoluteVolumeChangePerSecond(int idx_solute);

  void CalculateLpErrors(AnalyticFunction f, double t, Epetra_Vector* sol, double* L1, double* L2);

  // -- sources and sinks for components from n0 to n1 including
  void ComputeAddSourceTerms(double tp, double dtp, 
                             std::vector<TransportDomainFunction*>& src_sink, 
                             Epetra_MultiVector& tcc, int n0, int n1);

  bool PopulateBoundaryData(std::vector<int>& bc_model,
                            std::vector<double>& bc_value, int component);

  // -- limiters 
  void LimiterBarthJespersen(const int component,
                             Teuchos::RCP<const Epetra_Vector> scalar_field, 
                             Teuchos::RCP<CompositeVector>& gradient, 
                             Teuchos::RCP<Epetra_Vector>& limiter);

 private:
  void InitializeFields_();

  // advection members
  void AdvanceDonorUpwind(double dT);
  void AdvanceSecondOrderUpwindGeneric(double dT);
  void AdvanceSecondOrderUpwindRK1(double dT);
  void AdvanceSecondOrderUpwindRK2(double dT);

  // time integration members
  void Functional(const double t, const Epetra_Vector& component, Epetra_Vector& f_component);

  void IdentifyUpwindCells();

  void InterpolateCellVector(
      const Epetra_MultiVector& v0, const Epetra_MultiVector& v1, 
      double dT_int, double dT, Epetra_MultiVector& v_int);

  const Teuchos::RCP<Epetra_IntVector>& upwind_cell() { return upwind_cell_; }
  const Teuchos::RCP<Epetra_IntVector>& downwind_cell() { return downwind_cell_; }  

  // physical models
  // -- dispersion and diffusion
  void CalculateDispersionTensor_(
      const Epetra_MultiVector& darcy_flux, 
      const Epetra_MultiVector& porosity, const Epetra_MultiVector& saturation);

  void CalculateDiffusionTensor_(
      double md, int phase,
      const Epetra_MultiVector& porosity, const Epetra_MultiVector& saturation);

  int FindDiffusionValue(const std::string& tcc_name, double* md, int* phase);

  void CalculateAxiSymmetryDirection();

  // -- air-water partitioning using Henry's law. This is a temporary
  //    solution to get things moving.
  void PrepareAirWaterPartitioning_();
  void MakeAirWaterPartitioning_();

  // -- multiscale methods
  void AddMultiscalePorosity_(double t_old, double t_new, double t_int1, double t_int2);

  // initialization methods
  void InitializeAll_();
  void InitializeFieldFromField_(const std::string& field0, const std::string& field1, bool call_evaluator);
  void ProcessStringDispersionModel(const std::string name, int* model);

  // miscaleneous methods
  int FindComponentNumber(const std::string component_name);

 public:
  Teuchos::RCP<Teuchos::ParameterList> glist_;
  Teuchos::RCP<Teuchos::ParameterList> tp_list_;
  Teuchos::RCP<const Teuchos::ParameterList> preconditioner_list_;
  Teuchos::RCP<const Teuchos::ParameterList> linear_solver_list_;
  Teuchos::RCP<const Teuchos::ParameterList> nonlinear_solver_list_;

  int MyPID;  // parallel information: will be moved to private
  int spatial_disc_order, temporal_disc_order, limiter_model;

  int nsubcycles;  // output information
  int internal_tests;
  double tests_tolerance;

 protected:
  Teuchos::RCP<TreeVector> soln_;
 
 private:
  Teuchos::RCP<const AmanziMesh::Mesh> mesh_;
  Teuchos::RCP<State> S_;
  std::string passwd_;

  bool subcycling_;
  int dim;

  Teuchos::RCP<CompositeVector> tcc_tmp;  // next tcc
  Teuchos::RCP<CompositeVector> tcc;  // smart mirrow of tcc 
  Teuchos::RCP<const Epetra_MultiVector> darcy_flux;
  Teuchos::RCP<const Epetra_MultiVector> ws, ws_prev, phi;
  
#ifdef ALQUIMIA_ENABLED
  Teuchos::RCP<AmanziChemistry::Chemistry_State> chem_state_;
  Teuchos::RCP<AmanziChemistry::ChemistryEngine> chem_engine_;
#endif

  Teuchos::RCP<Epetra_IntVector> upwind_cell_;
  Teuchos::RCP<Epetra_IntVector> downwind_cell_;

  Teuchos::RCP<const Epetra_MultiVector> ws_start, ws_end;  // data for subcycling 
  Teuchos::RCP<Epetra_MultiVector> ws_subcycle_start, ws_subcycle_end;

  int current_component_;  // data for lifting
  Teuchos::RCP<Operators::ReconstructionCell> lifting_;

  std::vector<TransportDomainFunction*> srcs;  // Source or sink for components
  std::vector<TransportBoundaryFunction*> bcs;  // influx BC for components
  double bc_scaling;
  Teuchos::RCP<Epetra_Vector> Kxy;  // absolute permeability in plane xy

  Teuchos::RCP<Epetra_Import> cell_importer;  // parallel communicators
  Teuchos::RCP<Epetra_Import> face_importer;

  // mechanical dispersion and molecual diffusion
  Teuchos::RCP<MDMPartition> mdm_;
  std::vector<WhetStone::Tensor> D_;

  std::vector<Teuchos::RCP<MaterialProperties> > mat_properties_;  // vector of materials
  std::vector<Teuchos::RCP<DiffusionPhase> > diffusion_phase_;   // vector of phases

  int dispersion_models_;
  std::vector<int> axi_symmetry_;  // axi-symmetry direction of permeability tensor
  std::string dispersion_preconditioner;
  std::string dispersion_solver;

  // Hosting temporarily Henry law 
  bool henry_law_;
  std::vector<double> kH_;
  std::vector<int> air_water_map_;

  // multiscale models
  bool multiscale_porosity_;
  Teuchos::RCP<MultiscaleTransportPorosityPartition> msp_;
 
  double cfl_, dt_, dt_debug_, t_physics_;  

  std::vector<double> mass_solutes_exact_, mass_solutes_source_;  // mass for all solutes
  std::vector<std::string> runtime_solutes_;  // names of trached solutes
  std::vector<std::string> runtime_regions_;

  int ncells_owned, ncells_wghost;
  int nfaces_owned, nfaces_wghost;
  int nnodes_wghost;
 
  std::vector<std::string> component_names_;  // details of components
  int num_aqueous, num_gaseous;

  VerboseObject* vo_;

  // Forbidden.
  Transport_PK(const Transport_PK&);
  Transport_PK& operator=(const Transport_PK&);

 private:
  // factory registration
  static RegisteredPKFactory<Transport_PK> reg_;
};

}  // namespace Transport
}  // namespace Amanzi

#endif

