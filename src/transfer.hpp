#ifndef TRANSFER_HPP
#define TRANSFER_HPP

#include "internal.hpp"

namespace Omega_h {

void transfer_refine(Mesh* old_mesh, Mesh* new_mesh, LOs keys2edges,
    LOs keys2midverts, Int prod_dim, LOs keys2prods, LOs prods2new_ents,
    LOs same_ents2old_ents, LOs same_ents2new_ents);

void transfer_coarsen(Mesh* old_mesh, Mesh* new_mesh, LOs keys2verts,
    Adj keys2doms, Int prod_dim, LOs prods2new_ents, LOs same_ents2old_ents,
    LOs same_ents2new_ents);

void transfer_swap(Mesh* old_mesh, Mesh* new_mesh, Int prod_dim, LOs keys2edges,
    LOs keys2prods, LOs prods2new_ents, LOs same_ents2old_ents,
    LOs same_ents2new_ents);

void transfer_copy(Mesh* old_mesh, Mesh* new_mesh, Int prod_dim);

template <typename T>
void transfer_common(Mesh* old_mesh, Mesh* new_mesh, Int ent_dim,
    LOs same_ents2old_ents, LOs same_ents2new_ents, LOs prods2new_ents,
    TagBase const* tagbase, Read<T> prod_data);
template <typename T>
void transfer_inherit_refine(Mesh* old_mesh, Mesh* new_mesh, LOs keys2edges,
    Int prod_dim, LOs keys2prods, LOs prods2new_ents, LOs same_ents2old_ents,
    LOs same_ents2new_ents, std::string const& name);

#define INST_DECL(T)                                                           \
  extern template void transfer_common(Mesh* old_mesh, Mesh* new_mesh,         \
      Int ent_dim, LOs same_ents2old_ents, LOs same_ents2new_ents,             \
      LOs prods2new_ents, TagBase const* tagbase, Read<T> prod_data);          \
  extern template void transfer_inherit_refine<T>(Mesh * old_mesh,             \
      Mesh * new_mesh, LOs keys2edges, Int prod_dim, LOs keys2prods,           \
      LOs prods2new_ents, LOs same_ents2old_ents, LOs same_ents2new_ents,      \
      std::string const& name);
INST_DECL(I8)
INST_DECL(I32)
INST_DECL(I64)
INST_DECL(Real)
#undef INST_DECL

}  // end namespace Omega_h

#endif
