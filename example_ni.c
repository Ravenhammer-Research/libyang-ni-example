/*
 * example_ni.c
 * Simple libyang example demonstrating ietf-network-instance with
 * a schema-mount of ietf-routing under the `vrf-root` mount point.
 *
 */

#include "example_ni.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct ly_ctx *g_ctx = NULL;

/* ext-data to provide for schema-mount: a minimal yang-library + schema-mounts
 * describing mounting of ietf-routing at the mount-point label "vrf-root"
 * defined in module ietf-network-instance.
 */
/* (yang-library node omitted) */
LY_ERR build_yang_library(struct ly_ctx *ctx, struct lyd_node **yl) {
  const struct lys_module *mod_yl;
  struct lyd_node *module_set = NULL, *module = NULL;

  if (!ctx || !yl) {
    return LY_EINVAL;
  }

  mod_yl = ly_ctx_get_module_implemented(ctx, "ietf-yang-library");
  if (!mod_yl) {
    return LY_EINVAL;
  }

  if (lyd_new_inner(NULL, mod_yl, "yang-library", 0, yl) != LY_SUCCESS) {
    return LY_EINVAL;
  }
  if (lyd_new_list(*yl, mod_yl, "module-set", 0, &module_set, "set1") !=
      LY_SUCCESS) {
    lyd_free_all(*yl);
    *yl = NULL;
    return LY_EINVAL;
  }
  if (lyd_new_list(module_set, mod_yl, "module", 0, &module, "ietf-routing") !=
      LY_SUCCESS) {
    lyd_free_all(*yl);
    *yl = NULL;
    return LY_EINVAL;
  }
  if (lyd_new_term(module, mod_yl, "namespace",
                   "urn:ietf:params:xml:ns:yang:ietf-routing", 0,
                   NULL) != LY_SUCCESS) {
    lyd_free_all(*yl);
    *yl = NULL;
    return LY_EINVAL;
  }
  if (lyd_new_term(*yl, mod_yl, "content-id", "1", 0, NULL) != LY_SUCCESS) {
    lyd_free_all(*yl);
    *yl = NULL;
    return LY_EINVAL;
  }

  return LY_SUCCESS;
}

/* Build minimal schema-mounts ext-data using libyang APIs */
LY_ERR build_schema_mounts(struct lyd_node **sm_root) {
  const struct lys_module *mod_sm;
  struct lyd_node *mp = NULL;

  if (!g_ctx || !sm_root) {
    return LY_EINVAL;
  }

  mod_sm = ly_ctx_get_module_implemented(g_ctx, "ietf-yang-schema-mount");
  if (!mod_sm) {
    return LY_EINVAL;
  }

  if (lyd_new_inner(NULL, mod_sm, "schema-mounts", 0, sm_root) != LY_SUCCESS) {
    return LY_EINVAL;
  }

  if (lyd_new_list(*sm_root, NULL, "mount-point", 0, &mp,
                   "ietf-network-instance", "vrf-root") != LY_SUCCESS) {
    lyd_free_all(*sm_root);
    *sm_root = NULL;
    return LY_EINVAL;
  }
  if (lyd_new_inner(mp, NULL, "inline", 0, NULL) != LY_SUCCESS) {
    lyd_free_all(*sm_root);
    *sm_root = NULL;
    return LY_EINVAL;
  }

  return LY_SUCCESS;
}

/*
 * Callback invoked by libyang to get extension data for a mount-point instance.
 * Timing and reentrancy notes:
 * - The callback is called by libyang whenever it encounters a schema-mount
 *   extension instance while parsing or validating data. In this example it
 *   can be invoked during the programmatic construction of the tree in
 *   `main()` (for example while creating `vrf-root` / `routing`).
 * - The callback may be called reentrantly (libyang may parse/validate
 *   additional data while the callback runs). Therefore the callback must
 *   allocate and return a fresh ext-data tree (do not return pointers into
 *   global/shared trees) and must avoid modifying global parser/context state
 *   in a non-thread-safe way.
 * - Return `*ext_data_free = 1` so libyang will free the returned tree when
 *   appropriate. On error, free any partially-built data before returning.
 */
LY_ERR ext_data_clb(const struct lysc_ext_instance *ext,
                    const struct lyd_node *parent, void *user_data,
                    void **ext_data, ly_bool *ext_data_free) {
  struct lyd_node *yl = NULL, *sm_root = NULL, *first = NULL;
  LY_ERR rc = LY_SUCCESS;

  (void)ext;
  (void)parent;
  (void)user_data;

  if (!g_ctx) {
    return LY_EINVAL;
  }

  /* provide minimal yang-library + schema-mounts as siblings (yang-library
   * required by plugin) */
  CHECK_RET(build_yang_library(g_ctx, &yl));
  CHECK_RET(build_schema_mounts(&sm_root));

  first = yl;
  CHECK_RET(lyd_insert_sibling(yl, sm_root, &first));

  CHECK_RET(lyd_validate_module(
      &first, ly_ctx_get_module_implemented(g_ctx, "ietf-yang-schema-mount"), 0,
      NULL));

  *ext_data = first;
  *ext_data_free = 1;
  return LY_SUCCESS;

cleanup:
  lyd_free_all(yl);
  lyd_free_all(sm_root);
  return rc;
}

/* creation helpers */
LY_ERR create_network_instances(const struct lys_module *mod_ni,
                                struct lyd_node **root) {
  return lyd_new_inner(NULL, mod_ni, "network-instances", 0, root);
}

LY_ERR create_network_instance(struct lyd_node *root, const char *name,
                               struct lyd_node **ni) {
  return lyd_new_list(root, NULL, "network-instance", 0, ni, name);
}

LY_ERR create_vrf_root(struct lyd_node *ni, struct lyd_node **vrf) {
  return lyd_new_inner(ni, NULL, "vrf-root", 0, vrf);
}

LY_ERR create_routing(struct lyd_node *vrf, const struct lys_module *mod_rt,
                      struct lyd_node **routing) {
  return lyd_new_inner(vrf, mod_rt, "routing", 0, routing);
}

LY_ERR create_ribs(struct lyd_node *routing, struct lyd_node **ribs) {
  return lyd_new_inner(routing, NULL, "ribs", 0, ribs);
}

LY_ERR create_rib(struct lyd_node *ribs, const char *name,
                  struct lyd_node **rib) {
  return lyd_new_list(ribs, NULL, "rib", 0, rib, name);
}

LY_ERR create_address_family(struct lyd_node *rib, const char *val) {
  return lyd_new_term(rib, NULL, "address-family", val, 0, NULL);
}

int main(void) {
  struct ly_ctx *ctx = NULL;
  struct lyd_node *tree = NULL;
  char *out = NULL;
  LY_ERR rc;

  /* Create context that will search the RFC directory for YANG modules */
  if (ly_ctx_new("/home/sq/test/RFC", 0, &ctx) != LY_SUCCESS) {
    fprintf(stderr, "Failed to create libyang context\n");
    return 1;
  }
  g_ctx = ctx;

  /* load modules we will use (this also ensures imports are available) */
  ly_ctx_load_module(ctx, "ietf-yang-schema-mount", NULL, NULL);
  ly_ctx_load_module(ctx, "ietf-yang-library", NULL, NULL);
  ly_ctx_load_module(ctx, "ietf-network-instance", NULL, NULL);
  ly_ctx_load_module(ctx, "ietf-routing", NULL, NULL);

  /*
   * Register ext-data callback. Note: the callback may be invoked while
   * the application is itself building the data tree (see comments above),
   * so the callback implementation must be reentrancy-safe and must not
   * rely on mutable global state without proper synchronization.
   */
  ly_ctx_set_ext_data_clb(ctx, ext_data_clb, NULL);

  /* Build the same data tree programmatically using libyang APIs */
  const struct lys_module *mod_ni =
      ly_ctx_get_module_implemented(ctx, "ietf-network-instance");
  const struct lys_module *mod_rt =
      ly_ctx_get_module_implemented(ctx, "ietf-routing");
  if (!mod_ni || !mod_rt) {
    fprintf(stderr, "Required modules not loaded\n");
    ly_ctx_destroy(ctx);
    return 1;
  }

  struct lyd_node *root = NULL, *ni = NULL, *vrf = NULL, *routing = NULL,
                  *ribs = NULL, *rib = NULL;

  CHECK_RET(create_network_instances(mod_ni, &root));
  CHECK_RET(create_network_instance(root, "VRF1", &ni));
  CHECK_RET(create_vrf_root(ni, &vrf));
  CHECK_RET(create_routing(vrf, mod_rt, &routing));
  CHECK_RET(create_ribs(routing, &ribs));
  CHECK_RET(create_rib(ribs, "default", &rib));
  CHECK_RET(create_address_family(rib, "ietf-routing:ipv4"));

  tree = root;

cleanup:
  if (rc != LY_SUCCESS) {
    fprintf(stderr, "Failed to build data tree: %s\n", ly_errmsg(ctx));
    lyd_free_all(root);
    ly_ctx_destroy(ctx);
    return 1;
  }

  /* Print parsed tree */
  if (lyd_print_mem(&out, tree, LYD_XML, LYD_PRINT_SIBLINGS) == LY_SUCCESS) {
    printf("Parsed data:\n%s\n", out);
    free(out);
  }

  lyd_free_all(tree);
  ly_ctx_destroy(ctx);
  return 0;
}
