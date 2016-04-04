/*
  Mesh

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Authors: Lipnikov Konstantin (lipnikov@lanl.gov)
           Rao Garimella (rao@lanl.gov)

  Implementation of algorithms independent of the actual mesh 
  framework.
*/

#include <map>
#include <string>
#include <vector>

#include "errors.hh"
#include "Point.hh"
#include "Region.hh"

#include "Mesh.hh"
#include "MeshDefs.hh"

namespace Amanzi {
namespace AmanziMesh {

void Mesh::get_set_entities_box_vofs_(
    Teuchos::RCP<const AmanziGeometry::Region> region,
    const Entity_kind kind, 
    const Parallel_type ptype, 
    std::vector<Entity_ID>* setents,
    std::vector<double>* volume_fractions) const
{
  ASSERT(volume_fractions != NULL);
  /*
  std::cout << "region=" << region << std::endl;
  std::cout << "kind=" << kind << std::endl;
  std::cout << "ptype=" << ptype << std::endl;
  std::cout << "data=" << setents << std::endl;
  */

  setents->clear();
  volume_fractions->clear();

  int sdim = space_dimension();
  int mdim = region->manifold_dimension();

  switch (kind) {      
  case CELL:
  {
    std::string name = region->name() + "_cell";

    if (region_ids.find(name) != region_ids.end()) {
      *setents = region_ids[name];
      *volume_fractions = region_vofs[name];
    } else {
      int ncells = num_entities(CELL, ptype);  
      double volume;

      std::vector<AmanziGeometry::Point> polytope;

      for (int c = 0; c < ncells; ++c) {
        cell_get_coordinates(c, &polytope);
        if ((volume = region->intersect(polytope)) > 0.0) {
          setents->push_back(c);
          volume_fractions->push_back(volume);
        }
      }

      region_ids[name] = *setents;
      region_vofs[name] = *volume_fractions;
    }
    break;
  }

  case FACE:
  {
    std::string name = region->name() + "_face";

    if (region_ids.find(name) != region_ids.end()) {
      *setents = region_ids[name];
      *volume_fractions = region_vofs[name];
    } else {
      int nfaces = num_entities(FACE, ptype);
      double area;
        
      std::vector<AmanziGeometry::Point> polygon;

      for (int f = 0; f < nfaces; ++f) {
        face_get_coordinates(f, &polygon);
        if ((area = region->intersect(polygon)) > 0.0) {
          setents->push_back(f);
          volume_fractions->push_back(area);
        }
      }

      region_ids[name] = *setents;
      region_vofs[name] = *volume_fractions;
    }
    break;
  }

  case EDGE:
  {
    int nedges = num_entities(EDGE, ptype);
        
    for (int e = 0; e < nedges; ++e) {
      if (region->inside(edge_centroid(e))) {
        setents->push_back(e);
      }
    }
    break;
  }

  case NODE:
  {
    int nnodes = num_entities(NODE, ptype);
        
    AmanziGeometry::Point xv(space_dimension());

    for (int v = 0; v < nnodes; ++v) {
      node_get_coordinates(v, &xv);
      if (region->inside(xv)) {
        setents->push_back(v);
      }
    }
    break;
  }

  default:
    break;
  }

  // Check if no processor got any mesh entities
  int nents, nents_tmp = setents->size();
  get_comm()->SumAll(&nents_tmp, &nents, 1);
  if (nents == 0) {
    Errors::Message msg;
    msg << "Could not retrieve any mesh entities for set \"" << region->name() << "\".\n";
    Exceptions::amanzi_throw(msg);
  }
}


//---------------------------------------------------------
// Generic implemnetation of set routines.
//---------------------------------------------------------
unsigned int Mesh::get_set_size(const Set_ID setid,
                                const Entity_kind kind,
                                const Parallel_type ptype) const 
{
  Entity_ID_List ents;
  std::string setname = geometric_model()->FindRegion(setid)->name();

  get_set_entities(setname, kind, ptype, &ents);

  return ents.size();
}


unsigned int Mesh::get_set_size(const std::string setname, 
                                const Entity_kind kind, 
                                const Parallel_type ptype) const 
{
  Entity_ID_List setents;
  std::vector<double> vofs;

  get_set_entities(setname, kind, ptype, &setents, &vofs);
  
  return setents.size();
}


void Mesh::get_set_entities(const Set_ID setid, 
                            const Entity_kind kind, 
                            const Parallel_type ptype, 
                            Entity_ID_List *entids) const
{
  std::vector<double> vofs;
  std::string setname = geometric_model()->FindRegion(setid)->name();
  get_set_entities(setname, kind, ptype, entids, &vofs);
}

}  // namespace AmanziMesh
}  // namespace Amanzi
