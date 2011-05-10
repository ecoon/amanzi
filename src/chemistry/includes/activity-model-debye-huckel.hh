/* -*-  mode: c++; c-default-style: "google"; indent-tabs-mode: nil -*- */
#ifndef __ACTIVITY_MODEL_DEBYE_HUCKEL_HH__
#define __ACTIVITY_MODEL_DEBYE_HUCKEL_HH__

// Base class for activity calculations

#include "activity-model.hh"

class Species;

class ActivityModelDebyeHuckel : public ActivityModel {
 public:
  ActivityModelDebyeHuckel();
  ~ActivityModelDebyeHuckel();

  double Evaluate(const Species& species);

  void Display(void) const;

 protected:

 private:
  static const double debyeA;
  static const double debyeB;
  static const double debyeBdot;
};

#endif  // __ACTIVITY_MODEL_DEBYE_HUCKEL_HPP__

