/* Minimal GLib for the NLarn web port (RVIP), see glib.h. */
#include "glib.h"

#include <ctype.h>
#include <errno.h>
#include <libintl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <wctype.h>

/* ---------------------------------------------------------------- memory */

static void oom(gsize n)
{
    fprintf(stderr, "GLib shim: failed to allocate %zu bytes\n", n);
    abort();
}

gpointer g_malloc(gsize n)
{
    if (n == 0) return NULL;
    void *p = malloc(n);
    if (!p) oom(n);
    return p;
}

gpointer g_malloc0(gsize n)
{
    if (n == 0) return NULL;
    void *p = calloc(1, n);
    if (!p) oom(n);
    return p;
}

gpointer g_realloc(gpointer p, gsize n)
{
    if (n == 0) { free(p); return NULL; }
    void *q = realloc(p, n);
    if (!q) oom(n);
    return q;
}

void g_free(gpointer p) { free(p); }

void g_autoptr_cleanup_generic_gfree(void *pp) { g_free(*(void **)pp); }

/* --------------------------------------------------------------- logging */

static GLogFunc log_handler = NULL;
static gpointer log_handler_data = NULL;

GLogFunc g_log_set_default_handler(GLogFunc func, gpointer user_data)
{
    GLogFunc old = log_handler ? log_handler : g_log_default_handler;
    log_handler = func;
    log_handler_data = user_data;
    return old;
}

void g_log_default_handler(const gchar *log_domain, GLogLevelFlags log_level,
                           const gchar *message, gpointer user_data)
{
    (void)user_data;
    const char *lvl = (log_level & G_LOG_LEVEL_ERROR) ? "ERROR"
        : (log_level & G_LOG_LEVEL_CRITICAL) ? "CRITICAL"
        : (log_level & G_LOG_LEVEL_WARNING) ? "WARNING" : "Message";
    fprintf(stderr, "%s%s%s: %s\n", log_domain ? log_domain : "",
            log_domain ? "-" : "", lvl, message ? message : "(null)");
    if (log_level & (G_LOG_LEVEL_ERROR | G_LOG_FLAG_FATAL))
        abort();
}

static void g_logv(GLogLevelFlags level, const char *fmt, va_list ap)
{
    char *msg = g_strdup_vprintf(fmt, ap);
    if (log_handler)
        log_handler(NULL, level, msg, log_handler_data);
    else
        g_log_default_handler(NULL, level, msg, NULL);
    g_free(msg);
}

void g_log_msg(GLogLevelFlags level, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    g_logv(level, fmt, ap);
    va_end(ap);
}

void g_assertion_message(const char *file, int line, const char *func,
                         const char *expr)
{
    g_log_msg(G_LOG_LEVEL_ERROR | G_LOG_FLAG_FATAL,
              "%s:%d:%s: assertion failed: (%s)", file, line, func, expr);
    abort();
}

/* -------------------------------------------------------------- printing */

gint g_printf(const gchar *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vprintf(fmt, ap);
    va_end(ap);
    return r;
}

void g_printerr(const gchar *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

gchar *g_strdup_vprintf(const gchar *fmt, va_list ap)
{
    va_list aq;
    va_copy(aq, ap);
    int n = vsnprintf(NULL, 0, fmt, aq);
    va_end(aq);
    if (n < 0) n = 0;
    char *s = g_malloc((gsize)n + 1);
    vsnprintf(s, (size_t)n + 1, fmt, ap);
    return s;
}

gchar *g_strdup_printf(const gchar *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *s = g_strdup_vprintf(fmt, ap);
    va_end(ap);
    return s;
}

/* --------------------------------------------------------------- strings */

gchar *g_strdup(const gchar *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *d = g_malloc(n);
    memcpy(d, s, n);
    return d;
}

gchar *g_strndup(const gchar *s, gsize n)
{
    if (!s) return NULL;
    char *d = g_malloc(n + 1);
    strncpy(d, s, n);
    d[n] = 0;
    return d;
}

gchar **g_strdupv(gchar **v)
{
    if (!v) return NULL;
    guint n = g_strv_length(v);
    gchar **r = g_new(gchar *, n + 1);
    for (guint i = 0; i < n; i++) r[i] = g_strdup(v[i]);
    r[n] = NULL;
    return r;
}

gchar *g_strconcat(const gchar *first, ...)
{
    if (!first) return NULL;
    va_list ap;
    size_t n = strlen(first);
    va_start(ap, first);
    for (const char *s; (s = va_arg(ap, const char *));) n += strlen(s);
    va_end(ap);
    char *r = g_malloc(n + 1), *p = r;
    p = stpcpy(p, first);
    va_start(ap, first);
    for (const char *s; (s = va_arg(ap, const char *));) p = stpcpy(p, s);
    va_end(ap);
    return r;
}

gchar **g_strsplit(const gchar *s, const gchar *delim, gint max_tokens)
{
    GPtrArray *a = g_ptr_array_new();
    if (max_tokens < 1) max_tokens = G_MAXINT;
    size_t dl = strlen(delim);
    const char *rest = s;
    if (*s)
    {
        const char *hit;
        while (--max_tokens > 0 && (hit = strstr(rest, delim)))
        {
            g_ptr_array_add(a, g_strndup(rest, (gsize)(hit - rest)));
            rest = hit + dl;
        }
        g_ptr_array_add(a, g_strdup(rest));
    }
    g_ptr_array_add(a, NULL);
    return (gchar **)g_ptr_array_free(a, FALSE);
}

gchar *g_strjoinv(const gchar *sep, gchar **v)
{
    if (!sep) sep = "";
    GString *s = g_string_new("");
    for (guint i = 0; v[i]; i++)
    {
        if (i) g_string_append(s, sep);
        g_string_append(s, v[i]);
    }
    return g_string_free(s, FALSE);
}

void g_strfreev(gchar **v)
{
    if (!v) return;
    for (guint i = 0; v[i]; i++) g_free(v[i]);
    g_free(v);
}

guint g_strv_length(gchar **v)
{
    guint n = 0;
    while (v && v[n]) n++;
    return n;
}

gchar *g_strchug(gchar *s)
{
    char *p = s;
    while (*p && g_ascii_isspace((guchar)*p)) p++;
    memmove(s, p, strlen(p) + 1);
    return s;
}

gchar *g_strchomp(gchar *s)
{
    size_t n = strlen(s);
    while (n && g_ascii_isspace((guchar)s[n - 1])) s[--n] = 0;
    return s;
}

gboolean g_str_has_prefix(const gchar *s, const gchar *prefix)
{
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

gboolean g_str_has_suffix(const gchar *s, const gchar *suffix)
{
    size_t a = strlen(s), b = strlen(suffix);
    return a >= b && strcmp(s + a - b, suffix) == 0;
}

int g_strcmp0(const char *a, const char *b)
{
    if (!a) return -(a != b);
    if (!b) return a != b;
    return strcmp(a, b);
}

gchar g_ascii_toupper(gchar c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }
gchar g_ascii_tolower(gchar c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

gint g_ascii_strcasecmp(const gchar *a, const gchar *b)
{
    while (*a && *b)
    {
        int d = (guchar)g_ascii_tolower(*a) - (guchar)g_ascii_tolower(*b);
        if (d) return d;
        a++, b++;
    }
    return (guchar)g_ascii_tolower(*a) - (guchar)g_ascii_tolower(*b);
}

gchar *g_ascii_strdown(const gchar *s, gssize len)
{
    if (len < 0) len = (gssize)strlen(s);
    char *r = g_strndup(s, (gsize)len);
    for (char *p = r; *p; p++) *p = g_ascii_tolower(*p);
    return r;
}

/* ----------------------------------------------------------------- UTF-8 */

#define S6 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
const gchar g_utf8_skip_table[256] = {
    S6, S6, S6, S6, S6, S6, S6, S6, /* 0x00-0x7f */
    S6, S6, S6, S6,                 /* 0x80-0xbf continuation: skip 1 */
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3, 4,4,4,4,4,4,4,4,5,5,5,5,6,6,1,1
};
#undef S6

glong g_utf8_strlen(const gchar *p, gssize max)
{
    glong n = 0;
    const gchar *start = p;
    if (!p) return 0;
    if (max < 0)
    {
        while (*p) { p = g_utf8_next_char(p); n++; }
        return n;
    }
    while (p - start < max && *p)
    {
        p = g_utf8_next_char(p);
        if (p - start <= max) n++;
    }
    return n;
}

gunichar g_utf8_get_char(const gchar *s)
{
    const guchar *p = (const guchar *)s;
    guchar c = p[0];
    if (c < 0x80) return c;
    int len = g_utf8_skip_table[c];
    gunichar r = c & (0x7f >> len);
    for (int i = 1; i < len; i++)
    {
        if ((p[i] & 0xc0) != 0x80) return (gunichar)-1;
        r = (r << 6) | (p[i] & 0x3f);
    }
    return r;
}

gchar *g_utf8_offset_to_pointer(const gchar *s, glong offset)
{
    if (offset >= 0)
    {
        while (offset-- > 0) s = g_utf8_next_char(s);
    }
    else
    {
        while (offset++ < 0)
        {
            s--;
            while (((guchar)*s & 0xc0) == 0x80) s--;
        }
    }
    return (gchar *)s;
}

gint g_unichar_to_utf8(gunichar c, gchar *out)
{
    int len, first;
    if (c < 0x80) { first = 0; len = 1; }
    else if (c < 0x800) { first = 0xc0; len = 2; }
    else if (c < 0x10000) { first = 0xe0; len = 3; }
    else if (c < 0x200000) { first = 0xf0; len = 4; }
    else if (c < 0x4000000) { first = 0xf8; len = 5; }
    else { first = 0xfc; len = 6; }
    if (out)
    {
        for (int i = len - 1; i > 0; i--)
        {
            out[i] = (gchar)((c & 0x3f) | 0x80);
            c >>= 6;
        }
        out[0] = (gchar)(c | (gunichar)first);
    }
    return len;
}

gunichar g_unichar_tolower(gunichar c) { return (gunichar)towlower((wint_t)c); }
gunichar g_unichar_toupper(gunichar c) { return (gunichar)towupper((wint_t)c); }

/* --------------------------------------------------------------- GString */

static void gs_grow(GString *s, gsize extra)
{
    if (s->len + extra + 1 > s->allocated_len)
    {
        gsize n = s->allocated_len ? s->allocated_len : 16;
        while (n < s->len + extra + 1) n *= 2;
        s->str = g_realloc(s->str, n);
        s->allocated_len = n;
    }
}

GString *g_string_sized_new(gsize n)
{
    GString *s = g_new0(GString, 1);
    gs_grow(s, n ? n : 1);
    s->str[0] = 0;
    return s;
}

GString *g_string_new_len(const gchar *init, gssize len)
{
    if (len < 0) return g_string_new(init);
    GString *s = g_string_sized_new((gsize)len);
    if (init) g_string_append_len(s, init, len);
    return s;
}

GString *g_string_new(const gchar *init)
{
    GString *s = g_string_sized_new(init ? strlen(init) : 1);
    if (init) g_string_append(s, init);
    return s;
}

gchar *g_string_free(GString *s, gboolean free_segment)
{
    if (!s) return NULL;
    gchar *r = s->str;
    if (free_segment) { g_free(r); r = NULL; }
    g_free(s);
    return r;
}

GString *g_string_insert_len(GString *s, gssize pos, const gchar *val, gssize len)
{
    if (len < 0) len = (gssize)strlen(val);
    if (pos < 0 || (gsize)pos > s->len) pos = (gssize)s->len;
    /* val may point into s->str */
    gchar *copy = g_strndup(val, (gsize)len);
    gs_grow(s, (gsize)len);
    memmove(s->str + pos + len, s->str + pos, s->len - (gsize)pos);
    memcpy(s->str + pos, copy, (size_t)len);
    g_free(copy);
    s->len += (gsize)len;
    s->str[s->len] = 0;
    return s;
}

GString *g_string_append_len(GString *s, const gchar *val, gssize len)
{
    return g_string_insert_len(s, -1, val, len);
}

GString *g_string_append(GString *s, const gchar *val)
{
    return g_string_insert_len(s, -1, val, -1);
}

GString *g_string_insert_c(GString *s, gssize pos, gchar c)
{
    return g_string_insert_len(s, pos, &c, 1);
}

GString *g_string_append_c(GString *s, gchar c) { return g_string_insert_c(s, -1, c); }
GString *g_string_prepend(GString *s, const gchar *val) { return g_string_insert_len(s, 0, val, -1); }
GString *g_string_prepend_c(GString *s, gchar c) { return g_string_insert_c(s, 0, c); }

GString *g_string_append_unichar(GString *s, gunichar c)
{
    gchar buf[8];
    int n = g_unichar_to_utf8(c, buf);
    return g_string_append_len(s, buf, n);
}

static void gs_append_vprintf(GString *s, const gchar *fmt, va_list ap)
{
    gchar *t = g_strdup_vprintf(fmt, ap);
    g_string_append(s, t);
    g_free(t);
}

void g_string_append_printf(GString *s, const gchar *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    gs_append_vprintf(s, fmt, ap);
    va_end(ap);
}

void g_string_printf(GString *s, const gchar *fmt, ...)
{
    va_list ap;
    g_string_truncate(s, 0);
    va_start(ap, fmt);
    gs_append_vprintf(s, fmt, ap);
    va_end(ap);
}

GString *g_string_erase(GString *s, gssize pos, gssize len)
{
    if (pos < 0 || (gsize)pos > s->len) return s;
    if (len < 0 || (gsize)(pos + len) > s->len) len = (gssize)s->len - pos;
    memmove(s->str + pos, s->str + pos + len, s->len - (gsize)(pos + len));
    s->len -= (gsize)len;
    s->str[s->len] = 0;
    return s;
}

GString *g_string_truncate(GString *s, gsize len)
{
    if (len < s->len) { s->len = len; s->str[len] = 0; }
    return s;
}

/* ---------------------------------------------------- stable merge sort */

static void msort_rec(char *b, char *tmp, size_t n, size_t sz,
                      GCompareDataFunc cmp, gpointer data)
{
    if (n < 2) return;
    size_t n1 = n / 2, n2 = n - n1;
    char *b1 = b, *b2 = b + n1 * sz;
    msort_rec(b1, tmp, n1, sz, cmp, data);
    msort_rec(b2, tmp, n2, sz, cmp, data);
    char *out = tmp;
    while (n1 > 0 && n2 > 0)
    {
        if (cmp(b1, b2, data) <= 0) { memcpy(out, b1, sz); b1 += sz; n1--; }
        else { memcpy(out, b2, sz); b2 += sz; n2--; }
        out += sz;
    }
    if (n1 > 0) memcpy(out, b1, n1 * sz);
    memcpy(b, tmp, (n - n2) * sz);
}

static void g_msort(void *base, size_t n, size_t sz, GCompareDataFunc cmp, gpointer data)
{
    if (n < 2) return;
    char *tmp = g_malloc(n * sz);
    msort_rec(base, tmp, n, sz, cmp, data);
    g_free(tmp);
}

static gint cmp_nodata(gconstpointer a, gconstpointer b, gpointer f)
{
    return ((GCompareFunc)f)(a, b);
}

/* ------------------------------------------------------------- GPtrArray */

GPtrArray *g_ptr_array_new_with_free_func(GDestroyNotify f)
{
    GPtrArray *a = g_new0(GPtrArray, 1);
    a->free_func = f;
    return a;
}

GPtrArray *g_ptr_array_new(void) { return g_ptr_array_new_with_free_func(NULL); }

void g_ptr_array_add(GPtrArray *a, gpointer p)
{
    if (a->len + 1 > a->alloc)
    {
        a->alloc = a->alloc ? a->alloc * 2 : 16;
        a->pdata = g_realloc(a->pdata, a->alloc * sizeof(gpointer));
    }
    a->pdata[a->len++] = p;
}

gpointer *g_ptr_array_free(GPtrArray *a, gboolean free_segment)
{
    if (!a) return NULL;
    gpointer *r = a->pdata;
    if (free_segment)
    {
        if (a->free_func)
            for (guint i = 0; i < a->len; i++) a->free_func(a->pdata[i]);
        g_free(r);
        r = NULL;
    }
    else if (!r)
    {
        r = g_new0(gpointer, 1);
    }
    g_free(a);
    return r;
}

gpointer g_ptr_array_remove_index(GPtrArray *a, guint i)
{
    g_return_val_if_fail(i < a->len, NULL);
    gpointer p = a->pdata[i];
    if (a->free_func) a->free_func(p);
    memmove(a->pdata + i, a->pdata + i + 1, (a->len - i - 1) * sizeof(gpointer));
    a->len--;
    return p;
}

gpointer g_ptr_array_remove_index_fast(GPtrArray *a, guint i)
{
    g_return_val_if_fail(i < a->len, NULL);
    gpointer p = a->pdata[i];
    if (a->free_func) a->free_func(p);
    a->pdata[i] = a->pdata[a->len - 1];
    a->len--;
    return p;
}

gboolean g_ptr_array_remove(GPtrArray *a, gpointer p)
{
    for (guint i = 0; i < a->len; i++)
        if (a->pdata[i] == p) { g_ptr_array_remove_index(a, i); return TRUE; }
    return FALSE;
}

gboolean g_ptr_array_remove_fast(GPtrArray *a, gpointer p)
{
    for (guint i = 0; i < a->len; i++)
        if (a->pdata[i] == p) { g_ptr_array_remove_index_fast(a, i); return TRUE; }
    return FALSE;
}

void g_ptr_array_foreach(GPtrArray *a, GFunc f, gpointer user_data)
{
    for (guint i = 0; i < a->len; i++) f(a->pdata[i], user_data);
}

void g_ptr_array_sort(GPtrArray *a, GCompareFunc f)
{
    g_msort(a->pdata, a->len, sizeof(gpointer), cmp_nodata, (gpointer)f);
}

void g_ptr_array_sort_with_data(GPtrArray *a, GCompareDataFunc f, gpointer user_data)
{
    g_msort(a->pdata, a->len, sizeof(gpointer), f, user_data);
}

/* ---------------------------------------------------------------- GArray */

static void ga_reserve(GArray *a, guint n)
{
    guint need = n + (a->zero_terminated ? 1 : 0);
    if (need > a->alloc)
    {
        guint na = a->alloc ? a->alloc : 16;
        while (na < need) na *= 2;
        a->data = g_realloc(a->data, (gsize)na * a->elt_size);
        memset(a->data + (gsize)a->alloc * a->elt_size, 0,
               (gsize)(na - a->alloc) * a->elt_size);
        a->alloc = na;
    }
}

static void ga_terminate(GArray *a)
{
    if (a->zero_terminated)
        memset(a->data + (gsize)a->len * a->elt_size, 0, a->elt_size);
}

GArray *g_array_sized_new(gboolean zero_terminated, gboolean clear, guint elt_size, guint n)
{
    GArray *a = g_new0(GArray, 1);
    a->elt_size = elt_size;
    a->zero_terminated = zero_terminated;
    a->clear = clear;
    ga_reserve(a, n);
    if (a->data) ga_terminate(a);
    return a;
}

GArray *g_array_new(gboolean zero_terminated, gboolean clear, guint elt_size)
{
    return g_array_sized_new(zero_terminated, clear, elt_size, 0);
}

GArray *g_array_append_vals(GArray *a, gconstpointer data, guint n)
{
    ga_reserve(a, a->len + n);
    memcpy(a->data + (gsize)a->len * a->elt_size, data, (gsize)n * a->elt_size);
    a->len += n;
    ga_terminate(a);
    return a;
}

GArray *g_array_set_size(GArray *a, guint n)
{
    ga_reserve(a, n);
    if (n > a->len)
        memset(a->data + (gsize)a->len * a->elt_size, 0, (gsize)(n - a->len) * a->elt_size);
    a->len = n;
    if (a->data) ga_terminate(a);
    return a;
}

GArray *g_array_remove_index(GArray *a, guint i)
{
    g_return_val_if_fail(i < a->len, a);
    memmove(a->data + (gsize)i * a->elt_size, a->data + (gsize)(i + 1) * a->elt_size,
            (gsize)(a->len - i - 1) * a->elt_size);
    a->len--;
    ga_terminate(a);
    return a;
}

void g_array_sort(GArray *a, GCompareFunc f)
{
    g_msort(a->data, a->len, a->elt_size, cmp_nodata, (gpointer)f);
}

gchar *g_array_free(GArray *a, gboolean free_segment)
{
    if (!a) return NULL;
    gchar *r = a->data;
    if (free_segment) { g_free(r); r = NULL; }
    g_free(a);
    return r;
}

/* ----------------------------------------------------------------- GList */

GList *g_list_last(GList *l)
{
    if (l) while (l->next) l = l->next;
    return l;
}

GList *g_list_append(GList *l, gpointer data)
{
    GList *n = g_new0(GList, 1);
    n->data = data;
    if (!l) return n;
    GList *last = g_list_last(l);
    last->next = n;
    n->prev = last;
    return l;
}

GList *g_list_prepend(GList *l, gpointer data)
{
    GList *n = g_new0(GList, 1);
    n->data = data;
    n->next = l;
    if (l) { n->prev = l->prev; if (l->prev) l->prev->next = n; l->prev = n; }
    return n;
}

void g_list_free(GList *l)
{
    while (l) { GList *n = l->next; g_free(l); l = n; }
}

GList *g_list_nth(GList *l, guint n)
{
    while (l && n-- > 0) l = l->next;
    return l;
}

guint g_list_length(GList *l)
{
    guint n = 0;
    for (; l; l = l->next) n++;
    return n;
}

gint g_list_index(GList *l, gconstpointer data)
{
    for (gint i = 0; l; l = l->next, i++)
        if (l->data == data) return i;
    return -1;
}

GList *g_list_delete_link(GList *l, GList *link)
{
    if (!link) return l;
    if (link->prev) link->prev->next = link->next;
    if (link->next) link->next->prev = link->prev;
    if (link == l) l = link->next;
    g_free(link);
    return l;
}

GList *g_list_remove(GList *l, gconstpointer data)
{
    for (GList *it = l; it; it = it->next)
        if (it->data == data) return g_list_delete_link(l, it);
    return l;
}

GList *g_list_copy(GList *l)
{
    GList *r = NULL, *last = NULL;
    for (; l; l = l->next)
    {
        GList *n = g_new0(GList, 1);
        n->data = l->data;
        n->prev = last;
        if (last) last->next = n; else r = n;
        last = n;
    }
    return r;
}

typedef struct { GCompareDataFunc f; gpointer d; } list_cmp_ctx;

/* compare the data pointers, not the array slots */
static gint list_cmp(gconstpointer a, gconstpointer b, gpointer c)
{
    const list_cmp_ctx *x = c;
    return x->f(*(gconstpointer *)a, *(gconstpointer *)b, x->d);
}

GList *g_list_sort_with_data(GList *l, GCompareDataFunc f, gpointer user_data)
{
    guint n = g_list_length(l);
    if (n < 2) return l;
    gpointer *v = g_new(gpointer, n);
    guint i = 0;
    for (GList *it = l; it; it = it->next) v[i++] = it->data;
    list_cmp_ctx ctx = { f, user_data };
    g_msort(v, n, sizeof(gpointer), list_cmp, &ctx);
    i = 0;
    for (GList *it = l; it; it = it->next) it->data = v[i++];
    g_free(v);
    return l;
}

GList *g_list_sort(GList *l, GCompareFunc f)
{
    return g_list_sort_with_data(l, cmp_nodata, (gpointer)f);
}

/* ---------------------------------------------------------------- GQueue */

GQueue *g_queue_new(void) { return g_new0(GQueue, 1); }

void g_queue_free(GQueue *q)
{
    if (!q) return;
    g_list_free(q->head);
    g_free(q);
}

gboolean g_queue_is_empty(GQueue *q) { return q->head == NULL; }

void g_queue_push_head(GQueue *q, gpointer data)
{
    q->head = g_list_prepend(q->head, data);
    if (!q->tail) q->tail = q->head;
    q->length++;
}

gpointer g_queue_pop_head(GQueue *q)
{
    if (!q->head) return NULL;
    GList *n = q->head;
    gpointer d = n->data;
    q->head = n->next;
    if (q->head) q->head->prev = NULL; else q->tail = NULL;
    g_free(n);
    q->length--;
    return d;
}

/* ------------------------------------------------------------ GHashTable */

typedef struct hnode {
    gpointer key, value;
    guint hash;
    struct hnode *next;
} hnode;

struct _GHashTable {
    hnode **b;
    guint nb, size;
    GHashFunc hash;
    GEqualFunc eq;
    GDestroyNotify kfree, vfree;
};

guint g_direct_hash(gconstpointer v) { return (guint)(uintptr_t)v; }
gboolean g_direct_equal(gconstpointer a, gconstpointer b) { return a == b; }

GHashTable *g_hash_table_new_full(GHashFunc hash, GEqualFunc eq,
                                  GDestroyNotify key_free, GDestroyNotify value_free)
{
    GHashTable *h = g_new0(GHashTable, 1);
    h->hash = hash ? hash : g_direct_hash;
    h->eq = eq;
    h->kfree = key_free;
    h->vfree = value_free;
    h->nb = 16;
    h->b = g_new0(hnode *, h->nb);
    return h;
}

GHashTable *g_hash_table_new(GHashFunc hash, GEqualFunc eq)
{
    return g_hash_table_new_full(hash, eq, NULL, NULL);
}

static guint hmix(guint h) { h ^= h >> 16; h *= 0x45d9f3bu; h ^= h >> 16; return h; }

static hnode **hfind(GHashTable *h, gconstpointer key, guint hv)
{
    hnode **pp = &h->b[hmix(hv) & (h->nb - 1)];
    for (; *pp; pp = &(*pp)->next)
        if ((*pp)->hash == hv && (h->eq ? h->eq((*pp)->key, key) : (*pp)->key == key))
            break;
    return pp;
}

static void hresize(GHashTable *h)
{
    guint nb = h->nb * 2;
    hnode **b = g_new0(hnode *, nb);
    for (guint i = 0; i < h->nb; i++)
        for (hnode *n = h->b[i], *next; n; n = next)
        {
            next = n->next;
            guint j = hmix(n->hash) & (nb - 1);
            n->next = b[j];
            b[j] = n;
        }
    g_free(h->b);
    h->b = b;
    h->nb = nb;
}

gboolean g_hash_table_insert(GHashTable *h, gpointer key, gpointer value)
{
    guint hv = h->hash(key);
    hnode **pp = hfind(h, key, hv);
    if (*pp)
    {
        /* GLib keeps the old key and frees the new one */
        if (h->kfree) h->kfree(key);
        if (h->vfree) h->vfree((*pp)->value);
        (*pp)->value = value;
        return FALSE;
    }
    hnode *n = g_new0(hnode, 1);
    n->key = key;
    n->value = value;
    n->hash = hv;
    *pp = n;
    if (++h->size > h->nb) hresize(h);
    return TRUE;
}

gpointer g_hash_table_lookup(GHashTable *h, gconstpointer key)
{
    hnode *n = *hfind(h, key, h->hash(key));
    return n ? n->value : NULL;
}

gboolean g_hash_table_remove(GHashTable *h, gconstpointer key)
{
    hnode **pp = hfind(h, key, h->hash(key));
    hnode *n = *pp;
    if (!n) return FALSE;
    *pp = n->next;
    h->size--;
    if (h->kfree) h->kfree(n->key);
    if (h->vfree) h->vfree(n->value);
    g_free(n);
    return TRUE;
}

void g_hash_table_remove_all(GHashTable *h)
{
    for (guint i = 0; i < h->nb; i++)
    {
        for (hnode *n = h->b[i], *next; n; n = next)
        {
            next = n->next;
            if (h->kfree) h->kfree(n->key);
            if (h->vfree) h->vfree(n->value);
            g_free(n);
        }
        h->b[i] = NULL;
    }
    h->size = 0;
}

void g_hash_table_foreach(GHashTable *h, GHFunc f, gpointer user_data)
{
    for (guint i = 0; i < h->nb; i++)
        for (hnode *n = h->b[i], *next; n; n = next)
        {
            next = n->next;
            f(n->key, n->value, user_data);
        }
}

guint g_hash_table_size(GHashTable *h) { return h->size; }

static GList *hlist(GHashTable *h, gboolean keys)
{
    GList *l = NULL;
    for (guint i = 0; i < h->nb; i++)
        for (hnode *n = h->b[i]; n; n = n->next)
            l = g_list_prepend(l, keys ? n->key : n->value);
    return l;
}

GList *g_hash_table_get_keys(GHashTable *h) { return hlist(h, TRUE); }
GList *g_hash_table_get_values(GHashTable *h) { return hlist(h, FALSE); }

void g_hash_table_destroy(GHashTable *h)
{
    if (!h) return;
    g_hash_table_remove_all(h);
    g_free(h->b);
    g_free(h);
}

/* ---------------------------------------------------------------- errors */

static void set_error(GError **e, gint code, const char *fmt, ...) G_GNUC_PRINTF(3, 4);
static void set_error(GError **e, gint code, const char *fmt, ...)
{
    if (!e) return;
    GError *err = g_new0(GError, 1);
    err->code = code;
    va_list ap;
    va_start(ap, fmt);
    err->message = g_strdup_vprintf(fmt, ap);
    va_end(ap);
    if (*e) g_error_free(*e);
    *e = err;
}

void g_error_free(GError *e)
{
    if (!e) return;
    g_free(e->message);
    g_free(e);
}

void g_clear_error(GError **e)
{
    if (e && *e) { g_error_free(*e); *e = NULL; }
}

/* ------------------------------------------------- files and environment */

gboolean g_file_test(const gchar *filename, GFileTest test)
{
    struct stat st;
    if ((test & G_FILE_TEST_IS_SYMLINK) && lstat(filename, &st) == 0 && S_ISLNK(st.st_mode))
        return TRUE;
    if (stat(filename, &st) != 0) return FALSE;
    if (test & G_FILE_TEST_EXISTS) return TRUE;
    if ((test & G_FILE_TEST_IS_REGULAR) && S_ISREG(st.st_mode)) return TRUE;
    if ((test & G_FILE_TEST_IS_DIR) && S_ISDIR(st.st_mode)) return TRUE;
    if ((test & G_FILE_TEST_IS_EXECUTABLE) && (st.st_mode & 0111) && !S_ISDIR(st.st_mode))
        return TRUE;
    return FALSE;
}

gboolean g_file_get_contents(const gchar *filename, gchar **contents,
                             gsize *length, GError **error)
{
    FILE *f = fopen(filename, "rb");
    if (!f)
    {
        set_error(error, errno, "Failed to open file \"%s\": %s", filename, strerror(errno));
        return FALSE;
    }
    GString *s = g_string_new("");
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0)
        g_string_append_len(s, buf, (gssize)n);
    fclose(f);
    if (length) *length = s->len;
    *contents = g_string_free(s, FALSE);
    return TRUE;
}

gboolean g_file_set_contents(const gchar *filename, const gchar *contents,
                             gssize length, GError **error)
{
    if (length < 0) length = (gssize)strlen(contents);
    gchar *tmp = g_strconcat(filename, ".tmp", NULL);
    FILE *f = fopen(tmp, "wb");
    if (!f)
    {
        set_error(error, errno, "Failed to create file \"%s\": %s", tmp, strerror(errno));
        g_free(tmp);
        return FALSE;
    }
    gboolean ok = fwrite(contents, 1, (size_t)length, f) == (size_t)length;
    ok = (fclose(f) == 0) && ok;
    if (!ok || rename(tmp, filename) != 0)
    {
        set_error(error, errno, "Failed to write file \"%s\": %s", filename, strerror(errno));
        unlink(tmp);
        g_free(tmp);
        return FALSE;
    }
    g_free(tmp);
    return TRUE;
}

/* join elements with sep, collapsing duplicate separators at the joints */
static gchar *build_pathv(const gchar *sep, const gchar *first, va_list ap)
{
    GString *s = g_string_new("");
    size_t sl = strlen(sep);
    gboolean have = FALSE;
    for (const gchar *e = first; e; e = va_arg(ap, const gchar *))
    {
        const gchar *start = e, *end = e + strlen(e);
        if (have)
            while (sl && (size_t)(end - start) >= sl && !strncmp(start, sep, sl)) start += sl;
        if (have || *start)
        {
            /* strip trailing separators except on the first element alone */
            const gchar *stop = end;
            while (sl && (size_t)(stop - start) > sl && !strncmp(stop - sl, sep, sl)) stop -= sl;
            if (!*start && have) continue;
            if (have && s->len && !g_str_has_suffix(s->str, sep)) g_string_append(s, sep);
            g_string_append_len(s, start, stop - start);
            have = TRUE;
        }
    }
    return g_string_free(s, FALSE);
}

gchar *g_build_path(const gchar *sep, const gchar *first, ...)
{
    va_list ap;
    va_start(ap, first);
    gchar *r = build_pathv(sep, first, ap);
    va_end(ap);
    return r;
}

gchar *g_build_filename(const gchar *first, ...)
{
    va_list ap;
    va_start(ap, first);
    gchar *r = build_pathv(G_DIR_SEPARATOR_S, first, ap);
    va_end(ap);
    return r;
}

gchar *g_path_get_dirname(const gchar *path)
{
    const char *slash = strrchr(path, '/');
    if (!slash) return g_strdup(".");
    while (slash > path && slash[-1] == '/') slash--;
    if (slash == path) return g_strdup("/");
    return g_strndup(path, (gsize)(slash - path));
}

const gchar *g_get_home_dir(void)
{
    const char *h = getenv("HOME");
    return (h && *h) ? h : "/";
}

const gchar *g_get_user_config_dir(void)
{
    static gchar *dir = NULL;
    if (!dir)
    {
        const char *x = getenv("XDG_CONFIG_HOME");
        dir = (x && *x) ? g_strdup(x) : g_build_filename(g_get_home_dir(), ".config", NULL);
    }
    return dir;
}

const gchar *g_get_user_special_dir(GUserDirectory d)
{
    (void)d;
    return g_get_home_dir();
}

const gchar *const *g_get_language_names(void)
{
    /* the web build ships the English files only */
    static const gchar *const names[] = { "C", NULL };
    return names;
}

gboolean g_setenv(const gchar *name, const gchar *value, gboolean overwrite)
{
    return setenv(name, value, overwrite) == 0;
}

gint64 g_get_monotonic_time(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (gint64)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

/* ------------------------------------------------------------- GKeyFile */

typedef struct kline {
    gchar *key;   /* NULL: comment or blank line kept verbatim in value */
    gchar *value;
    struct kline *next;
} kline;

typedef struct kgroup {
    gchar *name;  /* NULL: lines before the first group */
    kline *lines;
    struct kgroup *next;
} kgroup;

struct _GKeyFile {
    kgroup *groups;
};

GKeyFile *g_key_file_new(void) { return g_new0(GKeyFile, 1); }

static void kf_clear(GKeyFile *kf)
{
    for (kgroup *g = kf->groups, *gn; g; g = gn)
    {
        gn = g->next;
        for (kline *l = g->lines, *ln; l; l = ln)
        {
            ln = l->next;
            g_free(l->key);
            g_free(l->value);
            g_free(l);
        }
        g_free(g->name);
        g_free(g);
    }
    kf->groups = NULL;
}

void g_key_file_free(GKeyFile *kf)
{
    if (!kf) return;
    kf_clear(kf);
    g_free(kf);
}

static kgroup *kf_group(GKeyFile *kf, const gchar *name, gboolean create)
{
    kgroup **pp = &kf->groups;
    for (; *pp; pp = &(*pp)->next)
        if (!g_strcmp0((*pp)->name, name)) return *pp;
    if (!create) return NULL;
    *pp = g_new0(kgroup, 1);
    (*pp)->name = g_strdup(name);
    return *pp;
}

static void kf_append(kgroup *g, gchar *key, gchar *value)
{
    kline **pp = &g->lines;
    while (*pp) pp = &(*pp)->next;
    *pp = g_new0(kline, 1);
    (*pp)->key = key;
    (*pp)->value = value;
}

gboolean g_key_file_load_from_data(GKeyFile *kf, const gchar *data, gsize len,
                                   GKeyFileFlags flags, GError **error)
{
    (void)error;
    kf_clear(kf);
    gchar *copy = g_strndup(data, len);
    gchar **lines = g_strsplit(copy, "\n", -1);
    g_free(copy);
    kgroup *g = NULL;
    guint n = g_strv_length(lines);
    /* a trailing newline yields an empty last element: drop it */
    if (n && !*lines[n - 1]) n--;
    for (guint i = 0; i < n; i++)
    {
        gchar *l = lines[i];
        size_t ll = strlen(l);
        if (ll && l[ll - 1] == '\r') l[--ll] = 0;
        gchar *t = g_strchug(g_strdup(l));
        if (*t == '[' && strchr(t, ']'))
        {
            *strchr(t, ']') = 0;
            g = kf_group(kf, t + 1, TRUE);
        }
        else if (*t == '#' || *t == 0)
        {
            if (flags & G_KEY_FILE_KEEP_COMMENTS)
                kf_append(g ? g : kf_group(kf, NULL, TRUE), NULL, g_strdup(l));
        }
        else if (strchr(t, '=') && g)
        {
            gchar *eq = strchr(t, '=');
            gchar *key = g_strchomp(g_strndup(t, (gsize)(eq - t)));
            gchar *val = g_strchug(g_strdup(eq + 1));
            kf_append(g, key, val);
        }
        g_free(t);
    }
    g_strfreev(lines);
    return TRUE;
}

gboolean g_key_file_load_from_file(GKeyFile *kf, const gchar *file,
                                   GKeyFileFlags flags, GError **error)
{
    gchar *data;
    gsize len;
    if (!g_file_get_contents(file, &data, &len, error)) return FALSE;
    gboolean r = g_key_file_load_from_data(kf, data, len, flags, error);
    g_free(data);
    return r;
}

gboolean g_key_file_save_to_file(GKeyFile *kf, const gchar *file, GError **error)
{
    GString *s = g_string_new("");
    for (kgroup *g = kf->groups; g; g = g->next)
    {
        if (g->name) g_string_append_printf(s, "[%s]\n", g->name);
        for (kline *l = g->lines; l; l = l->next)
        {
            if (l->key) g_string_append_printf(s, "%s=%s\n", l->key, l->value);
            else g_string_append_printf(s, "%s\n", l->value);
        }
    }
    gboolean r = g_file_set_contents(file, s->str, (gssize)s->len, error);
    g_string_free(s, TRUE);
    return r;
}

static const gchar *kf_raw(GKeyFile *kf, const gchar *group, const gchar *key, GError **error)
{
    kgroup *g = kf_group(kf, group, FALSE);
    if (!g)
    {
        set_error(error, 1, "Key file does not have group \"%s\"", group);
        return NULL;
    }
    for (kline *l = g->lines; l; l = l->next)
        if (l->key && !strcmp(l->key, key)) return l->value;
    set_error(error, 2, "Key file does not have key \"%s\" in group \"%s\"", key, group);
    return NULL;
}

void g_key_file_set_value(GKeyFile *kf, const gchar *group, const gchar *key, const gchar *value)
{
    kgroup *g = kf_group(kf, group, TRUE);
    for (kline *l = g->lines; l; l = l->next)
        if (l->key && !strcmp(l->key, key))
        {
            g_free(l->value);
            l->value = g_strdup(value);
            return;
        }
    kf_append(g, g_strdup(key), g_strdup(value));
}

/* GLib's escaping: \s \n \t \r \\ (plus leading space as \s) */
static gchar *kf_unescape(const gchar *v, gboolean list)
{
    GString *s = g_string_new("");
    for (const gchar *p = v; *p; p++)
    {
        if (*p == '\\' && p[1])
        {
            p++;
            switch (*p)
            {
            case 's': g_string_append_c(s, ' '); break;
            case 'n': g_string_append_c(s, '\n'); break;
            case 't': g_string_append_c(s, '\t'); break;
            case 'r': g_string_append_c(s, '\r'); break;
            case ';': if (list) { g_string_append(s, "\\;"); break; } /* fall through */
            default: g_string_append_c(s, *p);
            }
        }
        else g_string_append_c(s, *p);
    }
    return g_string_free(s, FALSE);
}

static gchar *kf_escape(const gchar *v, gboolean list)
{
    GString *s = g_string_new("");
    for (const gchar *p = v; *p; p++)
    {
        switch (*p)
        {
        case ' ': g_string_append(s, p == v ? "\\s" : " "); break;
        case '\n': g_string_append(s, "\\n"); break;
        case '\t': g_string_append(s, "\\t"); break;
        case '\r': g_string_append(s, "\\r"); break;
        case '\\': g_string_append(s, "\\\\"); break;
        case ';': g_string_append(s, list ? "\\;" : ";"); break;
        default: g_string_append_c(s, *p);
        }
    }
    return g_string_free(s, FALSE);
}

gchar *g_key_file_get_string(GKeyFile *kf, const gchar *group, const gchar *key, GError **error)
{
    const gchar *v = kf_raw(kf, group, key, error);
    return v ? kf_unescape(v, FALSE) : NULL;
}

gint g_key_file_get_integer(GKeyFile *kf, const gchar *group, const gchar *key, GError **error)
{
    const gchar *v = kf_raw(kf, group, key, error);
    if (!v) return 0;
    char *end;
    errno = 0;
    long r = strtol(v, &end, 10);
    if (end == v || *end || errno)
    {
        set_error(error, 3, "Value \"%s\" cannot be interpreted as a number.", v);
        return 0;
    }
    return (gint)r;
}

gboolean g_key_file_get_boolean(GKeyFile *kf, const gchar *group, const gchar *key, GError **error)
{
    const gchar *v = kf_raw(kf, group, key, error);
    if (!v) return FALSE;
    if (!strcmp(v, "true") || !strcmp(v, "1")) return TRUE;
    if (!strcmp(v, "false") || !strcmp(v, "0")) return FALSE;
    set_error(error, 3, "Value \"%s\" cannot be interpreted as a boolean.", v);
    return FALSE;
}

gchar **g_key_file_get_string_list(GKeyFile *kf, const gchar *group, const gchar *key,
                                   gsize *length, GError **error)
{
    const gchar *v = kf_raw(kf, group, key, error);
    if (!v) { if (length) *length = 0; return NULL; }
    GPtrArray *a = g_ptr_array_new();
    GString *cur = g_string_new("");
    for (const gchar *p = v; *p; p++)
    {
        if (*p == '\\' && p[1]) { g_string_append_c(cur, *p); g_string_append_c(cur, *++p); }
        else if (*p == ';')
        {
            g_ptr_array_add(a, kf_unescape(cur->str, FALSE));
            g_string_truncate(cur, 0);
        }
        else g_string_append_c(cur, *p);
    }
    if (cur->len) g_ptr_array_add(a, kf_unescape(cur->str, FALSE));
    g_string_free(cur, TRUE);
    if (length) *length = a->len;
    g_ptr_array_add(a, NULL);
    return (gchar **)g_ptr_array_free(a, FALSE);
}

void g_key_file_set_string(GKeyFile *kf, const gchar *group, const gchar *key, const gchar *value)
{
    gchar *e = kf_escape(value, FALSE);
    g_key_file_set_value(kf, group, key, e);
    g_free(e);
}

void g_key_file_set_integer(GKeyFile *kf, const gchar *group, const gchar *key, gint value)
{
    gchar buf[32];
    snprintf(buf, sizeof buf, "%d", value);
    g_key_file_set_value(kf, group, key, buf);
}

void g_key_file_set_boolean(GKeyFile *kf, const gchar *group, const gchar *key, gboolean value)
{
    g_key_file_set_value(kf, group, key, value ? "true" : "false");
}

void g_key_file_set_string_list(GKeyFile *kf, const gchar *group, const gchar *key,
                                const gchar *const list[], gsize length)
{
    GString *s = g_string_new("");
    for (gsize i = 0; i < length && list[i]; i++)
    {
        gchar *e = kf_escape(list[i], TRUE);
        g_string_append(s, e);
        g_string_append_c(s, ';');
        g_free(e);
    }
    g_key_file_set_value(kf, group, key, s->str);
    g_string_free(s, TRUE);
}

/* ------------------------------------------------------- GOptionContext */

struct _GOptionContext {
    const GOptionEntry *entries;
};

GOptionContext *g_option_context_new(const gchar *parameter_string)
{
    (void)parameter_string;
    return g_new0(GOptionContext, 1);
}

void g_option_context_add_main_entries(GOptionContext *c, const GOptionEntry *entries,
                                       const gchar *translation_domain)
{
    (void)translation_domain;
    c->entries = entries;
}

void g_option_context_free(GOptionContext *c) { g_free(c); }

static gboolean opt_set(const GOptionEntry *e, const char *val, GError **error)
{
    switch (e->arg)
    {
    case G_OPTION_ARG_NONE: *(gboolean *)e->arg_data = TRUE; return TRUE;
    case G_OPTION_ARG_INT: *(gint *)e->arg_data = atoi(val); return TRUE;
    case G_OPTION_ARG_STRING:
    case G_OPTION_ARG_FILENAME:
        g_free(*(gchar **)e->arg_data);
        *(gchar **)e->arg_data = g_strdup(val);
        return TRUE;
    }
    set_error(error, 1, "unsupported option type");
    return FALSE;
}

/* --long, --long=value, --long value, -x, -x value, -xvalue */
gboolean g_option_context_parse(GOptionContext *c, gint *argc, gchar ***argv, GError **error)
{
    gchar **av = *argv;
    gint out = 1;
    for (gint i = 1; i < *argc; i++)
    {
        const char *a = av[i];
        const GOptionEntry *hit = NULL;
        const char *val = NULL;
        if (a[0] == '-' && a[1] == '-' && a[2])
        {
            const char *eq = strchr(a + 2, '=');
            size_t n = eq ? (size_t)(eq - a - 2) : strlen(a + 2);
            for (const GOptionEntry *e = c->entries; e && e->long_name; e++)
                if (strlen(e->long_name) == n && !strncmp(e->long_name, a + 2, n)) hit = e;
            if (eq) val = eq + 1;
        }
        else if (a[0] == '-' && a[1] && a[1] != '-')
        {
            for (const GOptionEntry *e = c->entries; e && e->long_name; e++)
                if (e->short_name == a[1]) hit = e;
            if (a[2]) val = a + 2;
        }
        else
        {
            av[out++] = av[i];
            continue;
        }
        if (!hit)
        {
            set_error(error, 1, "Unknown option %s", a);
            return FALSE;
        }
        if (hit->arg != G_OPTION_ARG_NONE && !val)
        {
            if (i + 1 >= *argc)
            {
                set_error(error, 1, "Missing argument for %s", a);
                return FALSE;
            }
            val = av[++i];
        }
        if (!opt_set(hit, val, error)) return FALSE;
    }
    *argc = out;
    av[out] = NULL;
    return TRUE;
}

/* ------------------------------------------------------------------ i18n */

const gchar *g_dpgettext2(const gchar *domain, const gchar *context, const gchar *msgid)
{
    gchar *full = g_strconcat(context, "\004", msgid, NULL);
    const gchar *t = domain ? dgettext(domain, full) : gettext(full);
    gboolean untranslated = (t == full) || !strcmp(t, full);
    g_free(full);
    return untranslated ? msgid : t;
}
