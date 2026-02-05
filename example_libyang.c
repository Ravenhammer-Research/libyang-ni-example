/*
 * example_libyang.c
 * Simple libyang v2 C example: loads YANG modules from /usr/local/share/yang/modules,
 * builds an `ietf-network-instance` data tree with an `ietf-routing` mount,
 * validates it with libyang and prints XML output.
 *
 * Uses libyang v2 API signatures.
 */

#include <stdio.h>
#include <stdlib.h>
#include <libyang/libyang.h>

/* global context used by ext-data callback */
static struct ly_ctx *g_ctx = NULL;

/* simple log callback to prefix libyang messages */
static void my_log_cb(LY_LOG_LEVEL level, const char *msg, const char *data_path, const char *schema_path, uint64_t line)
{
    const char *lvl = "?";
    if (level == LY_LLERR) lvl = "ERR";
    else if (level == LY_LLWRN) lvl = "WRN";
    else if (level == LY_LLVRB) return;
    else if (level == LY_LLDBG) lvl = "DBG";
    fprintf(stderr, "[libyang %s] %s\n", lvl, msg ? msg : "(null)");
    if (data_path) fprintf(stderr, "  data-path: %s\n", data_path);
    else fprintf(stderr, "  data-path: (null)\n");
    if (schema_path) fprintf(stderr, "  schema-path: %s\n", schema_path);
    else fprintf(stderr, "  schema-path: (null)\n");
    fprintf(stderr, "  line: %llu\n", (unsigned long long)line);
}

static LY_ERR
ext_data_cb(const struct lysc_ext_instance *ext, const struct lyd_node *parent, void *user_data,
        void **ext_data, ly_bool *ext_data_free)
{
    struct lyd_node *data = NULL;
    const struct lys_module *sm_mod;
    (void)ext;
    (void)parent;

    if (user_data) {
        /* avoid recursion while parsing the provided ext-data: keep a callback set but
         * with NULL user_data so nested requests see no user_data. */
        ly_ctx_set_ext_data_clb(g_ctx, ext_data_cb, NULL);
        if (lyd_parse_data_mem(g_ctx, (const char *)user_data, LYD_XML,
                LYD_PARSE_STRICT | LYD_PARSE_ONLY, 0, &data) != LY_SUCCESS) {
            /* restore callback on error */
            ly_ctx_set_ext_data_clb(g_ctx, ext_data_cb, user_data);
            return LY_EINVAL;
        }
        sm_mod = ly_ctx_get_module_implemented(g_ctx, "ietf-yang-schema-mount");
        if (sm_mod) {
            if (lyd_validate_module(&data, sm_mod, 0, NULL) != LY_SUCCESS) {
                ly_ctx_set_ext_data_clb(g_ctx, ext_data_cb, user_data);
                lyd_free_all(data);
                return LY_EVALID;
            }
        }
        /* restore callback */
        ly_ctx_set_ext_data_clb(g_ctx, ext_data_cb, user_data);
    }

    *ext_data = data;
    *ext_data_free = 1;
    return LY_SUCCESS;
}

int main(void)
{
    struct ly_ctx *ctx = NULL;
    struct lyd_node *data = NULL;
    char *out = NULL;

    /* set our logger */
    ly_log_options(LY_LOLOG | LY_LOSTORE_LAST);
    ly_set_log_clb(my_log_cb);

    /* main context */
    if (ly_ctx_new(NULL, 0, &ctx) != LY_SUCCESS) {
        fprintf(stderr, "Failed to create libyang main context\n");
        return 1;
    }

    /* make context find system modules (adjust path if needed) */
    ly_ctx_set_searchdir(ctx, "/usr/local/share/yang/modules/yang/standard/ietf/RFC");

    /* load modules into main context (yang-library + schema-mount + network-instance + routing) */
    ly_ctx_load_module(ctx, "ietf-yang-library", NULL, NULL);
    ly_ctx_load_module(ctx, "ietf-yang-schema-mount", NULL, NULL);
    ly_ctx_load_module(ctx, "ietf-network-instance", NULL, NULL);
    ly_ctx_load_module(ctx, "ietf-routing", NULL, NULL);

    /* register ext-data callback and provide yang-library + schema-mounts (no inline)
     * the callback will parse and validate this XML when requested by the schema-mount
     * extension plugin. */
    const char *ext_xml =
        "<network-instances xmlns=\"urn:ietf:params:xml:ns:yang:ietf-network-instance\">"
        "  <network-instance>"
        "    <name>VRF1</name>"
        "    <vrf-root>"
        "      <yang-library xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-library\">"
        "        <module-set>"
        "          <name>example-set</name>"
        "          <module><name>ietf-yang-library</name></module>"
        "          <module><name>ietf-yang-schema-mount</name></module>"
        "          <module><name>ietf-network-instance</name></module>"
        "          <module><name>ietf-routing</name></module>"
        "        </module-set>"
        "        <content-id>1</content-id>"
        "      </yang-library>"
        "    </vrf-root>"
        "  </network-instance>"
        "</network-instances>"
        "<modules-state xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-library\">"
        "  <module-set-id>1</module-set-id>"
        "</modules-state>"
        "<schema-mounts xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-schema-mount\">"
        "  <mount-point>"
        "    <module>ietf-network-instance</module>"
        "    <label>vrf-root</label>"
        "    <shared-schema/>"
        "  </mount-point>"
        "</schema-mounts>";

    /* set global context and register callback */
    g_ctx = ctx;
    ly_ctx_set_ext_data_clb(ctx, ext_data_cb, (void *)ext_xml);

    /* parse input XML (the mounted subtree is present in the data and will be validated using ext-data) */
    const char *input_xml =
        "<network-instances xmlns=\"urn:ietf:params:xml:ns:yang:ietf-network-instance\" xmlns:rt=\"urn:ietf:params:xml:ns:yang:ietf-routing\">"
        "  <network-instance>"
        "    <name>VRF1</name>"
        "    <vrf-root>"
        "      <rt:routing>"
        "        <ribs>"
        "          <rib>"
        "            <name>default</name>"
        "          </rib>"
        "        </ribs>"
        "      </rt:routing>"
        "    </vrf-root>"
        "  </network-instance>"
        "</network-instances>";

    if (lyd_parse_data_mem(ctx, input_xml, LYD_XML, LYD_PARSE_STRICT, LYD_VALIDATE_PRESENT, &data) != LY_SUCCESS) {
        fprintf(stderr, "Failed to parse example data (ensure searchdir and ext-data callback are correct)\n");
        ly_ctx_set_ext_data_clb(ctx, NULL, NULL);
        ly_ctx_destroy(ctx);
        return 1;
    }

    if (lyd_print_mem(&out, data, LYD_XML, LYD_PRINT_SIBLINGS) == LY_SUCCESS) {
        printf("Parsed data:\n%s\n", out);
        free(out);
    }

    lyd_free_all(data);
    ly_ctx_destroy(ctx);
    return 0;
}
