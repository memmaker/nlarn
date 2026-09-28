/* Minimal GLib for the NLarn web port (RVIP): only the API NLarn uses,
 * implemented in port/glib/glib.c on top of libc. Found before the real
 * GLib via -Iport/glib. Semantics follow GLib 2.x where NLarn relies on
 * them (NULL-safe frees, g_string_free/g_ptr_array_free returning the data,
 * GList/GQueue layouts, g_strsplit max_tokens, UTF-8 helpers). */
#ifndef NLARN_PORT_GLIB_H
#define NLARN_PORT_GLIB_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <limits.h>
#include <float.h>

typedef char gchar;
typedef unsigned char guchar;
typedef int gint;
typedef unsigned int guint;
typedef short gshort;
typedef unsigned short gushort;
typedef long glong;
typedef unsigned long gulong;
typedef int8_t gint8;
typedef uint8_t guint8;
typedef int16_t gint16;
typedef uint16_t guint16;
typedef int32_t gint32;
typedef uint32_t guint32;
typedef int64_t gint64;
typedef uint64_t guint64;
typedef gint gboolean;
typedef void *gpointer;
typedef const void *gconstpointer;
typedef size_t gsize;
typedef ptrdiff_t gssize;
typedef double gdouble;
typedef float gfloat;
typedef guint32 gunichar;
typedef gchar **GStrv;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define G_GUINT64_FORMAT PRIu64
#define G_GINT64_FORMAT PRId64
#define G_MAXINT INT32_MAX
#define G_MININT INT32_MIN
#define G_MAXUINT UINT32_MAX
#define G_PI 3.1415926535897932384626433832795028841971693993751
#define G_DIR_SEPARATOR '/'
#define G_DIR_SEPARATOR_S "/"
#define G_N_ELEMENTS(arr) (sizeof(arr) / sizeof((arr)[0]))
#define G_GNUC_PRINTF(f, a) __attribute__((format(printf, f, a)))

#undef MIN
#undef MAX
#undef ABS
#undef CLAMP
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define ABS(a) (((a) < 0) ? -(a) : (a))
#define CLAMP(x, lo, hi) (((x) > (hi)) ? (hi) : (((x) < (lo)) ? (lo) : (x)))

#define GPOINTER_TO_UINT(p) ((guint)(uintptr_t)(p))
#define GUINT_TO_POINTER(u) ((gpointer)(uintptr_t)(u))
#define GPOINTER_TO_INT(p) ((gint)(intptr_t)(p))
#define GINT_TO_POINTER(i) ((gpointer)(intptr_t)(i))

typedef void (*GDestroyNotify)(gpointer data);
typedef void (*GFunc)(gpointer data, gpointer user_data);
typedef gint (*GCompareFunc)(gconstpointer a, gconstpointer b);
typedef gint (*GCompareDataFunc)(gconstpointer a, gconstpointer b, gpointer user_data);
typedef guint (*GHashFunc)(gconstpointer key);
typedef gboolean (*GEqualFunc)(gconstpointer a, gconstpointer b);
typedef void (*GHFunc)(gpointer key, gpointer value, gpointer user_data);

/* memory */
gpointer g_malloc(gsize n);
gpointer g_malloc0(gsize n);
gpointer g_realloc(gpointer p, gsize n);
void g_free(gpointer p);
#define g_new(T, n) ((T *)g_malloc(sizeof(T) * (gsize)(n)))
#define g_new0(T, n) ((T *)g_malloc0(sizeof(T) * (gsize)(n)))
void g_autoptr_cleanup_generic_gfree(void *pp);
#define g_autofree __attribute__((cleanup(g_autoptr_cleanup_generic_gfree)))

/* logging and assertions */
typedef enum {
    G_LOG_FLAG_RECURSION = 1 << 0,
    G_LOG_FLAG_FATAL = 1 << 1,
    G_LOG_LEVEL_ERROR = 1 << 2,
    G_LOG_LEVEL_CRITICAL = 1 << 3,
    G_LOG_LEVEL_WARNING = 1 << 4,
    G_LOG_LEVEL_MESSAGE = 1 << 5,
    G_LOG_LEVEL_INFO = 1 << 6,
    G_LOG_LEVEL_DEBUG = 1 << 7,
} GLogLevelFlags;
typedef void (*GLogFunc)(const gchar *log_domain, GLogLevelFlags log_level,
                         const gchar *message, gpointer user_data);
GLogFunc g_log_set_default_handler(GLogFunc func, gpointer user_data);
void g_log_default_handler(const gchar *log_domain, GLogLevelFlags log_level,
                           const gchar *message, gpointer user_data);
void g_log_msg(GLogLevelFlags level, const char *fmt, ...) G_GNUC_PRINTF(2, 3);
void g_assertion_message(const char *file, int line, const char *func,
                         const char *expr) __attribute__((noreturn));

#ifdef G_DISABLE_ASSERT
#define g_assert(expr) ((void)0)
#define g_assert_not_reached() ((void)0)
#else
#define g_assert(expr) \
    ((expr) ? (void)0 : g_assertion_message(__FILE__, __LINE__, __func__, #expr))
#define g_assert_not_reached() \
    g_assertion_message(__FILE__, __LINE__, __func__, "code should not be reached")
#endif
#define g_return_if_fail(expr) do { if (!(expr)) { \
    g_log_msg(G_LOG_LEVEL_CRITICAL, "%s: assertion '%s' failed", __func__, #expr); \
    return; } } while (0)
#define g_return_val_if_fail(expr, val) do { if (!(expr)) { \
    g_log_msg(G_LOG_LEVEL_CRITICAL, "%s: assertion '%s' failed", __func__, #expr); \
    return (val); } } while (0)

/* printing */
gint g_printf(const gchar *fmt, ...) G_GNUC_PRINTF(1, 2);
void g_printerr(const gchar *fmt, ...) G_GNUC_PRINTF(1, 2);
#define g_snprintf snprintf
gchar *g_strdup_printf(const gchar *fmt, ...) G_GNUC_PRINTF(1, 2);
gchar *g_strdup_vprintf(const gchar *fmt, va_list ap);

/* strings */
gchar *g_strdup(const gchar *s);
gchar *g_strndup(const gchar *s, gsize n);
gchar **g_strdupv(gchar **v);
gchar *g_strconcat(const gchar *first, ...) __attribute__((sentinel));
gchar **g_strsplit(const gchar *s, const gchar *delim, gint max_tokens);
gchar *g_strjoinv(const gchar *sep, gchar **v);
void g_strfreev(gchar **v);
guint g_strv_length(gchar **v);
gchar *g_strchug(gchar *s);
gchar *g_strchomp(gchar *s);
#define g_strstrip(s) g_strchomp(g_strchug(s))
gboolean g_str_has_prefix(const gchar *s, const gchar *prefix);
gboolean g_str_has_suffix(const gchar *s, const gchar *suffix);
int g_strcmp0(const char *a, const char *b);
gint g_ascii_strcasecmp(const gchar *a, const gchar *b);
gchar *g_ascii_strdown(const gchar *s, gssize len);
gchar g_ascii_toupper(gchar c);
gchar g_ascii_tolower(gchar c);
#define g_ascii_isspace(c) ((c) == ' ' || ((c) >= '\t' && (c) <= '\r'))
#define g_ascii_isalnum(c) (((c) >= '0' && (c) <= '9') || ((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))
#define g_ascii_ispunct(c) ((c) > 32 && (c) < 127 && !g_ascii_isalnum(c))

/* UTF-8 */
extern const gchar g_utf8_skip_table[256];
#define g_utf8_next_char(p) ((char *)((p) + g_utf8_skip_table[*(const guchar *)(p)]))
glong g_utf8_strlen(const gchar *p, gssize max);
gunichar g_utf8_get_char(const gchar *p);
gchar *g_utf8_offset_to_pointer(const gchar *s, glong offset);
gint g_unichar_to_utf8(gunichar c, gchar *outbuf);
gunichar g_unichar_tolower(gunichar c);
gunichar g_unichar_toupper(gunichar c);

/* GString */
typedef struct {
    gchar *str;
    gsize len;
    gsize allocated_len;
} GString;
GString *g_string_new(const gchar *init);
GString *g_string_new_len(const gchar *init, gssize len);
GString *g_string_sized_new(gsize n);
gchar *g_string_free(GString *s, gboolean free_segment);
GString *g_string_append(GString *s, const gchar *val);
GString *g_string_append_len(GString *s, const gchar *val, gssize len);
GString *g_string_append_c(GString *s, gchar c);
GString *g_string_append_unichar(GString *s, gunichar c);
void g_string_append_printf(GString *s, const gchar *fmt, ...) G_GNUC_PRINTF(2, 3);
void g_string_printf(GString *s, const gchar *fmt, ...) G_GNUC_PRINTF(2, 3);
GString *g_string_prepend(GString *s, const gchar *val);
GString *g_string_prepend_c(GString *s, gchar c);
GString *g_string_insert_len(GString *s, gssize pos, const gchar *val, gssize len);
GString *g_string_insert_c(GString *s, gssize pos, gchar c);
GString *g_string_erase(GString *s, gssize pos, gssize len);
GString *g_string_truncate(GString *s, gsize len);

/* GPtrArray */
typedef struct {
    gpointer *pdata;
    guint len;
    /* private */
    guint alloc;
    GDestroyNotify free_func;
} GPtrArray;
#define g_ptr_array_index(a, i) ((a)->pdata[i])
GPtrArray *g_ptr_array_new(void);
GPtrArray *g_ptr_array_new_with_free_func(GDestroyNotify f);
void g_ptr_array_add(GPtrArray *a, gpointer p);
gpointer *g_ptr_array_free(GPtrArray *a, gboolean free_segment);
gpointer g_ptr_array_remove_index(GPtrArray *a, guint i);
gpointer g_ptr_array_remove_index_fast(GPtrArray *a, guint i);
gboolean g_ptr_array_remove(GPtrArray *a, gpointer p);
gboolean g_ptr_array_remove_fast(GPtrArray *a, gpointer p);
void g_ptr_array_foreach(GPtrArray *a, GFunc f, gpointer user_data);
void g_ptr_array_sort(GPtrArray *a, GCompareFunc f);
void g_ptr_array_sort_with_data(GPtrArray *a, GCompareDataFunc f, gpointer user_data);

/* GArray */
typedef struct {
    gchar *data;
    guint len;
    /* private */
    guint alloc;
    guint elt_size;
    gboolean zero_terminated, clear;
} GArray;
#define g_array_index(a, T, i) (((T *)(void *)(a)->data)[i])
#define g_array_append_val(a, v) g_array_append_vals(a, &(v), 1)
GArray *g_array_new(gboolean zero_terminated, gboolean clear, guint elt_size);
GArray *g_array_sized_new(gboolean zero_terminated, gboolean clear, guint elt_size, guint n);
GArray *g_array_append_vals(GArray *a, gconstpointer data, guint n);
GArray *g_array_set_size(GArray *a, guint n);
GArray *g_array_remove_index(GArray *a, guint i);
void g_array_sort(GArray *a, GCompareFunc f);
gchar *g_array_free(GArray *a, gboolean free_segment);

/* GList */
typedef struct _GList GList;
struct _GList {
    gpointer data;
    GList *next;
    GList *prev;
};
#define g_list_previous(l) ((l) ? ((l)->prev) : NULL)
#define g_list_next(l) ((l) ? ((l)->next) : NULL)
GList *g_list_append(GList *l, gpointer data);
GList *g_list_prepend(GList *l, gpointer data);
void g_list_free(GList *l);
GList *g_list_nth(GList *l, guint n);
GList *g_list_last(GList *l);
guint g_list_length(GList *l);
gint g_list_index(GList *l, gconstpointer data);
GList *g_list_remove(GList *l, gconstpointer data);
GList *g_list_delete_link(GList *l, GList *link);
GList *g_list_copy(GList *l);
GList *g_list_sort(GList *l, GCompareFunc f);
GList *g_list_sort_with_data(GList *l, GCompareDataFunc f, gpointer user_data);

/* GQueue */
typedef struct {
    GList *head;
    GList *tail;
    guint length;
} GQueue;
GQueue *g_queue_new(void);
void g_queue_free(GQueue *q);
gboolean g_queue_is_empty(GQueue *q);
void g_queue_push_head(GQueue *q, gpointer data);
gpointer g_queue_pop_head(GQueue *q);

/* GHashTable */
typedef struct _GHashTable GHashTable;
guint g_direct_hash(gconstpointer v);
gboolean g_direct_equal(gconstpointer a, gconstpointer b);
GHashTable *g_hash_table_new(GHashFunc hash, GEqualFunc eq);
GHashTable *g_hash_table_new_full(GHashFunc hash, GEqualFunc eq,
                                  GDestroyNotify key_free, GDestroyNotify value_free);
gboolean g_hash_table_insert(GHashTable *h, gpointer key, gpointer value);
gpointer g_hash_table_lookup(GHashTable *h, gconstpointer key);
gboolean g_hash_table_remove(GHashTable *h, gconstpointer key);
void g_hash_table_remove_all(GHashTable *h);
void g_hash_table_foreach(GHashTable *h, GHFunc f, gpointer user_data);
guint g_hash_table_size(GHashTable *h);
GList *g_hash_table_get_keys(GHashTable *h);
GList *g_hash_table_get_values(GHashTable *h);
void g_hash_table_destroy(GHashTable *h);

/* errors */
typedef guint32 GQuark;
typedef struct {
    GQuark domain;
    gint code;
    gchar *message;
} GError;
void g_error_free(GError *e);
void g_clear_error(GError **e);

/* files and environment */
typedef enum {
    G_FILE_TEST_IS_REGULAR = 1 << 0,
    G_FILE_TEST_IS_SYMLINK = 1 << 1,
    G_FILE_TEST_IS_DIR = 1 << 2,
    G_FILE_TEST_IS_EXECUTABLE = 1 << 3,
    G_FILE_TEST_EXISTS = 1 << 4,
} GFileTest;
gboolean g_file_test(const gchar *filename, GFileTest test);
gboolean g_file_get_contents(const gchar *filename, gchar **contents,
                             gsize *length, GError **error);
gboolean g_file_set_contents(const gchar *filename, const gchar *contents,
                             gssize length, GError **error);
gchar *g_build_path(const gchar *sep, const gchar *first, ...) __attribute__((sentinel));
gchar *g_build_filename(const gchar *first, ...) __attribute__((sentinel));
gchar *g_path_get_dirname(const gchar *path);
const gchar *g_get_home_dir(void);
const gchar *g_get_user_config_dir(void);
typedef enum { G_USER_DIRECTORY_DESKTOP, G_USER_DIRECTORY_DOCUMENTS } GUserDirectory;
const gchar *g_get_user_special_dir(GUserDirectory d);
const gchar *const *g_get_language_names(void);
gboolean g_setenv(const gchar *name, const gchar *value, gboolean overwrite);
gint64 g_get_monotonic_time(void);

/* key files (ini) */
typedef struct _GKeyFile GKeyFile;
typedef enum { G_KEY_FILE_NONE = 0, G_KEY_FILE_KEEP_COMMENTS = 1,
               G_KEY_FILE_KEEP_TRANSLATIONS = 2 } GKeyFileFlags;
GKeyFile *g_key_file_new(void);
void g_key_file_free(GKeyFile *kf);
gboolean g_key_file_load_from_file(GKeyFile *kf, const gchar *file,
                                   GKeyFileFlags flags, GError **error);
gboolean g_key_file_load_from_data(GKeyFile *kf, const gchar *data, gsize len,
                                   GKeyFileFlags flags, GError **error);
gboolean g_key_file_save_to_file(GKeyFile *kf, const gchar *file, GError **error);
gchar *g_key_file_get_string(GKeyFile *kf, const gchar *group, const gchar *key, GError **error);
gint g_key_file_get_integer(GKeyFile *kf, const gchar *group, const gchar *key, GError **error);
gboolean g_key_file_get_boolean(GKeyFile *kf, const gchar *group, const gchar *key, GError **error);
gchar **g_key_file_get_string_list(GKeyFile *kf, const gchar *group, const gchar *key,
                                   gsize *length, GError **error);
void g_key_file_set_value(GKeyFile *kf, const gchar *group, const gchar *key, const gchar *value);
void g_key_file_set_string(GKeyFile *kf, const gchar *group, const gchar *key, const gchar *value);
void g_key_file_set_integer(GKeyFile *kf, const gchar *group, const gchar *key, gint value);
void g_key_file_set_boolean(GKeyFile *kf, const gchar *group, const gchar *key, gboolean value);
void g_key_file_set_string_list(GKeyFile *kf, const gchar *group, const gchar *key,
                                const gchar *const list[], gsize length);

/* command line options */
typedef enum { G_OPTION_ARG_NONE, G_OPTION_ARG_STRING, G_OPTION_ARG_INT,
               G_OPTION_ARG_FILENAME } GOptionArg;
typedef struct {
    const gchar *long_name;
    gchar short_name;
    gint flags;
    GOptionArg arg;
    gpointer arg_data;
    const gchar *description;
    const gchar *arg_description;
} GOptionEntry;
typedef struct _GOptionContext GOptionContext;
typedef struct _GOptionGroup GOptionGroup;
GOptionContext *g_option_context_new(const gchar *parameter_string);
void g_option_context_add_main_entries(GOptionContext *c, const GOptionEntry *entries,
                                       const gchar *translation_domain);
gboolean g_option_context_parse(GOptionContext *c, gint *argc, gchar ***argv, GError **error);
void g_option_context_free(GOptionContext *c);

/* i18n: contexts are "ctx\004msg" in the catalogue */
const gchar *g_dpgettext2(const gchar *domain, const gchar *context, const gchar *msgid);

#endif
