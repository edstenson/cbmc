/*******************************************************************\

 Module: analyses variable-sensitivity

 Author: Thomas Kiley, thomas.kiley@diffblue.com

 abstract_objectt is the top of the inheritance heirarchy of objects
 used to represent individual variables in the general non-relational
 domain.  It is a two element abstraction (i.e. it is either top or
 bottom).  Within the hierachy of objects under it, child classes are
 more precise abstractions (the converse doesn't hold to avoid
 diamonds and inheriting unnecessary fields).  Thus the common parent
 of two classes is an abstraction capable of representing both.  This
 is important for understanding merge.

 These objects are intended to be used in a copy-on-write style, which
 is why their interface differs a bit from ai_domain_baset's
 modify-in-place style of interface.

 Although these can represent bottom (this variable cannot take any
 value) it is not common for them to do so.

\*******************************************************************/
#ifndef CPROVER_ANALYSES_VARIABLE_SENSITIVITY_ABSTRACT_OBJECT_H
#define CPROVER_ANALYSES_VARIABLE_SENSITIVITY_ABSTRACT_OBJECT_H



#include <memory>
#include <map>
#include <iosfwd>

#include <util/expr.h>

class typet;
class constant_exprt;
class abstract_environmentt;
class namespacet;


#define CLONE \
  virtual abstract_objectt *mutable_clone() const override \
  { \
    typedef std::remove_const<std::remove_reference<decltype(*this)>::type \
      >::type current_typet; \
    return new current_typet(*this); \
  } \


/* Merge is designed to allow different abstractions to be merged
 * gracefully.  There are two real use-cases for this:
 *
 *  1. Having different abstractions for the variable in different
 *     parts of the program.
 *  2. Allowing different domains to write to ambiguous locations
 *     for example, if a stores multiple values (maybe one per
 *     location) with a constant for each, i does not represent one
 *     single value (top, non-unit interval, etc.) and v is something
 *     other than constant, then
 *         a[i] = v
 *     will cause this to happen.
 *
 * To handle this, merge is dispatched to the first abstract object being
 * merged, which switches based on the type of the other object. If it can
 * merge then it merges, otherwise it calls the parent merge.
 */

template<class T>
using sharing_ptrt=std::shared_ptr<const T>; // NOLINT(*)

typedef sharing_ptrt<class abstract_objectt> abstract_object_pointert;

class abstract_objectt
{
public:
  explicit abstract_objectt(const typet &type);
  abstract_objectt(const typet &type, bool top, bool bottom);
  abstract_objectt(
    const exprt &expr,
    const abstract_environmentt &environment,
    const namespacet &ns);

  virtual ~abstract_objectt() {}

  const typet &type() const;
  virtual bool is_top() const;
  virtual bool is_bottom() const;

  // Interface for transforms
  abstract_object_pointert expression_transform(
    const exprt &expr,
    const abstract_environmentt &environment,
    const namespacet &ns) const;

  virtual exprt to_constant() const;

  virtual void output(
    std::ostream &out, const class ai_baset &ai, const namespacet &ns) const;

  abstract_object_pointert clone() const
  {
    return abstract_object_pointert(mutable_clone());
  }

  static abstract_object_pointert merge(
    abstract_object_pointert op1,
    abstract_object_pointert op2,
    bool &out_modifications);

private:
  // To enforce copy-on-write these are private and have read-only accessors
  typet t;
  bool bottom;
protected:
  template<class T>
  using internal_sharing_ptrt=std::shared_ptr<T>;

  typedef internal_sharing_ptrt<class abstract_objectt>
    internal_abstract_object_pointert;

  // Macro is not used as this does not override
  virtual abstract_objectt *mutable_clone() const
  {
    return new abstract_objectt(*this);
  }

  bool top;

  // The one exception is merge in descendant classes, which needs this
  void make_top() { top=true; }

  // Sets the state of this object
  virtual const abstract_objectt *merge(abstract_object_pointert other) const;

  template<class keyt>
  static bool merge_maps(
    const std::map<keyt, abstract_object_pointert> &map1,
    const std::map<keyt, abstract_object_pointert> &map2,
    std::map<keyt, abstract_object_pointert> &out_map);
};

template<typename keyt>
bool abstract_objectt::merge_maps(
  const std::map<keyt, abstract_object_pointert> &m1,
  const std::map<keyt, abstract_object_pointert> &m2,
  std::map<keyt, abstract_object_pointert> &out_map)
{
  out_map.clear();

  typedef std::map<keyt, abstract_object_pointert> abstract_object_mapt;

  typename abstract_object_mapt::const_iterator it1=m1.begin();
  typename abstract_object_mapt::const_iterator it2=m2.begin();

  bool modified=false;

  while(true)
  {
    if(it1->first<it2->first)
    {
      // element of m1 is not in m2

      it1++;
      modified=true;
      if(it1==m1.end())
        break;
    }
    else if(it2->first<it1->first)
    {
      // element of m2 is not in m1

      it2++;
      if(it2==m2.end())
      {
        modified=true; // as there is a remaining element in m1
        break;
      }
    }
    else
    {
      // merge entries

      const abstract_object_pointert &v1=it1->second;
      const abstract_object_pointert &v2=it2->second;

      bool changes=false;
      abstract_object_pointert v_new;

      v_new=abstract_objectt::merge(v1, v2, changes);

      modified|=changes;

      out_map[it1->first]=v_new;

      it1++;

      if(it1==m1.end())
        break;

      it2++;

      if(it2==m2.end())
      {
        modified=true; // as there is a remaining element in m1
        break;
      }
    }
  }

  return modified;
}


#endif // CPROVER_ANALYSES_VARIABLE_SENSITIVITY_ABSTRACT_OBJECT_H
