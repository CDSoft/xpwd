#include <glob.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <X11/Xlib.h>

const char *process_blacklist[] = {
    "bash-language-server",
    "clangd",
    "clangd.main",
    "dot-language-server",
    "elm-language-server",
    "lua-language-server",
    "nimlsp",
    "pasls",
    "pyright-langserver",
    "zls",
    "xclip",
    NULL
};

const char *path_blacklist[] = {
    "/bin",
    "/lib",
    "/lib64",
    "/usr",
    NULL
};

#define XA_STRING   (XInternAtom(dpy, "STRING", 0))
#define XA_CARDINAL (XInternAtom(dpy, "CARDINAL", 0))
#define XA_WM_STATE (XInternAtom(dpy, "WM_STATE", 0))

#define NAME_SIZE 32
#define LINE_SIZE 200
#define PATH_SIZE 1024

typedef struct processes_s *processes_t;
struct processes_s
{
    struct proc_s {
        long pid;
        long ppid;
        char name[NAME_SIZE];
        char path[PATH_SIZE];
    } *ps;
    size_t n;
};

bool debug = false;

static void help(void)
{
    printf("usage: xpwd [-d]\n"
           "\n"
           "xpwd guesses and prints the current directory of the process in the current window\n"
           "\n"
           "Options:\n"
           "  -h        show this help\n"
           "  -d        show debug information\n");
    exit(EXIT_SUCCESS);
}

static void error(const char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}

static void trace(const char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    if (debug)
    {
        vfprintf(stderr, msg, ap);
    }
    va_end(ap);
}

static int ppidCmp(const void *p1, const void *p2)
{
    return ((struct proc_s *)p1)->ppid - ((struct proc_s *)p2)->ppid;
}

static Window focusedWindow(Display *dpy)
{
    Atom type;
    Window focuswin, root, *children;
    int format, status;
    unsigned long nitems, after;
    unsigned int nchildren;
    unsigned char *data;

    XGetInputFocus (dpy, &focuswin, (int[1]){});
    root = XDefaultRootWindow(dpy);
    if(root == focuswin) { return None; }

    do
    {
        status = XGetWindowProperty(
                    dpy, focuswin, XA_WM_STATE, 0, 1024, 0,
                    XA_WM_STATE, &type, &format, &nitems, &after, &data
                 );
        if (status == Success)
        {
            if (data)
            {
                XFree(data);
                trace("Window ID = %lu\n", focuswin);
                return focuswin;
            }
        }
        else
        {
            return 0;
        }
        XQueryTree(dpy, focuswin, &root, &focuswin, &children, &nchildren);
        trace("Current window does not have WM_STATE, getting parent\n");
    } while(focuswin != root);

    return 0;
}

static long windowPid(Display *dpy, Window focuswin)
{
    Atom nameAtom = XInternAtom(dpy, "_NET_WM_PID", 1);
    Atom type;
    int format, status;
    long pid = -1;
    unsigned long nitems, after;
    unsigned char *data;

    status = XGetWindowProperty(
                dpy, focuswin, nameAtom, 0, 1024, 0,
                XA_CARDINAL, &type, &format, &nitems, &after, &data
             );
    if (status == Success && data) {
        pid = *((long*)data);
        XFree(data);
        trace("_NET_WM_PID = %lu\n", pid);
    }
    else
    {
        trace("%s", "_NET_WM_PID not found\n");
    }
    return pid;
}

static bool process_blacklisted(const char *name)
{
    const size_t n = strlen(name);
    for (int i = 0; process_blacklist[i] != NULL; i++)
    {
        if (strncmp(name, process_blacklist[i], n) == 0) {
            return true;
        }
    }
    return false;
}

static processes_t getProcesses(void)
{
    processes_t p = NULL;
    glob_t globbuf;
    unsigned int i, j, k;
    char line[LINE_SIZE+1] = {0};

    glob("/proc/[0-9]*", GLOB_NOSORT, NULL, &globbuf);
    p = malloc(sizeof(struct processes_s));
    p->ps = malloc(globbuf.gl_pathc * sizeof(struct proc_s));

    trace("Found %zu processes\n", globbuf.gl_pathc);
    for (i = j = 0; i < globbuf.gl_pathc; i++) {
        char name[NAME_SIZE];
        (void)globbuf.gl_pathv[globbuf.gl_pathc - i - 1];
        snprintf(name, sizeof(name), "%s%s", globbuf.gl_pathv[globbuf.gl_pathc - i - 1], "/stat");
        FILE *tn = fopen(name, "r");
        if (tn == NULL) { continue; }
        size_t n = fread(line, LINE_SIZE, 1, tn);
        if (n > 0)
        {
            p->ps[j].pid = atoi(strtok(line, " "));
            k = snprintf(p->ps[j].name, NAME_SIZE, "%s", strtok(NULL, " ") + 1);
            p->ps[j].name[k - 1] = 0;
            strtok(NULL, " "); // discard process state
            p->ps[j].ppid = atoi(strtok(NULL, " "));
            trace("\t%-20s\tpid=%6ld\tppid=%6ld\n", p->ps[j].name, p->ps[j].pid, p->ps[j].ppid);
            if (!process_blacklisted(p->ps[j].name))
            {
                j++;
            }
        }
        fclose(tn);
    }
    p->n = j;
    globfree(&globbuf);
    return p;
}

static void freeProcesses(processes_t p)
{
    free(p->ps);
    free(p);
}

static bool path_blacklisted(const char *name)
{
    if (name[0] == '/' && name[1] == '\0')
    {
        return true;
    }
    for (int i = 0; path_blacklist[i] != NULL; i++)
    {
        const size_t n = strlen(path_blacklist[i]);
        if (strncmp(name, path_blacklist[i], n) == 0) {
            return true;
        }
    }
    return false;
}

static bool readPath(struct proc_s *proc)
{
    char buf[255];
    char path[64];
    ssize_t len;

    snprintf(path, sizeof(path), "/proc/%ld/cwd", proc->pid);
    if ((len = readlink(path, buf, sizeof(buf))) != -1)
    {
        buf[len] = '\0';
    }
    if (len <= 0)
    {
        trace("Error readlink %s\n", path);
        return false;
    }
    if (path_blacklisted(buf))
    {
        return false;
    }
    trace("Read %s\n", path);
    if (access(buf, F_OK))
    {
        return false;
    }
    fprintf(stdout, "%s\n", buf);
    return true;
}

static bool cwdOfDeepestChild(processes_t p, long pid)
{
    struct proc_s key = { .pid = pid, .ppid = pid},
                  *res = NULL, *lastRes = NULL;
    do
    {
        if(res)
        {
            lastRes = res;
            key.ppid = res->pid;
        }
        res = (struct proc_s *)bsearch(&key, p->ps, p->n, sizeof(struct proc_s), ppidCmp);
    } while(res);

    if (!lastRes)
    {
        if (readPath(&key))
        {
            return true;
        }
    }

    for (int i = 0; lastRes != p->ps && (lastRes - i)->ppid == lastRes->ppid; ++i)
    {
        if (readPath((lastRes - i)))
        {
            return true;
        }
    }
    for (int i = 1; lastRes != p->ps + p->n && (lastRes + i)->ppid == lastRes->ppid; ++i)
    {
        if (readPath((lastRes + i)))
        {
            return true;
        }
    }
    return false;
}

int getHomeDirectory()
{
    trace("getenv $HOME...\n");
    fprintf(stdout, "%s\n", getenv("HOME"));
    return EXIT_FAILURE;
}

int main(int argc, const char *argv[])
{
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0) { help(); }
        else if (strcmp(argv[i], "-d") == 0) { debug = true; }
        else { error("%s: invalid option", argv[i]); }
    }

    Display *dpy = XOpenDisplay(NULL);      if (dpy == NULL) { error("Can not connect to X server"); }
    Window w = focusedWindow(dpy);          if (w == None) { return getHomeDirectory(); }
    long pid = windowPid(dpy, w);           if (pid == -1) { return getHomeDirectory(); }
    processes_t p = getProcesses();         if (p == NULL) { return getHomeDirectory(); }
    qsort(p->ps, p->n, sizeof(struct proc_s), ppidCmp);
    if (!cwdOfDeepestChild(p, pid)) { return getHomeDirectory(); }
    freeProcesses(p);
    return EXIT_SUCCESS;
}
