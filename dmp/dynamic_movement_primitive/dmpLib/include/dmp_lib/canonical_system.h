/*********************************************************************
 Computational Learning and Motor Control Lab
 University of Southern California
 Prof. Stefan Schaal
 *********************************************************************
 \remarks		...
 
 \file		canonical_system.h

 \author	Peter Pastor, Mrinal Kalakrishnan
 \date		Nov 4, 2010

 *********************************************************************/

#ifndef CANONICAL_SYSTEM_BASE_H_
#define CANONICAL_SYSTEM_BASE_H_

// system include
#include <boost/shared_ptr.hpp>
#include <Eigen/Eigen>

// local include
#include <dmp_lib/canonical_system_parameters.h>
#include <dmp_lib/canonical_system_state.h>
#include <dmp_lib/time.h>
#include <dmp_lib/status.h>

namespace dmp_lib
{

/*!
 */
class CanonicalSystem : public Status
{

  /*! Allow the DynamicMovementPrimitive class to access private member variables directly
   */
  friend class DynamicMovementPrimitive;

public:

  /*! Constructor
   */
  CanonicalSystem() {};

  /*! Destructor
   */
  virtual ~CanonicalSystem() {};

  /*!
   * @param parameters
   * @param state
   * @return True on success, otherwise False
   */
  bool initialize(const CSParamPtr parameters,
                  const CSStatePtr state);

  /*!
   * @param other_cs
   * @return True on success, otherwise False
   */
  bool isCompatible(const CanonicalSystem& other_cs) const;

  /*!
   * @param parameters
   * @param state
   * @return True on success, otherwise False
   */
  bool get(CSParamConstPtr& parameters,
           CSStateConstPtr& state) const;

  /*! Reset the canonical system
   */
  virtual void reset() = 0;

  /*! Integrate the canonical system
   * @param dmp_time
   * @return True on success, otherwise False
   * REAL-TIME REQUIREMENTS
   */
  virtual bool integrate(const Time& dmp_time) = 0;

  /*!
   * @return
   * REAL-TIME REQUIREMENTS
   */
  virtual double getTime() const = 0;

  /*! Sets the rollout with num_time_steps time steps.
   * @param num_time_steps
   * @param cutoff
   * @return True on success, otherwise False
   */
  virtual bool getRollout(const int num_time_steps, const double cutoff, Eigen::VectorXd& rollout) const = 0;

  /*!
   * @return
   */
  CSParamPtr getParameters() const;

  /*!
   * @return
   */
  CSStatePtr getState() const;

protected:

  /*!
   */
  CSParamPtr parameters_;

  /*!
   */
  CSStatePtr state_;

};

/*! Abbreviation for convenience
 */
typedef boost::shared_ptr<CanonicalSystem> CSPtr;
typedef boost::shared_ptr<CanonicalSystem const> CSConstPtr;

// Inline definitions follow
inline CSParamPtr CanonicalSystem::getParameters() const
{
  assert(initialized_);
  return parameters_;
}
inline CSStatePtr CanonicalSystem::getState() const
{
  assert(initialized_);
  return state_;
}

}

#endif /* CANONICAL_SYSTEM_BASE_H_ */
