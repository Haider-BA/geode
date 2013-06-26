//#####################################################################
// Class Springs
//#####################################################################
#include <other/core/force/Springs.h>
#include <other/core/array/NdArray.h>
#include <other/core/array/ProjectedArray.h>
#include <other/core/array/view.h>
#include <other/core/structure/Hashtable.h>
#include <other/core/math/cube.h>
#include <other/core/python/Class.h>
#include <other/core/vector/SolidMatrix.h>
#include <other/core/vector/SymmetricMatrix.h>
#include <other/core/vector/normalize.h>
namespace other {

typedef real T;
template<> OTHER_DEFINE_TYPE(Springs<Vector<real,3>>)

template<class TV> Springs<TV>::Springs(Array<const Vector<int,2>> springs, Array<const T> mass, Array<const TV> X, NdArray<const T> stiffness, NdArray<const T> damping_ratio)
  : springs(springs)
  , resist_compression(true)
  , off_axis_damping(0)
  , nodes_(X.size())
  , mass(mass)
  , info(springs.size(),false) {
  OTHER_ASSERT(!springs.size() || scalar_view(springs).max()<nodes_);
  OTHER_ASSERT(mass.size()==nodes_);
  OTHER_ASSERT(stiffness.rank()==0 || (stiffness.rank()==1 && stiffness.shape[0]==springs.size()));
  OTHER_ASSERT(damping_ratio.rank()==0 || (damping_ratio.rank()==1 && damping_ratio.shape[0]==springs.size()));

  for (int s=0;s<springs.size();s++) {
    int i,j;springs[s].get(i,j);
    SpringInfo<TV>& I = info[s];
    I.restlength = magnitude(X[i]-X[j]);
    T harmonic_mass = 1/(1/mass[i]+1/mass[j]);
    T stiffness_ = stiffness.rank()?stiffness[s]:stiffness();
    T damping_ratio_ = damping_ratio.rank()?damping_ratio[s]:damping_ratio();
    I.stiffness = stiffness_*harmonic_mass/sqr(I.restlength);
    I.damping = 2*damping_ratio_*harmonic_mass*sqrt(stiffness_)/sqr(I.restlength);
  }
}

template<class TV> Springs<TV>::~Springs() {}

template<class TV> Array<T> Springs<TV>::restlengths() const {
  return info.template project<T,&SpringInfo<TV>::restlength>().copy();
}

template<class TV> int Springs<TV>::nodes() const {
  return nodes_;
}

template<class TV> void Springs<TV>::
structure(SolidMatrixStructure& structure) const {
  OTHER_ASSERT(structure.size()>=nodes_);
  for (int s=0;s<springs.size();s++) {
    int i,j;springs[s].get(i,j);
    structure.add_entry(i,j);
  }
}

template<class TV> void Springs<TV>::update_position(Array<const TV> X_, bool definite) {
  OTHER_ASSERT(X_.size()==nodes_);
  X = X_;
  for (int s=0;s<springs.size();s++) {
    int i,j;springs[s].get(i,j);
    SpringInfo<TV>& I = info[s];
    I.direction = X[j]-X[i];
    I.length = I.direction.normalize();
    I.alpha = 0;
    if (!resist_compression && I.length<I.restlength)
      I.beta = 0;
    else {
      I.beta = I.stiffness;
      if (I.length>(definite?I.restlength:(T).01*I.restlength)) {
        T rotational = I.stiffness*(1-I.restlength/I.length);
        I.alpha += rotational;
        I.beta -= rotational;
      }
    }
  }
}

template<class TV> void Springs<TV>::add_frequency_squared(RawArray<T> frequency_squared) const {
  OTHER_ASSERT(frequency_squared.size()==nodes_);
  for (int s=0;s<springs.size();s++) {
    int i,j;springs[s].get(i,j);
    const SpringInfo<TV>& I=info[s];
    frequency_squared[i] += 4*I.stiffness/mass[i];
    frequency_squared[j] += 4*I.stiffness/mass[j];
  }
}

template<class TV> T Springs<TV>::elastic_energy() const {
  T energy = 0;
  if (resist_compression)
    for (int s=0;s<springs.size();s++) {
      int i,j;springs[s].get(i,j);
      const SpringInfo<TV>& I = info[s];
      energy += I.stiffness*sqr(I.length-I.restlength);
    }
  else
    for (int s=0;s<springs.size();s++) {
      int i,j;springs[s].get(i,j);
      const SpringInfo<TV>& I=info[s];
      if (I.length>I.restlength)
        energy += I.stiffness*sqr(I.length-I.restlength);
    }
  return energy/2;
}

template<class TV> void Springs<TV>::add_elastic_force(RawArray<TV> F) const {
  OTHER_ASSERT(F.size()==nodes_);
  for (int s=0;s<springs.size();s++) {
    int i,j;springs[s].get(i,j);
    const SpringInfo<TV>& I=info[s];
    TV f = I.stiffness*(I.length-I.restlength)*I.direction;
    F[i] += f;
    F[j] -= f;
  }
}

template<class TV> void Springs<TV>::add_elastic_differential(RawArray<TV> dF, RawArray<const TV> dX) const {
  OTHER_ASSERT(dF.size()==nodes_);
  OTHER_ASSERT(dX.size()==nodes_);
  for (int s=0;s<springs.size();s++) {
    int i,j;springs[s].get(i,j); 
    const SpringInfo<TV>& I=info[s];
    TV dx = dX[j]-dX[i];
    TV f = I.alpha*dx+I.beta*dot(dx,I.direction)*I.direction;
    dF[i] += f;
    dF[j] -= f;
  }
}

template<class TV> void Springs<TV>::add_elastic_gradient(SolidMatrix<TV>& matrix) const {
  OTHER_ASSERT(matrix.size()==nodes_);
  for (int s=0;s<springs.size();s++) {
    int i,j;springs[s].get(i,j);
    const SpringInfo<TV>& I=info[s];
    SymmetricMatrix<T,3> A = scaled_outer_product(I.beta,I.direction)+I.alpha;
    matrix.add_entry(i,-A);
    matrix.add_entry(i,j,A);
    matrix.add_entry(j,-A);
  }
}

template<class TV> void Springs<TV>::add_elastic_gradient_block_diagonal(RawArray<SymmetricMatrix<T,m>> dFdX) const {
  OTHER_ASSERT(dFdX.size()==nodes_);
  for (int s=0;s<springs.size();s++) {
    int i,j;springs[s].get(i,j); 
    const SpringInfo<TV>& I = info[s];
    SymmetricMatrix<T,m> A = scaled_outer_product(I.beta,I.direction)+I.alpha;
    dFdX[i] -= A;
    dFdX[j] -= A;
  }
}

template<class TV> T Springs<TV>::damping_energy(RawArray<const TV> V) const {
  OTHER_ASSERT(V.size()==nodes_);
  T energy=0;
  if (!off_axis_damping)
    for (int s=0;s<springs.size();s++) {
      int i,j;springs[s].get(i,j);
      const SpringInfo<TV>& I=info[s];
      energy += I.damping*sqr(dot(V[j]-V[i],I.direction));
    }
  else {
    const T alpha = off_axis_damping,
            beta = 1-off_axis_damping;
    for (int s=0;s<springs.size();s++) {
      int i,j;springs[s].get(i,j);
      const SpringInfo<TV>& I=info[s];
      TV dv = V[j]-V[i];
      energy += I.damping*(alpha*sqr_magnitude(dv)+beta*sqr(dot(dv,I.direction)));
    }
  }
  return energy/2;
}

template<class TV> void Springs<TV>::add_damping_force(RawArray<TV> force,RawArray<const TV> V) const {
  OTHER_ASSERT(V.size()==nodes_);
  OTHER_ASSERT(force.size()==nodes_);
  if (!off_axis_damping)
    for (int s=0;s<springs.size();s++) {
      int i,j;springs[s].get(i,j);
      const SpringInfo<TV>& I=info[s];
      TV f = I.damping*dot(V[j]-V[i],I.direction)*I.direction;
      force[i]+=f;force[j]-=f;
    }
  else {
    const T alpha = off_axis_damping,
            beta = 1-off_axis_damping;
    for (int s=0;s<springs.size();s++) {
      int i,j;springs[s].get(i,j);
      const SpringInfo<TV>& I=info[s];
      TV dv = V[j]-V[i];
      TV f = alpha*I.damping*dv+beta*I.damping*dot(dv,I.direction)*I.direction;
      force[i] += f;
      force[j] -= f;
    }
  }
}

template<class TV> void Springs<TV>::add_damping_gradient(SolidMatrix<TV>& matrix) const {
  OTHER_ASSERT(matrix.size()==nodes_);
  if (!off_axis_damping)
    for (int s=0;s<springs.size();s++) {
      int i,j;springs[s].get(i,j);
      const SpringInfo<TV>& I=info[s];
      SymmetricMatrix<T,3> A = scaled_outer_product(I.damping,I.direction);
      matrix.add_entry(i,-A);
      matrix.add_entry(i,j,A);
      matrix.add_entry(j,-A);
    }
  else {
    const T alpha = off_axis_damping,
            beta = 1-off_axis_damping;
    for (int s=0;s<springs.size();s++) {
      int i,j;springs[s].get(i,j);
      const SpringInfo<TV>& I=info[s];
      SymmetricMatrix<T,3> A = scaled_outer_product(beta*I.damping,I.direction);
      A += alpha*I.damping;
      matrix.add_entry(i,-A);
      matrix.add_entry(i,j,A);
      matrix.add_entry(j,-A);
    }
  }
}

template<class TV> T Springs<TV>::strain_rate(RawArray<const TV> V) const {
  T max_strain_rate = 0;
  for (int s=0;s<springs.size();s++) {
    int i,j;springs[s].get(i,j);
    const SpringInfo<TV>& I = info[s];
    T strain_rate = dot(V[j]-V[i],I.direction)/I.restlength;
    max_strain_rate = max(max_strain_rate,abs(strain_rate));
  }
  return max_strain_rate;
}

template<class TV> Box<T> Springs<TV>::limit_strain(RawArray<TV> X) const {
  Box<T> F_range = strain_range+1;
  Box<T> F_range_before(1);
  for (int iter=0;iter<100;iter++) {
    for (int s=0;s<springs.size();s++) {
      int i,j;springs[s].get(i,j);
      const SpringInfo<TV>& I = info[s]; 
      TV dx = X[j]-X[i];
      T length = normalize(dx);
      T F = length/I.restlength;
      F_range_before.enlarge(F);
      if (F_range.lazy_inside(F))
        continue;
      TV change = (F_range.clamp(F)*I.restlength-length)*dx;
      T alpha = mass[i]/(mass[i]+mass[j]); 
      X[i] -= (1-alpha)*change;
      X[j] += alpha*change;

      T fixed_F = magnitude(X[j]-X[i])/I.restlength;
      OTHER_ASSERT(abs(fixed_F-F_range.clamp(fixed_F))<1e-6);
    }
  }
  return F_range_before-1;
}

}
using namespace other;

void wrap_springs() {
  typedef real T;
  typedef Vector<T,3> TV;
  typedef Springs<TV> Self;
  Class<Self>("Springs")
    .OTHER_INIT(Array<const Vector<int,2>>,Array<const T>,Array<const TV>,NdArray<const T>,NdArray<const T>)
    .OTHER_METHOD(restlengths)
    .OTHER_FIELD(springs)
    .OTHER_FIELD(resist_compression)
    .OTHER_FIELD(strain_range)
    .OTHER_FIELD(off_axis_damping)
    .OTHER_METHOD(limit_strain)
    ;
}
