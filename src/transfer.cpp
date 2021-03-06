#include "transfer.hpp"

#include "access.hpp"
#include "fit.hpp"
#include "loop.hpp"
#include "map.hpp"
#include "metric.hpp"
#include "quality.hpp"
#include "size.hpp"
#include "tag.hpp"
#include "transfer_conserve.hpp"

namespace Omega_h {

template <typename T>
void transfer_common(Mesh* old_mesh, Mesh* new_mesh, Int ent_dim,
    LOs same_ents2old_ents, LOs same_ents2new_ents, LOs prods2new_ents,
    TagBase const* tagbase, Read<T> prod_data) {
  auto const& name = tagbase->name();
  auto ncomps = tagbase->ncomps();
  auto xfer = tagbase->xfer();
  auto outflags = tagbase->outflags();
  auto old_data = old_mesh->get_array<T>(ent_dim, name);
  auto same_data = unmap(same_ents2old_ents, old_data, ncomps);
  auto nnew_ents = new_mesh->nents(ent_dim);
  auto new_data = Write<T>(nnew_ents * ncomps);
  map_into(same_data, same_ents2new_ents, new_data, ncomps);
  map_into(prod_data, prods2new_ents, new_data, ncomps);
  new_mesh->add_tag(ent_dim, name, ncomps, xfer, outflags, Read<T>(new_data));
}

static void transfer_linear_interp(Mesh* old_mesh, Mesh* new_mesh,
    LOs keys2edges, LOs keys2midverts, LOs same_verts2old_verts,
    LOs same_verts2new_verts) {
  for (Int i = 0; i < old_mesh->ntags(VERT); ++i) {
    auto tagbase = old_mesh->get_tag(VERT, i);
    if (tagbase->xfer() == OMEGA_H_LINEAR_INTERP) {
      auto ncomps = tagbase->ncomps();
      auto old_data = old_mesh->get_array<Real>(VERT, tagbase->name());
      auto prod_data =
          average_field(old_mesh, EDGE, keys2edges, ncomps, old_data);
      transfer_common(old_mesh, new_mesh, VERT, same_verts2old_verts,
          same_verts2new_verts, keys2midverts, tagbase, prod_data);
    }
  }
}

static void transfer_metric(Mesh* old_mesh, Mesh* new_mesh, LOs keys2edges,
    LOs keys2midverts, LOs same_verts2old_verts, LOs same_verts2new_verts) {
  for (Int i = 0; i < old_mesh->ntags(VERT); ++i) {
    auto tagbase = old_mesh->get_tag(VERT, i);
    if (tagbase->xfer() == OMEGA_H_METRIC) {
      auto old_data = old_mesh->get_array<Real>(VERT, tagbase->name());
      auto prod_data = average_metric(old_mesh, EDGE, keys2edges, old_data);
      transfer_common(old_mesh, new_mesh, VERT, same_verts2old_verts,
          same_verts2new_verts, keys2midverts, tagbase, prod_data);
    }
  }
}

template <typename T>
void transfer_inherit_refine(Mesh* old_mesh, Mesh* new_mesh, LOs keys2edges,
    Int prod_dim, LOs keys2prods, LOs prods2new_ents, LOs same_ents2old_ents,
    LOs same_ents2new_ents, std::string const& name) {
  auto old_tag = old_mesh->get_tag<T>(prod_dim, name);
  auto ncomps = old_tag->ncomps();
  auto nprods = keys2prods.last();
  auto prod_data = Write<T>(nprods * ncomps);
  auto nkeys = keys2edges.size();
  /* transfer pairs */
  if (prod_dim > VERT) {
    auto dom_dim = prod_dim;
    auto dom_data = old_mesh->get_array<T>(dom_dim, name);
    auto edges2doms = old_mesh->ask_graph(EDGE, dom_dim);
    auto edges2edge_doms = edges2doms.a2ab;
    auto edge_doms2doms = edges2doms.ab2b;
    auto f = LAMBDA(LO key) {
      auto edge = keys2edges[key];
      auto prod = keys2prods[key];
      for (auto edge_dom = edges2edge_doms[edge];
           edge_dom < edges2edge_doms[edge + 1]; ++edge_dom) {
        auto dom = edge_doms2doms[edge_dom];
        for (Int pair = 0; pair < 2; ++pair) {
          for (Int comp = 0; comp < ncomps; ++comp) {
            prod_data[prod * ncomps + comp] = dom_data[dom * ncomps + comp];
          }
          ++prod;
        }
      }
    };
    parallel_for(nkeys, f);
  }
  if (prod_dim < old_mesh->dim()) {
    auto dom_dim = prod_dim + 1;
    auto dom_data = old_mesh->get_array<T>(dom_dim, name);
    auto edges2doms = old_mesh->ask_graph(EDGE, dom_dim);
    auto edges2edge_doms = edges2doms.a2ab;
    auto edge_doms2doms = edges2doms.ab2b;
    auto f = LAMBDA(LO key) {
      auto edge = keys2edges[key];
      auto ndoms = edges2edge_doms[edge + 1] - edges2edge_doms[edge];
      auto prod = keys2prods[key + 1] - ndoms;
      for (auto edge_dom = edges2edge_doms[edge];
           edge_dom < edges2edge_doms[edge + 1]; ++edge_dom) {
        auto dom = edge_doms2doms[edge_dom];
        for (Int comp = 0; comp < ncomps; ++comp) {
          prod_data[prod * ncomps + comp] = dom_data[dom * ncomps + comp];
        }
        ++prod;
      }
    };
    parallel_for(nkeys, f);
  }
  transfer_common(old_mesh, new_mesh, prod_dim, same_ents2old_ents,
      same_ents2new_ents, prods2new_ents, old_tag, Read<T>(prod_data));
}

static void transfer_inherit_refine(Mesh* old_mesh, Mesh* new_mesh,
    LOs keys2edges, Int prod_dim, LOs keys2prods, LOs prods2new_ents,
    LOs same_ents2old_ents, LOs same_ents2new_ents) {
  for (Int i = 0; i < old_mesh->ntags(prod_dim); ++i) {
    auto tagbase = old_mesh->get_tag(prod_dim, i);
    if (tagbase->xfer() == OMEGA_H_INHERIT) {
      switch (tagbase->type()) {
        case OMEGA_H_I8:
          transfer_inherit_refine<I8>(old_mesh, new_mesh, keys2edges, prod_dim,
              keys2prods, prods2new_ents, same_ents2old_ents,
              same_ents2new_ents, tagbase->name());
          break;
        case OMEGA_H_I32:
          transfer_inherit_refine<I32>(old_mesh, new_mesh, keys2edges, prod_dim,
              keys2prods, prods2new_ents, same_ents2old_ents,
              same_ents2new_ents, tagbase->name());
          break;
        case OMEGA_H_I64:
          transfer_inherit_refine<I64>(old_mesh, new_mesh, keys2edges, prod_dim,
              keys2prods, prods2new_ents, same_ents2old_ents,
              same_ents2new_ents, tagbase->name());
          break;
        case OMEGA_H_F64:
          transfer_inherit_refine<Real>(old_mesh, new_mesh, keys2edges,
              prod_dim, keys2prods, prods2new_ents, same_ents2old_ents,
              same_ents2new_ents, tagbase->name());
          break;
      }
    }
  }
}

static void transfer_pointwise_refine(Mesh* old_mesh, Mesh* new_mesh,
    LOs keys2edges, LOs keys2prods, LOs prods2new_ents, LOs same_ents2old_ents,
    LOs same_ents2new_ents) {
  auto dim = old_mesh->dim();
  for (Int i = 0; i < old_mesh->ntags(dim); ++i) {
    auto tagbase = old_mesh->get_tag(dim, i);
    if (tagbase->xfer() == OMEGA_H_POINTWISE) {
      transfer_inherit_refine<Real>(old_mesh, new_mesh, keys2edges, dim,
          keys2prods, prods2new_ents, same_ents2old_ents, same_ents2new_ents,
          tagbase->name());
    }
  }
}

static void transfer_length(Mesh* old_mesh, Mesh* new_mesh,
    LOs same_ents2old_ents, LOs same_ents2new_ents, LOs prods2new_ents) {
  for (Int i = 0; i < old_mesh->ntags(EDGE); ++i) {
    auto tagbase = old_mesh->get_tag(EDGE, i);
    if (tagbase->xfer() == OMEGA_H_LENGTH) {
      auto prod_data = measure_edges_metric(new_mesh, prods2new_ents);
      transfer_common(old_mesh, new_mesh, EDGE, same_ents2old_ents,
          same_ents2new_ents, prods2new_ents, tagbase, prod_data);
    }
  }
}

static void transfer_quality(Mesh* old_mesh, Mesh* new_mesh,
    LOs same_ents2old_ents, LOs same_ents2new_ents, LOs prods2new_ents) {
  auto dim = old_mesh->dim();
  for (Int i = 0; i < old_mesh->ntags(dim); ++i) {
    auto tagbase = old_mesh->get_tag(dim, i);
    if (tagbase->xfer() == OMEGA_H_QUALITY) {
      auto prod_data = measure_qualities(new_mesh, prods2new_ents);
      transfer_common(old_mesh, new_mesh, dim, same_ents2old_ents,
          same_ents2new_ents, prods2new_ents, tagbase, prod_data);
    }
  }
}

void transfer_refine(Mesh* old_mesh, Mesh* new_mesh, LOs keys2edges,
    LOs keys2midverts, Int prod_dim, LOs keys2prods, LOs prods2new_ents,
    LOs same_ents2old_ents, LOs same_ents2new_ents) {
  transfer_inherit_refine(old_mesh, new_mesh, keys2edges, prod_dim, keys2prods,
      prods2new_ents, same_ents2old_ents, same_ents2new_ents);
  if (prod_dim == VERT) {
    transfer_linear_interp(old_mesh, new_mesh, keys2edges, keys2midverts,
        same_ents2old_ents, same_ents2new_ents);
    transfer_metric(old_mesh, new_mesh, keys2edges, keys2midverts,
        same_ents2old_ents, same_ents2new_ents);
  } else if (prod_dim == EDGE) {
    transfer_length(old_mesh, new_mesh, same_ents2old_ents, same_ents2new_ents,
        prods2new_ents);
  } else if (prod_dim == old_mesh->dim()) {
    transfer_quality(old_mesh, new_mesh, same_ents2old_ents, same_ents2new_ents,
        prods2new_ents);
    transfer_conserve_refine(old_mesh, new_mesh, keys2edges, keys2prods,
        prods2new_ents, same_ents2old_ents, same_ents2new_ents);
    transfer_pointwise_refine(old_mesh, new_mesh, keys2edges, keys2prods,
        prods2new_ents, same_ents2old_ents, same_ents2new_ents);
    transfer_conserve_r3d_refine(old_mesh, new_mesh, keys2edges, keys2prods,
        prods2new_ents, same_ents2old_ents, same_ents2new_ents);
  }
}

template <typename T>
static void transfer_inherit_coarsen_tmpl(Mesh* old_mesh, Mesh* new_mesh,
    Adj keys2doms, Int prod_dim, LOs prods2new_ents, LOs same_ents2old_ents,
    LOs same_ents2new_ents, TagBase const* tagbase) {
  auto name = tagbase->name();
  auto old_tag = to<T>(tagbase);
  auto ncomps = old_tag->ncomps();
  auto dom_data = old_tag->array();
  auto key_doms2doms = keys2doms.ab2b;
  auto prod_data = unmap(key_doms2doms, dom_data, ncomps);
  transfer_common(old_mesh, new_mesh, prod_dim, same_ents2old_ents,
      same_ents2new_ents, prods2new_ents, old_tag, prod_data);
}

static void transfer_inherit_coarsen(Mesh* old_mesh, Mesh* new_mesh,
    Adj keys2doms, Int prod_dim, LOs prods2new_ents, LOs same_ents2old_ents,
    LOs same_ents2new_ents) {
  for (Int i = 0; i < old_mesh->ntags(prod_dim); ++i) {
    auto tagbase = old_mesh->get_tag(prod_dim, i);
    if (tagbase->xfer() == OMEGA_H_INHERIT) {
      switch (tagbase->type()) {
        case OMEGA_H_I8:
          transfer_inherit_coarsen_tmpl<I8>(old_mesh, new_mesh, keys2doms,
              prod_dim, prods2new_ents, same_ents2old_ents, same_ents2new_ents,
              tagbase);
          break;
        case OMEGA_H_I32:
          transfer_inherit_coarsen_tmpl<I32>(old_mesh, new_mesh, keys2doms,
              prod_dim, prods2new_ents, same_ents2old_ents, same_ents2new_ents,
              tagbase);
          break;
        case OMEGA_H_I64:
          transfer_inherit_coarsen_tmpl<I64>(old_mesh, new_mesh, keys2doms,
              prod_dim, prods2new_ents, same_ents2old_ents, same_ents2new_ents,
              tagbase);
          break;
        case OMEGA_H_F64:
          transfer_inherit_coarsen_tmpl<Real>(old_mesh, new_mesh, keys2doms,
              prod_dim, prods2new_ents, same_ents2old_ents, same_ents2new_ents,
              tagbase);
          break;
      }
    }
  }
}

template <typename T>
static void transfer_no_products_tmpl(Mesh* old_mesh, Mesh* new_mesh,
    Int prod_dim, LOs same_ents2old_ents, LOs same_ents2new_ents,
    TagBase const* tagbase) {
  auto old_tag = to<T>(tagbase);
  auto prods2new_ents = LOs({});
  auto prod_data = Read<T>({});
  transfer_common(old_mesh, new_mesh, prod_dim, same_ents2old_ents,
      same_ents2new_ents, prods2new_ents, old_tag, prod_data);
}

static void transfer_no_products(Mesh* old_mesh, Mesh* new_mesh, Int prod_dim,
    LOs same_ents2old_ents, LOs same_ents2new_ents) {
  for (Int i = 0; i < old_mesh->ntags(prod_dim); ++i) {
    auto tagbase = old_mesh->get_tag(prod_dim, i);
    if ((tagbase->xfer() == OMEGA_H_INHERIT) ||
        (tagbase->xfer() == OMEGA_H_LINEAR_INTERP) ||
        (tagbase->xfer() == OMEGA_H_METRIC)) {
      switch (tagbase->type()) {
        case OMEGA_H_I8:
          transfer_no_products_tmpl<I8>(old_mesh, new_mesh, prod_dim,
              same_ents2old_ents, same_ents2new_ents, tagbase);
          break;
        case OMEGA_H_I32:
          transfer_no_products_tmpl<I32>(old_mesh, new_mesh, prod_dim,
              same_ents2old_ents, same_ents2new_ents, tagbase);
          break;
        case OMEGA_H_I64:
          transfer_no_products_tmpl<I64>(old_mesh, new_mesh, prod_dim,
              same_ents2old_ents, same_ents2new_ents, tagbase);
          break;
        case OMEGA_H_F64:
          transfer_no_products_tmpl<Real>(old_mesh, new_mesh, prod_dim,
              same_ents2old_ents, same_ents2new_ents, tagbase);
          break;
      }
    }
  }
}

DEVICE static void transfer_average_cavity(LO key, LOs const& keys2kds,
    LOs const& kds2kd_elems, LOs const& kd_elems2elems, LOs const& keys2prods,
    Int ncomps, Reals const& old_data, Write<Real> const& prod_data_w) {
  auto kd = keys2kds[key];
  for (Int comp = 0; comp < ncomps; ++comp) {
    Real sum = 0.0;
    Int n = 0;
    for (auto kd_elem = kds2kd_elems[kd]; kd_elem < kds2kd_elems[kd + 1];
         ++kd_elem) {
      auto old_elem = kd_elems2elems[kd_elem];
      sum += old_data[old_elem * ncomps + comp];
      ++n;
    }
    auto avg = sum / n;
    for (auto prod = keys2prods[key]; prod < keys2prods[key + 1]; ++prod) {
      prod_data_w[prod * ncomps + comp] = avg;
    }
  }
}

template <Int dim>
DEVICE static void transfer_fit_cavity(LO key, LOs const& keys2kds,
    LOs const& kds2kd_elems, LOs const& kd_elems2elems,
    LOs const& old_elem_verts2verts, Reals const& old_coords,
    LOs const& keys2prods, LOs const& prods2new_elems,
    LOs const& new_elem_verts2verts, Reals const& new_coords, Int ncomps,
    Reals const& old_data, Write<Real> const& prod_data_w) {
  auto kd = keys2kds[key];
  auto qr_decomp = get_cavity_qr_factorization<dim>(
      kd, kds2kd_elems, kd_elems2elems, old_elem_verts2verts, old_coords);
  for (Int comp = 0; comp < ncomps; ++comp) {
    auto coeffs = fit_cavity_polynomial<dim>(
        qr_decomp, kd, kds2kd_elems, kd_elems2elems, old_data, comp, ncomps);
    for (auto prod = keys2prods[key]; prod < keys2prods[key + 1]; ++prod) {
      auto new_elem = prods2new_elems[prod];
      auto v = gather_verts<dim + 1>(new_elem_verts2verts, new_elem);
      auto p = gather_vectors<dim + 1, dim>(new_coords, v);
      auto x = average(p);
      auto val = eval_polynomial<dim>(coeffs, x);
      prod_data_w[prod * ncomps + comp] = val;
    }
  }
}

template <Int dim>
static void transfer_pointwise_coarsen_tmpl(Mesh* old_mesh, Mesh* new_mesh,
    LOs keys2kds, LOs keys2prods, LOs prods2new_ents, LOs same_ents2old_ents,
    LOs same_ents2new_ents, TagBase const* tagbase) {
  auto name = tagbase->name();
  auto old_tag = to<Real>(tagbase);
  auto ncomps = old_tag->ncomps();
  auto old_data = old_tag->array();
  auto kds2elems = old_mesh->ask_up(VERT, dim);
  auto kds2kd_elems = kds2elems.a2ab;
  auto kd_elems2elems = kds2elems.ab2b;
  auto old_elem_verts2verts = old_mesh->ask_verts_of(dim);
  auto old_coords = old_mesh->coords();
  auto kd_class_dim = old_mesh->get_array<I8>(VERT, "class_dim");
  auto new_elem_verts2verts = new_mesh->ask_verts_of(dim);
  auto new_coords = new_mesh->coords();
  auto nkeys = keys2kds.size();
  auto nprods = keys2prods.last();
  auto prod_data_w = Write<Real>(nprods * ncomps);
  auto f = LAMBDA(LO key) {
    if (kd_class_dim[keys2kds[key]] == dim) {
      transfer_fit_cavity<dim>(key, keys2kds, kds2kd_elems, kd_elems2elems,
          old_elem_verts2verts, old_coords, keys2prods, prods2new_ents,
          new_elem_verts2verts, new_coords, ncomps, old_data, prod_data_w);
    } else {
      transfer_average_cavity(key, keys2kds, kds2kd_elems, kd_elems2elems,
          keys2prods, ncomps, old_data, prod_data_w);
    }
  };
  parallel_for(nkeys, f);
  auto prod_data = Reals(prod_data_w);
  transfer_common(old_mesh, new_mesh, dim, same_ents2old_ents,
      same_ents2new_ents, prods2new_ents, old_tag, prod_data);
}

static void transfer_pointwise_coarsen(Mesh* old_mesh, Mesh* new_mesh,
    LOs keys2kds, LOs keys2prods, LOs prods2new_ents, LOs same_ents2old_ents,
    LOs same_ents2new_ents) {
  auto dim = new_mesh->dim();
  for (Int i = 0; i < old_mesh->ntags(dim); ++i) {
    auto tagbase = old_mesh->get_tag(dim, i);
    if (tagbase->xfer() == OMEGA_H_POINTWISE) {
      if (dim == 3) {
        transfer_pointwise_coarsen_tmpl<3>(old_mesh, new_mesh, keys2kds,
            keys2prods, prods2new_ents, same_ents2old_ents, same_ents2new_ents,
            tagbase);
      } else if (dim == 2) {
        transfer_pointwise_coarsen_tmpl<2>(old_mesh, new_mesh, keys2kds,
            keys2prods, prods2new_ents, same_ents2old_ents, same_ents2new_ents,
            tagbase);
      }
    }
  }
}

void transfer_coarsen(Mesh* old_mesh, Mesh* new_mesh, LOs keys2verts,
    Adj keys2doms, Int prod_dim, LOs prods2new_ents, LOs same_ents2old_ents,
    LOs same_ents2new_ents) {
  if (prod_dim == VERT) {
    transfer_no_products(
        old_mesh, new_mesh, prod_dim, same_ents2old_ents, same_ents2new_ents);
  } else {
    transfer_inherit_coarsen(old_mesh, new_mesh, keys2doms, prod_dim,
        prods2new_ents, same_ents2old_ents, same_ents2new_ents);
  }
  if (prod_dim == EDGE) {
    transfer_length(old_mesh, new_mesh, same_ents2old_ents, same_ents2new_ents,
        prods2new_ents);
  }
  if (prod_dim == old_mesh->dim()) {
    transfer_quality(old_mesh, new_mesh, same_ents2old_ents, same_ents2new_ents,
        prods2new_ents);
    transfer_conserve(old_mesh, new_mesh, VERT, keys2verts, keys2doms.a2ab,
        prods2new_ents, same_ents2old_ents, same_ents2new_ents);
    transfer_pointwise_coarsen(old_mesh, new_mesh, keys2verts, keys2doms.a2ab,
        prods2new_ents, same_ents2old_ents, same_ents2new_ents);
    transfer_conserve_r3d(old_mesh, new_mesh, VERT, keys2verts, keys2doms.a2ab,
        prods2new_ents, same_ents2old_ents, same_ents2new_ents);
  }
}

template <typename T>
static void transfer_copy_tmpl(
    Mesh* new_mesh, Int prod_dim, TagBase const* tagbase) {
  auto old_tag = to<T>(tagbase);
  auto const& name = old_tag->name();
  auto ncomps = old_tag->ncomps();
  auto xfer = old_tag->xfer();
  auto outflags = old_tag->outflags();
  auto old_data = old_tag->array();
  new_mesh->add_tag(prod_dim, name, ncomps, xfer, outflags, old_data);
}

void transfer_copy(Mesh* old_mesh, Mesh* new_mesh, Int prod_dim) {
  for (Int i = 0; i < old_mesh->ntags(prod_dim); ++i) {
    auto tagbase = old_mesh->get_tag(prod_dim, i);
    if (tagbase->xfer() != OMEGA_H_DONT_TRANSFER) {
      switch (tagbase->type()) {
        case OMEGA_H_I8:
          transfer_copy_tmpl<I8>(new_mesh, prod_dim, tagbase);
          break;
        case OMEGA_H_I32:
          transfer_copy_tmpl<I32>(new_mesh, prod_dim, tagbase);
          break;
        case OMEGA_H_I64:
          transfer_copy_tmpl<I64>(new_mesh, prod_dim, tagbase);
          break;
        case OMEGA_H_F64:
          transfer_copy_tmpl<Real>(new_mesh, prod_dim, tagbase);
          break;
      }
    }
  }
}

template <typename T>
static void transfer_inherit_swap_tmpl(Mesh* old_mesh, Mesh* new_mesh,
    Int prod_dim, LOs keys2edges, LOs keys2prods, LOs prods2new_ents,
    LOs same_ents2old_ents, LOs same_ents2new_ents, TagBase const* tagbase) {
  auto const& name = tagbase->name();
  auto old_tag = old_mesh->get_tag<T>(EDGE, name);
  auto ncomps = old_tag->ncomps();
  auto edge_data = old_tag->array();
  auto key_data = unmap(keys2edges, edge_data, ncomps);
  auto prod_data = expand(key_data, keys2prods, ncomps);
  transfer_common(old_mesh, new_mesh, prod_dim, same_ents2old_ents,
      same_ents2new_ents, prods2new_ents, old_tag, prod_data);
}

static void transfer_inherit_swap(Mesh* old_mesh, Mesh* new_mesh, Int prod_dim,
    LOs keys2edges, LOs keys2prods, LOs prods2new_ents, LOs same_ents2old_ents,
    LOs same_ents2new_ents) {
  for (Int i = 0; i < old_mesh->ntags(prod_dim); ++i) {
    auto tagbase = old_mesh->get_tag(prod_dim, i);
    if (tagbase->xfer() == OMEGA_H_INHERIT) {
      switch (tagbase->type()) {
        case OMEGA_H_I8:
          transfer_inherit_swap_tmpl<I8>(old_mesh, new_mesh, prod_dim,
              keys2edges, keys2prods, prods2new_ents, same_ents2old_ents,
              same_ents2new_ents, tagbase);
          break;
        case OMEGA_H_I32:
          transfer_inherit_swap_tmpl<I32>(old_mesh, new_mesh, prod_dim,
              keys2edges, keys2prods, prods2new_ents, same_ents2old_ents,
              same_ents2new_ents, tagbase);
          break;
        case OMEGA_H_I64:
          transfer_inherit_swap_tmpl<I64>(old_mesh, new_mesh, prod_dim,
              keys2edges, keys2prods, prods2new_ents, same_ents2old_ents,
              same_ents2new_ents, tagbase);
          break;
        case OMEGA_H_F64:
          transfer_inherit_swap_tmpl<Real>(old_mesh, new_mesh, prod_dim,
              keys2edges, keys2prods, prods2new_ents, same_ents2old_ents,
              same_ents2new_ents, tagbase);
          break;
      }
    }
  }
}

template <Int dim>
static void transfer_pointwise_swap_tmpl(Mesh* old_mesh, Mesh* new_mesh,
    LOs keys2kds, LOs keys2prods, LOs prods2new_ents, LOs same_ents2old_ents,
    LOs same_ents2new_ents, TagBase const* tagbase) {
  auto name = tagbase->name();
  auto old_tag = to<Real>(tagbase);
  auto ncomps = old_tag->ncomps();
  auto old_data = old_tag->array();
  auto kds2elems = old_mesh->ask_up(EDGE, dim);
  auto kds2kd_elems = kds2elems.a2ab;
  auto kd_elems2elems = kds2elems.ab2b;
  auto nkeys = keys2kds.size();
  auto nprods = keys2prods.last();
  auto prod_data_w = Write<Real>(nprods * ncomps);
  auto f = LAMBDA(LO key) {
    transfer_average_cavity(key, keys2kds, kds2kd_elems, kd_elems2elems,
        keys2prods, ncomps, old_data, prod_data_w);
  };
  parallel_for(nkeys, f);
  auto prod_data = Reals(prod_data_w);
  transfer_common(old_mesh, new_mesh, dim, same_ents2old_ents,
      same_ents2new_ents, prods2new_ents, old_tag, prod_data);
}

static void transfer_pointwise_swap(Mesh* old_mesh, Mesh* new_mesh,
    LOs keys2kds, LOs keys2prods, LOs prods2new_ents, LOs same_ents2old_ents,
    LOs same_ents2new_ents) {
  auto dim = new_mesh->dim();
  for (Int i = 0; i < old_mesh->ntags(dim); ++i) {
    auto tagbase = old_mesh->get_tag(dim, i);
    if (tagbase->xfer() == OMEGA_H_POINTWISE) {
      if (dim == 3) {
        transfer_pointwise_swap_tmpl<3>(old_mesh, new_mesh, keys2kds,
            keys2prods, prods2new_ents, same_ents2old_ents, same_ents2new_ents,
            tagbase);
      } else if (dim == 2) {
        transfer_pointwise_swap_tmpl<2>(old_mesh, new_mesh, keys2kds,
            keys2prods, prods2new_ents, same_ents2old_ents, same_ents2new_ents,
            tagbase);
      }
    }
  }
}

void transfer_swap(Mesh* old_mesh, Mesh* new_mesh, Int prod_dim, LOs keys2edges,
    LOs keys2prods, LOs prods2new_ents, LOs same_ents2old_ents,
    LOs same_ents2new_ents) {
  if (prod_dim == VERT) {
    transfer_copy(old_mesh, new_mesh, prod_dim);
  } else {
    transfer_inherit_swap(old_mesh, new_mesh, prod_dim, keys2edges, keys2prods,
        prods2new_ents, same_ents2old_ents, same_ents2new_ents);
  }
  if (prod_dim == EDGE) {
    transfer_length(old_mesh, new_mesh, same_ents2old_ents, same_ents2new_ents,
        prods2new_ents);
  }
  if (prod_dim == old_mesh->dim()) {
    transfer_quality(old_mesh, new_mesh, same_ents2old_ents, same_ents2new_ents,
        prods2new_ents);
    transfer_conserve(old_mesh, new_mesh, EDGE, keys2edges, keys2prods,
        prods2new_ents, same_ents2old_ents, same_ents2new_ents);
    transfer_pointwise_swap(old_mesh, new_mesh, keys2edges, keys2prods,
        prods2new_ents, same_ents2old_ents, same_ents2new_ents);
    transfer_conserve_r3d(old_mesh, new_mesh, EDGE, keys2edges, keys2prods,
        prods2new_ents, same_ents2old_ents, same_ents2new_ents);
  }
}

#define INST(T)                                                                \
  template void transfer_common(Mesh* old_mesh, Mesh* new_mesh, Int ent_dim,   \
      LOs same_ents2old_ents, LOs same_ents2new_ents, LOs prods2new_ents,      \
      TagBase const* tagbase, Read<T> prod_data);                              \
  template void transfer_inherit_refine<T>(Mesh * old_mesh, Mesh * new_mesh,   \
      LOs keys2edges, Int prod_dim, LOs keys2prods, LOs prods2new_ents,        \
      LOs same_ents2old_ents, LOs same_ents2new_ents,                          \
      std::string const& name);
INST(I8)
INST(I32)
INST(I64)
INST(Real)
#undef INST

}  // end namespace Omega_h
