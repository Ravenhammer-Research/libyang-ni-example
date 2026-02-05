/* example_ni.h
 * Prototypes and helpers for example_ni.c
 */
#ifndef EXAMPLE_NI_H
#define EXAMPLE_NI_H

#include <libyang/libyang.h>

/* ext-data callback used by libyang */
LY_ERR ext_data_clb(const struct lysc_ext_instance *ext,
                    const struct lyd_node *parent, void *user_data,
                    void **ext_data, ly_bool *ext_data_free);

/* simple error-check macro for helpers */
#define CHECK_RET(call)                                                        \
  do {                                                                         \
    rc = (call);                                                               \
    if (rc != LY_SUCCESS)                                                      \
      goto cleanup;                                                            \
  } while (0)

/* helper functions used to build/attach ext-data (they handle internal checks)
 */
LY_ERR build_yang_library(struct ly_ctx *ctx, struct lyd_node **yl);
LY_ERR build_schema_mounts(struct lyd_node **sm_root);
LY_ERR attach_schema_mounts(struct lyd_node *yl, struct lyd_node *sm_root,
                            struct lyd_node **first);
LY_ERR validate_extdata(struct lyd_node **first);

/* helpers used to programmatically build the example data tree */
LY_ERR create_network_instances(const struct lys_module *mod_ni,
                                struct lyd_node **root);
LY_ERR create_network_instance(struct lyd_node *root, const char *name,
                               struct lyd_node **ni);
LY_ERR create_vrf_root(struct lyd_node *ni, struct lyd_node **vrf);
LY_ERR create_routing(struct lyd_node *vrf, const struct lys_module *mod_rt,
                      struct lyd_node **routing);
LY_ERR create_ribs(struct lyd_node *routing, struct lyd_node **ribs);
LY_ERR create_rib(struct lyd_node *ribs, const char *name,
                  struct lyd_node **rib);
LY_ERR create_address_family(struct lyd_node *rib, const char *val);

/* `CALL_OR_CLEANUP` removed; use `CHECK_RET(call)` instead */

#endif /* EXAMPLE_NI_H */
