//#####################################################################
// Class Value
//#####################################################################
#pragma once

#include <geode/value/forward.h>
#include <geode/python/Object.h>
#include <geode/python/try_convert.h>
#include <geode/python/ExceptionValue.h>
#include <geode/utility/type_traits.h>
#include <geode/vector/Vector.h>
extern void wrap_value_base();

#include <geode/python/Ptr.h>
#include <vector>

namespace geode {

using std::vector;
using std::type_info;

class GEODE_CORE_CLASS_EXPORT ValueBase : public Object, public WeakRefSupport {
public:
  GEODE_DECLARE_TYPE(GEODE_CORE_EXPORT)
  typedef Object Base;
private:
  friend class Action;
  template<class T> friend class Value;
  mutable bool dirty_; // are we up to date?
  mutable ExceptionValue error; // is the value an exception?

private:
  string name_;

  // A link between an Action and a Value
  struct Link
  {
    Link **value_prev, *value_next; // pointers for the value's linked list
    Link **action_prev, *action_next; // pointers for the action's linked list
    const ValueBase* value; // used only by dump()
    Action* action;
  };
  mutable Link* actions; // linked list of links to actions which depend on us
  static Link* pending; // linked list of pending signals

protected:
  GEODE_CORE_EXPORT ValueBase(string const &name = string());

public:
  GEODE_CORE_EXPORT virtual ~ValueBase();

#ifdef GEODE_PYTHON
  virtual Ref<> get_python() const = 0;

  Ref<> operator()() const {
    return get_python();
  }
#endif

  bool is_prop() const;
  virtual const type_info& type() const = 0;

  bool dirty() const {
    return dirty_;
  }

  template<class T> const Value<T>* cast() const {
    return is_type(typeid(T)) ? static_cast<const Value<T>*>(this) : 0;
  }

  GEODE_CORE_EXPORT bool is_type(const type_info& type) const;
  GEODE_CORE_EXPORT void signal() const;

  virtual void dump(int indent) const = 0;

  // things that depend on us
  virtual vector<Ref<const ValueBase>> dependents() const;

  // things that depend on us, even indirectly
  virtual vector<Ref<const ValueBase>> all_dependents() const;

  // things we depend on
  virtual vector<Ref<const ValueBase>> dependencies() const = 0;

  // things we depend on, even indirectly
  virtual vector<Ref<const ValueBase>> all_dependencies() const;

  const string& name() const;

private:
  GEODE_CORE_EXPORT void pull() const;

  virtual void update() const = 0;
  static inline void signal_pending();

  // The following exist only for python purposes: they throw exceptions if the ValueBase isn't a PropBase.
  PropBase& prop();
  const PropBase& prop() const;
  ValueBase& set_help(const string& h);
  ValueBase& set_category(const string& c);
  ValueBase& set_hidden(bool h);
  ValueBase& set_required(bool r);
  ValueBase& set_abbrev(char a);
  const string& get_help() const;
  const string& get_category() const;
  bool get_hidden() const;
  bool get_required() const;
  char get_abbrev() const;
  friend void ::wrap_value_base();
#ifdef GEODE_PYTHON
  ValueBase& set_python(PyObject* value_);
  ValueBase& set_allowed(PyObject* v);
  ValueBase& set_min_py(PyObject* m);
  ValueBase& set_max_py(PyObject* m);
  Ref<> get_min_py() const;
  Ref<> get_max_py() const;
  ValueBase& set_step_py(PyObject* s);
  Ref<> get_default() const;
  Ref<> get_allowed() const;
#endif
};

// Forward declare from Action.h for friending from Value, as this needs to call set_value
template<class T> void set_value_and_dependencies(ValueRef<T>& value, const T& v,
                                                  const vector<const ValueBase*>& dependencies);

template<class T> class GEODE_CORE_CLASS_EXPORT Value : public ValueBase
{
  static_assert(!is_const<T>::value,"T can't be const");
  static_assert(!is_reference<T>::value,"T can't be a reference");
  static_assert(!is_same<T,char*>::value && !is_same<T,const char*>::value,"T can't be char*");
public:
  typedef ValueBase Base;
    typedef T ValueType;

protected:
  // The cached value if !dirty_
  mutable typename aligned_storage<sizeof(T),alignment_of<T>::value>::type buffer;

  Value(const string& name = string())
    : ValueBase(name) {}

  ~Value() {
    if (!dirty_)
      static_cast<const T*>(static_cast<const void*>(&buffer))->~T();
  }

  void set_dirty() const {
    if (!dirty_) {
      dirty_ = true;
      error = ExceptionValue();
      static_cast<const T*>(static_cast<const void*>(&buffer))->~T();
      signal();
    }
  }

  template<class S> friend void geode::set_value_and_dependencies(ValueRef<S>& value, const S& v,
                                                                  const vector<const ValueBase*>& dependencies);

  void set_value(const T& value) const {
    if (!dirty_)
      static_cast<const T*>(static_cast<const void*>(&buffer))->~T();
    dirty_ = false;
    error = ExceptionValue();
    new (&buffer) T(value);
    signal();
  }

public:
  const T& operator()() const {
    pull();
    return *static_cast<const T*>(static_cast<const void*>(&buffer));
  }

#ifdef GEODE_PYTHON
  Ref<> get_python() const {
    pull();
    return try_to_python_ref(*static_cast<const T*>(static_cast<const void*>(&buffer)));
  }
#endif

  const type_info& type() const {
    return typeid(T);
  }

  // Look at a value without adding a dependency graph node
  const T& peek() const {
    GEODE_ASSERT(!dirty_);
    return *static_cast<const T*>(static_cast<const void*>(&buffer));
  }
};


template<class T> class ValueRef
{
  static_assert(!is_const<T>::value,"T can't be const");
  static_assert(!is_reference<T>::value,"T can't be a reference");
public:
  typedef T ValueType;

  Ref<const Value<T>> self;

  ValueRef(const Value<T>& value)
    : self(geode::ref(value)) {}

  ValueRef(const PropRef<T>& prop)
    : self(prop.self) {}

  ValueRef(const PropRef<const T>& prop)
    : self(prop.self) {}

  const T& operator()() const {
    return (*self)();
  }

  const Value<T>& operator*() const {
    return *self;
  }

  const Value<T>* operator->() const {
    return &*self;
  }

  operator const Value<T>&() const {
    return *self;
  }

  bool operator==(const ValueRef& v) const {
    return self == v.self;
  }

  friend ostream& operator<<(ostream& output,const ValueRef& v){
    output << "ValueRef(" << v.self->name() << ')';
    return output;
  }

};

template<class T> static inline PyObject* to_python(const ValueRef<T>& value) {
  return to_python(*value.self);
}

// from_python<ValueRef<T>> is declared in convert.h to avoid circularity

}
