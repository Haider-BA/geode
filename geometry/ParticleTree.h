//#####################################################################
// Class ParticleTree
//#####################################################################
#pragma once

#include <other/core/geometry/forward.h>
#include <other/core/geometry/BoxTree.h>
namespace other{

template<class TV> class ParticleTree : public BoxTree<TV>
{
  typedef typename TV::Scalar T;
public:
  OTHER_DECLARE_TYPE
  typedef BoxTree<TV> Base;
  using Base::leaves;using Base::prims;using Base::boxes;using Base::update_nonleaf_boxes;

  const Array<const TV> X;

protected:
  OTHER_CORE_EXPORT ParticleTree(Array<const TV> X, int leaf_size);
public:
  ~ParticleTree();

  void update() OTHER_CORE_EXPORT; // Call whenever X changes
  Array<int> remove_duplicates(T tolerance) const OTHER_CORE_EXPORT; // Returns map from point to component index

  template<class Shape>
  OTHER_CORE_EXPORT void intersection(const Shape& box, Array<int>& hits) const;
};

}
