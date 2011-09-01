
#include "Explicit_TI_RK.hpp"

Explicit_TI::RK::RK(Explicit_TI::fnBase& fn_, 
		    const Explicit_TI::RK::method_t method_, 
		    const Epetra_MultiVector& example_vector):
  fn(fn_)

{ 
  set_method(method_);
  create_storage(example_vector); 
}


Explicit_TI::RK::~RK()
{
  delete_storage();
}

void Explicit_TI::RK::set_method(const Explicit_TI::RK::method_t method_)
{
  method = method_;
  
  switch (method) {
  case forward_euler: 
    order = 1;
    break;
  case heun_method:
    order = 2;
    break;
  case kutta_3rd_order:
    order = 3;
    break;
  case runge_kutta_4th_order:
    order = 4;
    break;
  }

  a.resize(order, order);
  b.resize(order);
  c.resize(order);

  // initialize the RK tableau
  switch (method) {

  case forward_euler:
    b[0] = 1.0;
    c[0] = 0.0;
    break;

  case heun_method:
    a(1,0) = 1.0;

    b[0] = 0.5;
    b[1] = 0.5;

    c[0] = 0.0;
    c[1] = 1.0;
    break;

  case kutta_3rd_order:
    a(1,0) = 0.5;
    a(2,0) = -1.0;
    
    a(2,1) = 2.0;

    b[0] = 1.0/6.0;
    b[1] = 2.0/3.0;
    b[2] = 1.0/6.0;

    c[0] = 0.0;
    c[1] = 0.5;
    c[2] = 1.0;
    break;

  case runge_kutta_4th_order:
    a(1,0) = 0.5;
    a(2,0) = 0.0;
    a(3,0) = 0.0;
    
    a(2,1) = 0.5;
    a(3,2) = 0.0;
    
    a(3,2) = 1.0;
    
    b[0] = 1.0/6.0;
    b[1] = 1.0/3.0;
    b[2] = 1.0/3.0;
    b[3] = 1.0/6.0;

    c[0] = 0.0;
    c[1] = 0.5;
    c[2] = 0.5;
    c[3] = 1.0;

  }
}


void Explicit_TI::RK::create_storage(const Epetra_MultiVector& example_vector)
{
  k.resize(order);
  for (int i=0; i<order; i++)
    {
      k[i] = new Epetra_MultiVector(example_vector);
    }
}


void Explicit_TI::RK::delete_storage()
{
  for (int i=0; i<k.size(); i++)
    {
      delete k[i];
    }
}





void Explicit_TI::RK::step(const double t, const double h, const Epetra_MultiVector& y, Epetra_MultiVector& y_new)
{
  std::vector<Epetra_MultiVector*> k;
  k.resize(order);
  for (int i=0; i<order; i++)
    {
      k[i] = new Epetra_MultiVector(y);
    }
  
  
  Epetra_MultiVector sum_vec(y);
  double sum_time;
  for (int i=0; i<order; i++) 
    {
      sum_time = t + c[i]*h;
      
      sum_vec = y;
      
      for (int j=0; j<i; j++) 
        {
          if (a(i,j) != 0.0) 
	    {
	      sum_vec.Update(a(i,j), *k[j], 1.0);
	    }
	}
      fn.fun(sum_time, sum_vec, *k[i]);
      k[i]->Scale(h);
    }

  
  y_new = y;
      
  for (int i=0; i<order; i++)
    {
      if (b[i] != 0.0) 
        {
           y_new.Update(b[i],*k[i],1.0);
        }
    }


}
