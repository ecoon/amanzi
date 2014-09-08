/* -*-  mode: c++; c-default-style: "google"; indent-tabs-mode: nil -*- */
/* -------------------------------------------------------------------------
ATS

License: see $ATS_DIR/COPYRIGHT
Author: Ethan Coon

Implementation for a Field.

Field also stores some basic metadata for Vis, checkpointing, etc.
------------------------------------------------------------------------- */

#include <string>

#include "exodusII.h" 

#include "dbc.hh"
#include "errors.hh"
#include "CompositeVector.hh"
#include "composite_vector_function.hh"
#include "composite_vector_function_factory.hh"

#include "Field.hh"
#include "Field_CompositeVector.hh"

namespace Amanzi {

Field_CompositeVector::Field_CompositeVector(std::string fieldname, std::string owner) :
    Field::Field(fieldname, owner), data_() {
  type_ = COMPOSITE_VECTOR_FIELD;
};

Field_CompositeVector::Field_CompositeVector(std::string fieldname, std::string owner,
        const std::vector<std::vector<std::string> >& subfield_names) :
    Field::Field(fieldname, owner),
    data_(),
    subfield_names_(subfield_names) {
  type_ = COMPOSITE_VECTOR_FIELD;
};

Field_CompositeVector::Field_CompositeVector(std::string fieldname, std::string owner,
                                           Teuchos::RCP<CompositeVector>& data) :
    Field::Field(fieldname, owner), data_(data) {
  type_ = COMPOSITE_VECTOR_FIELD;
};

// copy constructor:
Field_CompositeVector::Field_CompositeVector(const Field_CompositeVector& other) :
    Field::Field(other),
    subfield_names_(other.subfield_names_) {
  data_ = Teuchos::rcp(new CompositeVector(*other.data_));
};

// Virtual copy constructor
Teuchos::RCP<Field> Field_CompositeVector::Clone() const {
  return Teuchos::rcp(new Field_CompositeVector(*this));
}

// Virtual copy constructor with non-empty name
Teuchos::RCP<Field> Field_CompositeVector::Clone(std::string fieldname) const {
  Teuchos::RCP<Field_CompositeVector> other = Teuchos::rcp(new Field_CompositeVector(*this));
  other->fieldname_ = fieldname;
  return other;
};

// Virtual copy constructor with non-empty name
Teuchos::RCP<Field> Field_CompositeVector::Clone(std::string fieldname, std::string owner) const {
  Teuchos::RCP<Field_CompositeVector> other = Teuchos::rcp(new Field_CompositeVector(*this));
  other->fieldname_ = fieldname;
  other->owner_ = owner;
  return other;
};

// Create the data
void Field_CompositeVector::CreateData() {}

// write-access to the data
Teuchos::RCP<CompositeVector> Field_CompositeVector::GetFieldData() {
  return data_;
};

// Overwrite data by pointer, not copy
void Field_CompositeVector::SetData(const Teuchos::RCP<CompositeVector>& data) {
  data_ = data;
};

void Field_CompositeVector::SetData(const CompositeVector& data) {
  *data_ = data;
};

void Field_CompositeVector::Initialize(Teuchos::ParameterList& plist) {
  // Protect against unset names
  EnsureSubfieldNames_();

  // First try all initialization method which set the entire data structure.
  // ------ Try to set values from a restart file -----
  if (plist.isParameter("restart file")) {
    ASSERT(!initialized());
    std::string filename = plist.get<std::string>("restart file");
    ReadCheckpoint_(filename);
    set_initialized();
    return;
  }

  // ------ Try to set values from an file -----
  if (plist.isSublist("exodus file initialization")) {
    // data must be pre-initialized to zero in case Exodus file does not
    // provide all values.
    data_->PutScalar(0.0);

    Teuchos::ParameterList file_list = plist.sublist("exodus file initialization");
    ReadFromExodusII_(file_list);
    set_initialized();
    return;
  }

  // Next try all partial initialization methods -- typically cells.
  // ------ Try to set cell values from a restart file -----
  if (plist.isParameter("cells from file")) {
    ASSERT(!initialized());
    std::string filename = plist.get<std::string>("cells from file");
    ReadCellsFromCheckpoint_(filename);
  }

  // ------ Set values using a constant -----
  if (plist.isParameter("constant")) {
    ASSERT(!initialized());
    double value = plist.get<double>("constant");
    data_->PutScalar(value);
    set_initialized();
  }

  // ------ Set values from 1D solution -----
  if (plist.isSublist("initialize from 1D column")) {
    ASSERT(!initialized());
    Teuchos::ParameterList& init_plist = plist.sublist("initialize from 1D column");
    InitializeFromColumn_(init_plist);
    set_initialized();
  }

  // ------ Set values using a function -----
  if (plist.isSublist("function")) {
    Teuchos::ParameterList func_plist = plist.sublist("function");
 
    // -- potential use of a mapping operator first -- 
    bool map_normal = plist.get<bool>("dot with normal", false); 
    if (map_normal) { 
      // map_normal take a vector and dots it with face normals 
      ASSERT(data_->NumComponents() == 1); // one comp 
      ASSERT(data_->HasComponent("face")); // is named face 
      ASSERT(data_->Location("face") == AmanziMesh::FACE); // is on face 
      ASSERT(data_->NumVectors("face") == 1);  // and is scalar 
 
      // create a vector on faces of the appropriate dimension 
      int dim = data_->Mesh()->space_dimension(); 

      CompositeVectorSpace cvs;
      cvs.SetMesh(data_->Mesh());
      cvs.SetComponent("face", AmanziMesh::FACE, dim);
      Teuchos::RCP<CompositeVector> vel_vec = Teuchos::rcp(new CompositeVector(cvs));

      // Evaluate the velocity function 
      Teuchos::RCP<Functions::CompositeVectorFunction> func = 
          Functions::CreateCompositeVectorFunction(func_plist, vel_vec->Map()); 
      func->Compute(0.0, vel_vec.ptr()); 
 
      // Dot the velocity with the normal 
      unsigned int nfaces_owned = data_->Mesh() 
          ->num_entities(AmanziMesh::FACE, AmanziMesh::OWNED); 
 
      Epetra_MultiVector& dat_f = *data_->ViewComponent("face",false); 
      const Epetra_MultiVector& vel_f = *vel_vec->ViewComponent("face",false); 
 
      AmanziGeometry::Point vel(dim); 
      for (unsigned int f=0; f!=nfaces_owned; ++f) { 
        AmanziGeometry::Point normal = data_->Mesh()->face_normal(f); 
        if (dim == 2) { 
          vel.set(vel_f[0][f], vel_f[1][f]); 
        } else if (dim == 3) { 
          vel.set(vel_f[0][f], vel_f[1][f], vel_f[2][f]); 
        } else { 
          ASSERT(0); 
        } 
        dat_f[0][f] = vel * normal; 
      } 
      set_initialized(); 
      return; 
 
    } else { 
      // no map, just evaluate the function 
      Teuchos::RCP<Functions::CompositeVectorFunction> func = 
          Functions::CreateCompositeVectorFunction(func_plist, data_->Map()); 
      func->Compute(0.0, data_.ptr()); 
      set_initialized(); 
    }
  }

  // ------ Set face values by interpolation -----
  if ((data_->HasComponent("face") || data_->HasComponent("boundary_face"))  && data_->HasComponent("cell") &&
      plist.get<bool>("initialize faces from cells", false) && initialized()) {
    DeriveFaceValuesFromCellValues_();
  }

  return;
};


void Field_CompositeVector::WriteVis(const Teuchos::Ptr<Visualization>& vis) {
  if (io_vis_ && (vis->mesh() == data_->Mesh())) {
    EnsureSubfieldNames_();

    // loop over the components and dump them to the vis file if possible
    int i = 0;
    for (CompositeVector::name_iterator compname=data_->begin();
         compname!=data_->end(); ++compname) {
      // check that this vector is a cell vector (currently this is the only
      // type of vector we can visualize
      if (data_->Location(*compname) == AmanziMesh::CELL) {
        // get the MultiVector that should be dumped
        Teuchos::RCP<Epetra_MultiVector> v = data_->ViewComponent(*compname, false);

        // construct the name for vis
        std::vector< std::string > vis_names(subfield_names_[i]);
        for (unsigned int j = 0; j!=subfield_names_[i].size(); ++j) {
          vis_names[j] = fieldname_ + std::string(".") + *compname
            + std::string(".") + subfield_names_[i][j];
        }
        vis->WriteVector(*v, vis_names);
      }
      i++;
    }
  }
};


void Field_CompositeVector::WriteCheckpoint(const Teuchos::Ptr<Checkpoint>& chk) {
  if (io_checkpoint_) {
    EnsureSubfieldNames_();

    // loop over the components and dump them to the checkpoint file if possible
    int i = 0;
    for (CompositeVector::name_iterator compname=data_->begin();
         compname!=data_->end(); ++compname) {
      // get the MultiVector that should be dumped
      Teuchos::RCP<Epetra_MultiVector> v = data_->ViewComponent(*compname, false);

      // construct name for the field in the checkpoint
      std::vector<std::string> chkp_names(subfield_names_[i]);
      for (unsigned int j = 0; j!=subfield_names_[i].size(); ++j) {
        chkp_names[j] = fieldname_ + "." + *compname + "." + subfield_names_[i][j];
      }
      chk->WriteVector(*v, chkp_names);
      i++;
    }
  }
};


void Field_CompositeVector::ReadCellsFromCheckpoint_(std::string filename) {
  Teuchos::RCP<Amanzi::HDF5_MPI> file_input =
      Teuchos::rcp(new Amanzi::HDF5_MPI(data_->Comm(), filename));
  EnsureSubfieldNames_();

  int i = 0;
  for (CompositeVector::name_iterator compname=data_->begin();
       compname!=data_->end(); ++compname) {

    if (*compname == std::string("cell")) {
      // get the MultiVector that should be read
      Teuchos::RCP<Epetra_MultiVector> vec = data_->ViewComponent(*compname, false);

      // construct name for the field in the checkpoint file
      std::vector<std::string> chkp_names(subfield_names_[i]);
      for (unsigned int j = 0; j!=subfield_names_[i].size(); ++j) {
        chkp_names[j] = fieldname_ + "." + *compname + "." + subfield_names_[i][j];
      }
      for (unsigned int j = 0; j!=subfield_names_[i].size(); ++j) {
        file_input->readData(*(*vec)(j), chkp_names[j]);
      }
    }
    ++i;
  }
}

void Field_CompositeVector::ReadCheckpoint_(std::string filename) {
  Teuchos::RCP<Amanzi::HDF5_MPI> file_input =
      Teuchos::rcp(new Amanzi::HDF5_MPI(data_->Comm(), filename));
  file_input->open_h5file();
  ReadCheckpoint(file_input.ptr());
  file_input->close_h5file();
}


// modify methods
// -- set data from file
void Field_CompositeVector::ReadCheckpoint(const Teuchos::Ptr<HDF5_MPI>& file_input) {
  EnsureSubfieldNames_();

  // loop over the components and dump them to the checkpoint file if possible
  int i = 0;
  for (CompositeVector::name_iterator compname=data_->begin();
       compname!=data_->end(); ++compname) {
    // get the MultiVector that should be read
    Teuchos::RCP<Epetra_MultiVector> vec = data_->ViewComponent(*compname, false);

    // construct name for the field in the checkpoint file
    std::vector<std::string> chkp_names(subfield_names_[i]);
    for (unsigned int j = 0; j!=subfield_names_[i].size(); ++j) {
      chkp_names[j] = fieldname_ + "." + *compname + "." + subfield_names_[i][j];
    }
    for (unsigned int j = 0; j!=subfield_names_[i].size(); ++j) {
      file_input->readData(*(*vec)(j), chkp_names[j]);
    }
    i++;
  }
}


void Field_CompositeVector::InitializeFromColumn_(Teuchos::ParameterList& plist) {
  // get filename, data names
  if (!plist.isParameter("file")) {
    Errors::Message message("Missing InitializeFromColumn parameter \"file\"");
    Exceptions::amanzi_throw(message);
  }
  std::string filename = plist.get<std::string>("file");
  std::string z_str = plist.get<std::string>("z header", "/z");
  std::string f_str = std::string("/")+fieldname_;
  f_str = plist.get<std::string>("f header", f_str);

  // Create the function
  Teuchos::ParameterList func_list;
  Teuchos::ParameterList& func_sublist = func_list.sublist("function-tabular");
  func_sublist.set("file", filename);
  func_sublist.set("x header", z_str);
  func_sublist.set("y header", f_str);
  FunctionFactory fac;
  Teuchos::RCP<Function> func = Teuchos::rcp(fac.Create(func_list));
      
  // orientation
  std::string orientation = plist.get<std::string>("coordinate orientation", "standard");
  if (orientation != "standard" && orientation != "depth") {
    Errors::Message message("InitializeFromColumn parameter \"orientation\" must be either \"standard\" (bottom up) or \"depth\" (top down)");
    Exceptions::amanzi_throw(message);
  }

  // starting surface
  Teuchos::Array<std::string> sidesets;
  if (plist.isParameter("surface sideset")) {
    sidesets.push_back(plist.get<std::string>("surface sideset"));
  } else if (plist.isParameter("surface sidesets")) {
    sidesets = plist.get<Teuchos::Array<std::string> >("surface sidesets");
  } else {
    Errors::Message message("Missing InitializeFromColumn parameter \"surface sideset\" or \"surface sidesets\"");
    Exceptions::amanzi_throw(message);
  }

  // evaluate
  Epetra_MultiVector& vec = *data_->ViewComponent("cell",false);
  if (orientation == "depth") {
    double z0, z;

    AmanziMesh::Entity_ID_List surf_faces;
    for (Teuchos::Array<std::string>::const_iterator setname=sidesets.begin();
         setname!=sidesets.end(); ++setname) {
      data_->Mesh()->get_set_entities(*setname,AmanziMesh::FACE,
              AmanziMesh::OWNED, &surf_faces);

      for (AmanziMesh::Entity_ID_List::const_iterator f=surf_faces.begin();
           f!=surf_faces.end(); ++f) {
        // Collect the reference coordinate z0
        AmanziGeometry::Point x0 = data_->Mesh()->face_centroid(*f);
        z0 = x0[x0.dim()-1];

        // Iterate down the column
        AmanziMesh::Entity_ID_List cells;
        data_->Mesh()->face_get_cells(*f, AmanziMesh::OWNED, &cells);
        ASSERT(cells.size() == 1);
        AmanziMesh::Entity_ID c = cells[0];

        while (c >= 0) {
          AmanziGeometry::Point x1 = data_->Mesh()->cell_centroid(c);
          z = z0 - x1[x1.dim()-1];
          vec[0][c] = (*func)(&z);
          c = data_->Mesh()->cell_get_cell_below(c);
        }
      }
    }

  } else {
    double z0, z;

    AmanziMesh::Entity_ID_List surf_faces;
    for (Teuchos::Array<std::string>::const_iterator setname=sidesets.begin();
         setname!=sidesets.end(); ++setname) {
      data_->Mesh()->get_set_entities(*setname,AmanziMesh::FACE,
              AmanziMesh::OWNED, &surf_faces);

      for (AmanziMesh::Entity_ID_List::const_iterator f=surf_faces.begin();
           f!=surf_faces.end(); ++f) {
        // Collect the reference coordinate z0
        AmanziGeometry::Point x0 = data_->Mesh()->face_centroid(*f);
        z0 = x0[x0.dim()-1];

        // Iterate down the column
        AmanziMesh::Entity_ID_List cells;
        data_->Mesh()->face_get_cells(*f, AmanziMesh::OWNED, &cells);
        ASSERT(cells.size() == 1);
        AmanziMesh::Entity_ID c = cells[0];

        while (c >= 0) {
          AmanziGeometry::Point x1 = data_->Mesh()->cell_centroid(c);
          z = x1[x1.dim()-1] - z0;
          vec[0][c] = (*func)(&z);
          c = data_->Mesh()->cell_get_cell_above(c);
        }
      }
    }
  }
}


// -----------------------------------------------------------------------------
// Interpolate pressure ICs on cells to ICs for lambda (faces).
// -----------------------------------------------------------------------------
void Field_CompositeVector::DeriveFaceValuesFromCellValues_() {

  if (data_->HasComponent("face")){
    data_->ScatterMasterToGhosted("cell");
    Teuchos::Ptr<const CompositeVector> cv_const(data_.ptr());
    const Epetra_MultiVector& cv_c = *cv_const->ViewComponent("cell",true);
    Epetra_MultiVector& cv_f = *data_->ViewComponent("face",false);

    int f_owned = cv_f.MyLength();
    for (int f=0; f!=f_owned; ++f) {
      AmanziMesh::Entity_ID_List cells;
      cv_const->Mesh()->face_get_cells(f, AmanziMesh::USED, &cells);
      int ncells = cells.size();

      double face_value = 0.0;
      for (int n=0; n!=ncells; ++n) {
        face_value += cv_c[0][cells[n]];
      }
      cv_f[0][f] = face_value / ncells;
    }
  }
  else if (data_->HasComponent("boundary_face")){
    Teuchos::Ptr<const CompositeVector> cv_const(data_.ptr());
    const Epetra_MultiVector& cv_c = *cv_const->ViewComponent("cell",true);
    Epetra_MultiVector& cv_f = *data_->ViewComponent("boundary_face",false);

    const Epetra_Map& fb_map = cv_const->Mesh()->exterior_face_map();
    const Epetra_Map& f_map = cv_const->Mesh()->face_map(false);

    int fb_owned = cv_f.MyLength();
    for (int fb=0; fb!=fb_owned; ++fb) {
      AmanziMesh::Entity_ID_List cells;

      int f_gid = fb_map.GID(fb);
      int f_lid = f_map.LID(f_gid);
      
      cv_const->Mesh()->face_get_cells(f_lid, AmanziMesh::USED, &cells);
      int ncells = cells.size();

      ASSERT((ncells==1));

      double face_value = cv_c[0][cells[0]];
      cv_f[0][fb] = face_value;
    }
  }
};


void Field_CompositeVector::EnsureSubfieldNames_() {
  // set default values for subfield names, ensuring they are unique
  if (subfield_names_.size() == 0) {
    subfield_names_.resize(data_->NumComponents());

    unsigned int i = 0;
    for (CompositeVector::name_iterator compname=data_->begin();
         compname!=data_->end(); ++compname) {
      subfield_names_[i].resize(data_->NumVectors(*compname));

      for (unsigned int j=0; j!=subfield_names_[i].size(); ++j) {
        std::stringstream s;
        s << j;
        subfield_names_[i][j] = s.str();
      }
      ++i;
    }
  } else {
    for (int i=0; i!=subfield_names_.size(); ++i) {
      for (int j=0; j!=subfield_names_[i].size(); ++j) {
        if (subfield_names_[i][j].length() == 0) {
          std::stringstream s;
          s << j;
          subfield_names_[i][j] = s.str();
        }
      }
    }
  }
};


void Field_CompositeVector::ReadFromExodusII_(Teuchos::ParameterList& file_list) 
{ 
  Epetra_MultiVector& dat_f = *data_->ViewComponent("cell", false); 
  int nvectors = dat_f.NumVectors(); 
 
  std::string file_name = file_list.get<std::string>("file"); 
  std::string attribute_name = file_list.get<std::string>("attribute"); 
 
  // open ExodusII file 
  const Epetra_Comm& comm = data_->Comm(); 
 
  if (comm.NumProc() > 1) { 
    std::stringstream add_extension; 
    add_extension << "." << comm.NumProc() << "." << comm.MyPID(); 
    file_name.append(add_extension.str()); 
  } 
 
  int CPU_word_size(8), IO_word_size(0), ierr; 
  float version; 
  int exoid = ex_open(file_name.c_str(), EX_READ, &CPU_word_size, &IO_word_size, &version); 
  printf("Opening file: %s ws=%d %d\n", file_name.c_str(), CPU_word_size, IO_word_size); 
 
  // read database parameters 
  int dim, num_nodes, num_elem, num_elem_blk, num_node_sets, num_side_sets; 
  char title[MAX_LINE_LENGTH + 1]; 
  ierr = ex_get_init(exoid, title, &dim, &num_nodes, &num_elem, 
                     &num_elem_blk, &num_node_sets, &num_side_sets); 
 
  int* ids = (int*) calloc(num_elem_blk, sizeof(int)); 
  ierr = ex_get_elem_blk_ids(exoid, ids); 
 
  // read attributes block-by-block 
  int offset = 0; 
  char elem_type[MAX_LINE_LENGTH + 1]; 
  for (int i = 0; i < num_elem_blk; i++) { 
    int num_elem_this_blk, num_attr, num_nodes_elem; 
    ierr = ex_get_elem_block(exoid, ids[i], elem_type, &num_elem_this_blk, 
                             &num_nodes_elem, &num_attr); 
 
    double* attrib = (double*) calloc(num_elem_this_blk * num_attr, sizeof(double)); 
    ierr = ex_get_elem_attr(exoid, ids[i], attrib); 
 
    for (int n = 0; n < num_elem_this_blk; n++) { 
      int c = n + offset; 
      for (int k = 0; k < nvectors; k++) dat_f[k][c] = attrib[n]; 
    } 
    free(attrib); 
    printf("MyPID=%d  ierr=%d  id=%d  ncells=%d\n", comm.MyPID(), ierr, ids[i], num_elem_this_blk); 
 
    offset += num_elem_this_blk; 
  } 
 
  ierr = ex_close(exoid); 
  printf("Closing file: %s ncells=%d error=%d\n", file_name.c_str(), offset, ierr); 
} 


long int Field_CompositeVector::GetLocalElementCount() {
  long int count(0);
  for (CompositeVector::name_iterator compname=data_->begin();
       compname!=data_->end(); ++compname) {
    // get the MultiVector that should be dumped
    Teuchos::RCP<Epetra_MultiVector> v = data_->ViewComponent(*compname, false);
    count += v->NumVectors() * v->MyLength();
  }
  return count;
}


} // namespace Amanzi
