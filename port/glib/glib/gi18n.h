/* NLarn web port: GLib's gi18n.h on libintl (see ../glib.h). */
#ifndef NLARN_PORT_GI18N_H
#define NLARN_PORT_GI18N_H
#include <glib.h>
#include <libintl.h>
#define _(s) gettext(s)
#define N_(s) (s)
#define C_(ctx, s) g_dpgettext2(NULL, ctx, s)
#define NC_(ctx, s) (s)
#endif
