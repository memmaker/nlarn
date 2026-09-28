/* NLarn web port: GLib's gstdio.h (see ../glib.h). */
#ifndef NLARN_PORT_GSTDIO_H
#define NLARN_PORT_GSTDIO_H
#include <glib.h>
#include <sys/stat.h>
#include <unistd.h>
#define g_mkdir(p, m) mkdir(p, m)
#define g_unlink(p) unlink(p)
#endif
