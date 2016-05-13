INLINE Real square(Real x) { return x * x; }
INLINE Real sign(Real x) { return (x < 0) ? -1.0 : 1.0; }
INLINE bool are_close(Real a, Real b, Real tol, Real floor) {
  Real am = fabs(a);
  Real bm = fabs(b);
  if (am < floor && bm < floor)
    return true;
  if (fabs(b - a) / max2(am, bm) <= tol)
    return true;
  return false;
}

template <U8 n>
class Vector : public Few<Real, n> {
};

template <U8 n>
INLINE bool are_close(Vector<n> a, Vector<n> b) {
  for (U8 i = 0; i < n; ++i)
    if (!are_close(a[i], b[i]))
      return false;
  return true;
}

/* column-first storage and indexing !!! */
template <U8 m, U8 n>
class Matrix : public Few<Vector<m>, n> {
  public:
    Matrix() {}
    Matrix(std::initializer_list<Real> l) {
      U8 k = 0;
      for (Real v : l) {
        (*this)[k % n][k / n] = v;
        ++k;
      }
    }
};

template <U8 m, U8 n>
INLINE bool are_close(Matrix<m,n> a, Matrix<m,n> b) {
  for (U8 j = 0; j < n; ++j)
    if (!are_close(a[j], b[j]))
      return false;
  return true;
}

template <U8 m, U8 n>
std::ostream& operator<<(std::ostream& o, Matrix<m,n> a)
{
  for (U8 i = 0; i < m; ++i) {
    for (U8 j = 0; j < n; ++j)
      o << ' ' << a[j][i];
    o << '\n';
  }
  return o;
}
