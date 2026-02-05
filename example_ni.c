/*
 * example_ni.c
 * Simple libyang example demonstrating ietf-network-instance with
 * a schema-mount of ietf-routing under the `vrf-root` mount point.
 *
 * Build with:
 *   gcc example_ni.c -o example_ni $(pkg-config --cflags --libs libyang)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "example_ni.h"

static struct ly_ctx *g_ctx = NULL;

/* ext-data to provide for schema-mount: a minimal yang-library + schema-mounts
 * describing mounting of ietf-routing at the mount-point label "vrf-root"
 * defined in module ietf-network-instance.
 */
static const char *ext_data_xml =
    "<yang-library xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-library\">"
    "  <module-set>"
    "    <name>set1</name>"
    "    <module>"
    "      <name>ietf-routing</name>"
    "      <namespace>urn:ietf:params:xml:ns:yang:ietf-routing</namespace>"
    "    </module>"
    "  </module-set>"
    "  <content-id>1</content-id>"
    "</yang-library>"
    "<schema-mounts xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-schema-mount\">"
    "  <mount-point>"
    "    <module>ietf-network-instance</module>"
    "    <label>vrf-root</label>"
    "    <inline/>"
    "  </mount-point>"
    "</schema-mounts>";

/* callback invoked by libyang to get extension data for a mount-point instance */
LY_ERR ext_data_clb(const struct lysc_ext_instance *ext, const struct lyd_node *parent, void *user_data,
             void **ext_data, ly_bool *ext_data_free)
{
    struct lyd_node *data = NULL;
    const char *xml = (const char *)user_data;

    (void)ext;
    (void)parent;

    if (!g_ctx || !xml) {
        return LY_EINVAL;
    }

    /* parse the provided XML (yang-library + schema-mounts) in the current context */
    if (lyd_parse_data_mem(g_ctx, xml, LYD_XML, LYD_PARSE_ONLY, 0, &data) != LY_SUCCESS) {
        return LY_EINVAL;
    }

    /* validate the parsed ext-data against the ietf-yang-schema-mount module */
    const struct lys_module *sm_mod = ly_ctx_get_module_implemented(g_ctx, "ietf-yang-schema-mount");
    if (!sm_mod) {
        lyd_free_all(data);
        return LY_EINVAL;
    }
    if (lyd_validate_module(&data, sm_mod, 0, NULL) != LY_SUCCESS) {
        lyd_free_all(data);
        return LY_EINVAL;
    }

    *ext_data = data;
    *ext_data_free = 1; /* libyang should free the returned data when appropriate */
    return LY_SUCCESS;
}

/* creation helpers */
LY_ERR create_network_instances(const struct lys_module *mod_ni, struct lyd_node **root)
{
    return lyd_new_inner(NULL, mod_ni, "network-instances", 0, root);
}

LY_ERR create_network_instance(struct lyd_node *root, const char *name, struct lyd_node **ni)
{
    return lyd_new_list(root, NULL, "network-instance", 0, ni, name);
}

LY_ERR create_vrf_root(struct lyd_node *ni, struct lyd_node **vrf)
{
    return lyd_new_inner(ni, NULL, "vrf-root", 0, vrf);
}

LY_ERR create_routing(struct lyd_node *vrf, const struct lys_module *mod_rt, struct lyd_node **routing)
{
    return lyd_new_inner(vrf, mod_rt, "routing", 0, routing);
}

LY_ERR create_ribs(struct lyd_node *routing, struct lyd_node **ribs)
{
    return lyd_new_inner(routing, NULL, "ribs", 0, ribs);
}

LY_ERR create_rib(struct lyd_node *ribs, const char *name, struct lyd_node **rib)
{
    return lyd_new_list(ribs, NULL, "rib", 0, rib, name);
}

LY_ERR create_address_family(struct lyd_node *rib, const char *val)
{
    return lyd_new_term(rib, NULL, "address-family", val, 0, NULL);
}

int
main(void)
{
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

    /* register ext-data callback (will be called when parsing data under mount-points) */
    ly_ctx_set_ext_data_clb(ctx, ext_data_clb, (void *)ext_data_xml);

    /* Build the same data tree programmatically using libyang APIs */
    const struct lys_module *mod_ni = ly_ctx_get_module_implemented(ctx, "ietf-network-instance");
    const struct lys_module *mod_rt = ly_ctx_get_module_implemented(ctx, "ietf-routing");
    if (!mod_ni || !mod_rt) {
        fprintf(stderr, "Required modules not loaded\n");
        ly_ctx_destroy(ctx);
        return 1;
    }

    struct lyd_node *root = NULL, *ni = NULL, *vrf = NULL, *routing = NULL, *ribs = NULL, *rib = NULL;

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
