#ifndef ALGEBRA_HPP
#define ALGEBRA_HPP

#include <cmath>

#include "Omega_h_math.hpp"
#include "internal.hpp"

namespace Omega_h {

INLINE Real average(Real a, Real b) { return (a + b) / 2.; }

INLINE Real cube(Real x) { return x * x * x; }

INLINE Real sign(Real x) { return (x < 0.0) ? -1.0 : 1.0; }

INLINE Real raise(Real x, Int p) {
  Real out = 1.;
  for (Int i = 0; i < p; ++i) out *= x;
  return out;
}

INLINE Real rel_diff_with_floor(Real a, Real b, Real floor = EPSILON) {
  Real am = fabs(a);
  Real bm = fabs(b);
  if (am <= floor && bm <= floor) return 0.0;
  return fabs(b - a) / max2(am, bm);
}

INLINE bool are_close(
    Real a, Real b, Real tol = EPSILON, Real floor = EPSILON) {
  return rel_diff_with_floor(a, b, floor) <= tol;
}

template <Int n>
INLINE Real average(Few<Real, n> x) {
  Real avg = 0;
  for (Int i = 0; i < n; ++i) avg += x[i];
  avg /= n;
  return avg;
}

template <Int n>
INLINE Vector<n> operator-(Vector<n> a) {
  Vector<n> c;
  for (Int i = 0; i < n; ++i) c[i] = -a[i];
  return c;
}

template <Int n>
INLINE bool are_close(
    Vector<n> a, Vector<n> b, Real tol = EPSILON, Real floor = EPSILON) {
  for (Int i = 0; i < n; ++i)
    if (!are_close(a[i], b[i], tol, floor)) return false;
  return true;
}

template <Int n>
INLINE Vector<n> zero_vector() {
  Vector<n> v;
  for (Int i = 0; i < n; ++i) v[i] = 0.0;
  return v;
}

template <Int m, Int n>
INLINE Vector<m> average(Few<Vector<m>, n> x) {
  Vector<m> avg = zero_vector<m>();
  for (Int i = 0; i < n; ++i) avg = avg + x[i];
  avg = avg / n;
  return avg;
}

template <Int m, Int n>
inline Matrix<m, n>::Matrix(std::initializer_list<Real> l) {
  Int k = 0;
  for (Real v : l) {
    (*this)[k % n][k / n] = v;
    ++k;
  }
}

template <Int m, Int n>
INLINE Matrix<m, n> operator*(Matrix<m, n> a, Real b) {
  Matrix<m, n> c;
  for (Int j = 0; j < n; ++j) c[j] = a[j] * b;
  return c;
}

template <Int m, Int n>
INLINE Matrix<m, n> operator*(Real a, Matrix<m, n> b) {
  return b * a;
}

template <Int m, Int n>
INLINE Matrix<m, n> operator/(Matrix<m, n> a, Real b) {
  Matrix<m, n> c;
  for (Int j = 0; j < n; ++j) c[j] = a[j] / b;
  return c;
}

template <Int m, Int n>
INLINE Matrix<m, n> operator+(Matrix<m, n> a, Matrix<m, n> b) {
  Matrix<m, n> c;
  for (Int j = 0; j < n; ++j) c[j] = a[j] + b[j];
  return c;
}

template <Int m, Int n>
INLINE Matrix<m, n> operator-(Matrix<m, n> a, Matrix<m, n> b) {
  Matrix<m, n> c;
  for (Int j = 0; j < n; ++j) c[j] = a[j] - b[j];
  return c;
}

template <Int m, Int n>
INLINE Real max_norm(Matrix<m, n> a) {
  Real x = 0.0;
  for (Int j = 0; j < n; ++j)
    for (Int i = 0; i < m; ++i) x = max2(x, fabs(a[j][i]));
  return x;
}

template <Int max_m, Int max_n>
INLINE Real frobenius_norm(Int m, Int n, Matrix<max_m, max_n> a) {
  Real x = 0.0;
  for (Int j = 0; j < n; ++j)
    for (Int i = 0; i < m; ++i) x += square(a[j][i]);
  return sqrt(x);
}

template <Int m, Int n>
INLINE bool are_close(
    Matrix<m, n> a, Matrix<m, n> b, Real tol = EPSILON, Real floor = EPSILON) {
  for (Int j = 0; j < n; ++j)
    if (!are_close(a[j], b[j], tol, floor)) return false;
  return true;
}

template <Int m, Int n>
INLINE Matrix<m, n> tensor_product(Vector<m> a, Vector<n> b) {
  Matrix<m, n> c;
  for (Int j = 0; j < n; ++j)
    for (Int i = 0; i < m; ++i) c[j][i] = a[i] * b[j];
  return c;
}

template <Int m>
INLINE Real trace(Matrix<m, m> a) {
  Real t = a[0][0];
  for (Int i = 1; i < m; ++i) t += a[i][i];
  return t;
}

template <Int m>
INLINE Vector<m> diagonal(Matrix<m, m> a) {
  Vector<m> v;
  for (Int i = 0; i < m; ++i) v[i] = a[i][i];
  return v;
}

template <Int m, Int n>
INLINE Matrix<m, n> zero_matrix() {
  Matrix<m, n> a;
  for (Int j = 0; j < n; ++j) a[j] = zero_vector<m>();
  return a;
}

template <Int m>
INLINE void subtract_from_diag(Matrix<m, m>& a, Real mu) {
  for (Int i = 0; i < m; ++i) a[i][i] -= mu;
}

/* a function to disambiguate a unit vector
   from its negative. we treat the signs of
   the components as bits of an integer,
   and negate the components if the resulting
   bit pattern makes a larger integer */
template <Int n>
INLINE Vector<n> positivize(Vector<n> v) {
  std::uint32_t bits = 0;
  for (Int i = 0; i < n; ++i) bits |= (std::uint32_t(v[i] >= 0.0) << i);
  std::uint32_t neg_bits = (~bits) & ((std::uint32_t(1) << n) - 1);
  if (neg_bits > bits) return v * -1.0;
  return v;
}

Reals normalize_vectors(Reals vs, Int dim);

}  // end namespace Omega_h

#endif
