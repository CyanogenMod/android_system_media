#ifndef FILTERFW_CORE_VEC_TYPES_
#define FILTERFW_CORE_VEC_TYPES_

#include "basictypes.h"

namespace android {
namespace mff {

template < class T, int dim>
class VecBase {
 public:
  T data[dim];
  VecBase() {}
  VecBase<T,dim>& operator = (const VecBase<T, dim> &x) {
    memcpy(data, x.data, sizeof(T)*dim);
    return *this;
  }
  T & operator [] (int i) {
    // out of boundary not checked
    return data[i];
  }
  const T & operator [] (int i) const {
    // out of boundary not checked
    return data[i];
  }
  T Length() {
    double sum = 0;
    for (int i = 0; i < dim; ++i)
      sum += static_cast<double> (data[i] * data[i]);
    return static_cast<T>(sqrt(sum));
  }
};

template < class T, int dim>
class Vec : public VecBase<T,dim> {
 public:
  Vec() {}
  Vec<T,dim>& operator = (const Vec<T, dim> &x) {
    memcpy(this->data, x.data, sizeof(T)*dim);
    return *this;
  }
};

template <class T, int dim>
Vec<T, dim> operator + (const Vec<T,dim> &x, const Vec<T,dim> &y) {
  Vec<T, dim> out;
  for (int i = 0; i < dim; i++)
    out.data[i] = x.data[i] + y.data[i];
  return out;
}

template <class T, int dim>
Vec<T, dim> operator - (const Vec<T,dim> &x, const Vec<T,dim> &y) {
  Vec<T, dim> out;
  for (int i = 0; i < dim; i++)
    out.data[i] = x.data[i] - y.data[i];
  return out;
}

template <class T, int dim>
Vec<T, dim> operator * (const Vec<T,dim> &x, const Vec<T,dim> &y) {
  Vec<T, dim> out;
  for (int i = 0; i < dim; i++)
    out.data[i] = x.data[i] * y.data[i];
  return out;
}

template <class T, int dim>
Vec<T, dim> operator / (const Vec<T,dim> &x, const Vec<T,dim> &y) {
  Vec<T, dim> out;
  for (int i = 0; i < dim; i++)
    out.data[i] = x.data[i] / y.data[i];
  return out;
}

template <class T, int dim>
T dot(const Vec<T,dim> &x, const Vec<T,dim> &y) {
  T out = 0;
  for (int i = 0; i < dim; i++)
    out += x.data[i] * y.data[i];
  return out;
}

template <class T, int dim>
Vec<T, dim> operator * (const Vec<T,dim> &x, T scale) {
  Vec<T, dim> out;
  for (int i = 0; i < dim; i++)
    out.data[i] = x.data[i] * scale;
  return out;
}

template <class T, int dim>
Vec<T, dim> operator / (const Vec<T,dim> &x, T scale) {
  Vec<T, dim> out;
  for (int i = 0; i < dim; i++)
    out.data[i] = x.data[i] / scale;
  return out;
}

template <class T, int dim>
Vec<T, dim> operator + (const Vec<T,dim> &x, T val) {
  Vec<T, dim> out;
  for (int i = 0; i < dim; i++)
    out.data[i] = x.data[i] + val;
  return out;
}

// specialization for vec2, vec3, vec4 float
template<>
class Vec<float, 2> : public VecBase<float, 2> {
public:
  Vec() {}
  Vec(float x, float y) {
    data[0] = x;
    data[1] = y;
  }
  Vec<float, 2>& operator = (const Vec<float, 2> &x) {
    memcpy(data, x.data, sizeof(float)*2);
    return *this;
  }
};

template<>
class Vec<float, 3> {
public:
  float data[3];
  Vec() {}
  Vec(float x, float y, float z) {
    data[0] = x;
    data[1] = y;
    data[2] = z;
  }
  Vec<float, 3>& operator = (const Vec<float, 3> &x) {
    memcpy(data, x.data, sizeof(float)*3);
    return *this;
  }
};

template<>
class Vec<float, 4> {
public:
  float data[4];
  Vec() {}
  Vec(float x, float y, float z, float w) {
    data[0] = x;
    data[1] = y;
    data[2] = z;
    data[3] = w;
  }
  Vec<float, 4>& operator = (const Vec<float, 4> &x) {
    memcpy(data, x.data, sizeof(float)*4);
    return *this;
  }
};

typedef Vec<float,2> Vec2f;
typedef Vec<float,3> Vec3f;
typedef Vec<float,4> Vec4f;

} // namespace filterfw
} // namespace android

#endif
