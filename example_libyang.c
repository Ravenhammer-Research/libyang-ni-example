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
#include <execinfo.h>
#include <unistd.h>
#include <dlfcn.h>

/* Use C++ demangler if available (libstdc++). Declare the function manually to avoid
 * depending on a C++ header in this C source. */
extern char *__cxa_demangle(const char *mangled_name, char *output_buffer, size_t *length, int *status);

/* simple log callback to prefix libyang messages */
static void my_log_cb(LY_LOG_LEVEL level, const char *msg, const char *data_path, const char *schema_path, uint64_t line)
{
    const char *lvl = "?";
    if (level == LY_LLERR) lvl = "ERR";
    else if (level == LY_LLWRN) lvl = "WRN";
    else if (level == LY_LLVRB) lvl = "VRB";
    else if (level == LY_LLDBG) lvl = "DBG";
    fprintf(stderr, "[libyang %s] %s\n", lvl, msg ? msg : "(null)");
    if (data_path) fprintf(stderr, "  data-path: %s\n", data_path);
    if (schema_path) fprintf(stderr, "  schema-path: %s\n", schema_path);
    if (line) fprintf(stderr, "  line: %llu\n", (unsigned long long)line);

    /* Print a native stack trace only for libyang error-level messages */
    if (level == LY_LLERR) {
        void *bt[32];
        int bt_size = backtrace(bt, 32);
        fprintf(stderr, "Stack trace (most recent call first):\n");
        for (int i = 0; i < bt_size; ++i) {
            Dl_info dlinfo;
            if (dladdr(bt[i], &dlinfo) && dlinfo.dli_sname) {
                int status = 0;
                char *demangled = __cxa_demangle(dlinfo.dli_sname, NULL, NULL, &status);
                if (status == 0 && demangled) {
                    fprintf(stderr, "  %p <%s> (%s)\n", bt[i], dlinfo.dli_fname ? dlinfo.dli_fname : "?", demangled);
                    free(demangled);
                } else {
                    fprintf(stderr, "  %p <%s> (%s)\n", bt[i], dlinfo.dli_fname ? dlinfo.dli_fname : "?", dlinfo.dli_sname);
                }
            } else {
                /* fallback to raw symbol if dladdr fails */
                fprintf(stderr, "  %p\n", bt[i]);
            }
        }
    }
}

/* ext-data callback to provide mounted data for yangmnt:mount-point */
static LY_ERR
mount_ext_data_cb(const struct lysc_ext_instance *ext, const struct lyd_node *parent, void *user_data,
                  void **ext_data, ly_bool *ext_data_free)
{
    struct ly_ctx *mctx = (struct ly_ctx *)user_data;
    const struct lys_module *m_rt = NULL;
    struct lyd_node *mount_root = NULL, *inst = NULL;
    LY_ERR rc;

    (void)ext;
    /* build a small routing subtree in the mount context */
    m_rt = ly_ctx_get_module_implemented(mctx, "ietf-routing");
    if (!m_rt) {
        return LY_EINVAL;
    }

    rc = lyd_new_inner(NULL, m_rt, "routing", 0, &mount_root);
    if (rc != LY_SUCCESS || !mount_root) {
        return rc;
    }

    rc = lyd_new_list2(mount_root, m_rt, "instance", "[name='main']", 0, &inst);
    if (rc != LY_SUCCESS || !inst) {
        lyd_free_all(mount_root);
        return rc;
    }

    /* ensure the key leaf 'name' has the value */
    rc = lyd_new_term(inst, m_rt, "name", "main", 0, NULL);
    if (rc != LY_SUCCESS) {
        lyd_free_all(mount_root);
        return rc;
    }

    /* Add ietf-yang-library data describing the mount context */
    struct lyd_node *ylib = NULL;
    if (ly_ctx_get_yanglib_data(mctx, &ylib, "%u", ly_ctx_get_change_count(mctx)) == LY_SUCCESS && ylib) {
        /* insert our mount_root as a sibling of yang-library root so ext_data contains both */
        struct lyd_node *first = NULL;
        lyd_insert_sibling(ylib, mount_root, &first);
        *ext_data = first ? first : mount_root;
    } else {
        /* no yanglib data, return only mount_root */
        *ext_data = mount_root;
    }
    *ext_data_free = 1; /* let libyang free it when context is destroyed */
    return LY_SUCCESS;
}

int main(void)
{
    struct ly_ctx *ctx = NULL;
    const struct lys_module *m_ni = NULL, *m_rt = NULL;
    struct lyd_node *root = NULL, *node = NULL, *tmp = NULL;
    char *xml = NULL;
    LY_ERR rc;

    /* Create context with search dir where YANG modules are located */
    /* enable verbose debug logging from libyang */
    ly_set_log_clb(my_log_cb);
    /* use verbose level but avoid heavy DICT debug group to reduce noise */
    ly_log_level(LY_LLVRB);
    ly_log_options(LY_LOLOG | LY_LOSTORE_LAST);
        /* disable the noisy DICT and DEPSETS debug groups */
        ly_log_dbg_groups(0);

    rc = ly_ctx_new("/usr/local/share/yang/modules", 0, &ctx);
    if (rc != LY_SUCCESS) {
        fprintf(stderr, "Failed to create libyang context\n");
        return 1;
    }

    fprintf(stderr, "Created main context.\n");

    /* Load schema modules we will use */
    m_ni = ly_ctx_load_module(ctx, "ietf-network-instance", "2019-01-21", NULL);
    if (!m_ni) {
        fprintf(stderr, "Failed to load module 'ietf-network-instance'\n");
        ly_ctx_destroy(ctx);
        return 1;
    }
    fprintf(stderr, "Loaded module: %s\n", m_ni->name);

    /* enumerate modules in main context */
    fprintf(stderr, "Modules in main context:\n");
    uint32_t mi = 0; struct lys_module *miter = NULL;
    while ((miter = ly_ctx_get_module_iter(ctx, &mi))) {
        fprintf(stderr, " - %s@%s%s\n", miter->name, miter->revision ? miter->revision : "(none)", miter->implemented ? " (implemented)" : "");
    }

    /* Create a separate context for mounted schemas and load ietf-routing into it */
    struct ly_ctx *mctx = NULL;
    m_rt = NULL;
    rc = ly_ctx_new("/usr/local/share/yang/modules", 0, &mctx);
    if (rc != LY_SUCCESS) {
        fprintf(stderr, "Failed to create mount context\n");
        ly_ctx_destroy(ctx);
        return 1;
    }
    fprintf(stderr, "Created mount context.\n");
    m_rt = ly_ctx_load_module(mctx, "ietf-routing", NULL, NULL);
    if (!m_rt) {
        fprintf(stderr, "Warning: failed to load module 'ietf-routing' into mount context\n");
        /* continue, ext callback will fail if used */
    }
    if (m_rt) fprintf(stderr, "Loaded mount module: %s\n", m_rt->name);
    /* enumerate mount context modules */
    fprintf(stderr, "Modules in mount context:\n");
    mi = 0; miter = NULL;
    while ((miter = ly_ctx_get_module_iter(mctx, &mi))) {
        fprintf(stderr, " - %s@%s%s\n", miter->name, miter->revision ? miter->revision : "(none)", miter->implemented ? " (implemented)" : "");
    }
    /* ensure ietf-yang-library is available in mount context for yang-library data */
    if (!ly_ctx_get_module_implemented(mctx, "ietf-yang-library")) {
        ly_ctx_load_module(mctx, "ietf-yang-library", NULL, NULL);
    }

    /* Register ext-data callback so mount-points can get mounted data */
    ly_ctx_set_ext_data_clb(ctx, mount_ext_data_cb, mctx);

    /* Create top-level container: /ietf-network-instance:network-instances */
    rc = lyd_new_inner(NULL, m_ni, "network-instances", 0, &root);
    if (rc != LY_SUCCESS || !root) {
        fprintf(stderr, "Failed to create root container\n");
        ly_ctx_destroy(ctx);
        return 1;
    }
    fprintf(stderr, "Created root container.\n");

    /* Create a network-instance */
        /* Create a network-instance list instance with key name='inst1' */
        rc = lyd_new_list2(root, m_ni, "network-instance", "[name='inst1']", 0, &node);
        if (rc != LY_SUCCESS || !node) {
            fprintf(stderr, "Failed to create network-instance list instance\n");
            lyd_free_all(root);
            ly_ctx_destroy(ctx);
            return 1;
        }
        fprintf(stderr, "Created network-instance 'inst1'.\n");

    /* Create routing mount/child */
    if (m_rt) {
        /* The routing nodes are mounted under the mount-point (vrf-root, vsi-root, etc.).
         * We create the vrf-root container and then rely on the ext-data callback to provide
         * the mounted routing data. */
        rc = lyd_new_inner(node, m_ni, "vrf-root", 0, &tmp);
        if (rc != LY_SUCCESS) {
            fprintf(stderr, "Failed to create routing container\n");
            lyd_free_all(root);
            ly_ctx_destroy(ctx);
            return 1;
        }
        /* now trigger creation of mounted data by calling the ext callback via lyplg_ext_get_data() */
        /* libyang will call the registered callback when validating or when ext data is needed; to ensure it's available,
         * we can request the extension data explicitly using lyplg_ext_get_data(). */
        /* Note: lyplg_ext_get_data requires the compiled ext instance; use lyplg_ext_get_data via API is optional here.
         * We will rely on validation to invoke the callback. */
    } else {
        rc = lyd_new_inner(node, m_ni, "routing", 0, &tmp);
        if (rc != LY_SUCCESS) {
            fprintf(stderr, "Failed to create fallback routing container\n");
            lyd_free_all(root);
            ly_ctx_destroy(ctx);
            return 1;
        }
    }

    /* Validate the data tree against the schema */
    rc = lyd_validate_all(&root, ctx, 0, NULL);
    if (rc != LY_SUCCESS) {
        fprintf(stderr, "Data tree validation failed\n");
        /* Avoid calling ly_err_print here because it uses the log callback
         * (which would invoke our `my_log_cb` again and produce a duplicate
         * error + stack trace). Print the last error fields directly. */
        const struct ly_err_item *err = ly_err_last(ctx);
        if (err) {
            fprintf(stderr, "libyang error: %s\n", err->msg ? err->msg : "(no msg)");
        } else {
            const char *m = ly_last_logmsg();
            if (m) fprintf(stderr, "libyang log: %s\n", m);
        }
        lyd_free_all(root);
        ly_ctx_destroy(ctx);
        return 1;
    }

    /* Print XML output of the constructed data tree */
    rc = lyd_print_mem(&xml, root, LYD_XML, 0);
    if (rc != LY_SUCCESS) {
        fprintf(stderr, "Failed to print data tree as XML\n");
        lyd_free_all(root);
        ly_ctx_destroy(ctx);
        return 1;
    }

    printf("Data tree as XML:\n%s\n", xml);

    /* Cleanup */
    free(xml);
    lyd_free_all(root);
    ly_ctx_destroy(ctx);

    return 0;
}
