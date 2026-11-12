#define _XOPEN_SOURCE 700
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "tinytemplate.h"

#define ENTRIES_DIR "notes/essays"
#define ASSETS_DIR "assets"
#define TAGS_FILE "tags"
#define ROBOTS_FILE "robots.txt"
#define TAGS_PATH ASSETS_DIR "/" TAGS_FILE
#define OUT_DIR "build/dist"
#define OUT_ARCHIVE_DIR "archive"
#define ENTRY_MD_FILE "index.md"
#define TEMPLATES_DIR "templates"
#define MAX_WORDS 65536
#define MAX_WORD_LEN 128

typedef struct {
    char* filename;
    char* stem;
    char* slug;
    char* title;
    char* iso_date;     // published: last git edit
    char* created_iso;  // created: first git commit of the file
    char date_ymd[11];  // published date, YYYY-MM-DD
    char year[5];       // year, month and day of the created date, used in the URL
    char month[3];
    char day[3];
    char* content;
    char** tags;
    size_t tags_count;
    char* out_rel;  // archive/YYYY/MM/DD/slug/index.html
    char* url;      // /archive/YYYY/MM/DD/slug/
} Entry;

typedef struct {
    char** items;
    size_t count;
    size_t cap;
} StrList;

static void die(const char* msg) {
    perror(msg);
    exit(1);
}

static void* xmalloc(size_t n) {
    void* p = malloc(n);
    if (!p) die("malloc");
    return p;
}

static void* xrealloc(void* p, size_t n) {
    void* q = realloc(p, n);
    if (!q) die("realloc");
    return q;
}

static char* xstrdup(const char* s) {
    char* p = strdup(s);
    if (!p) die("strdup");
    return p;
}

static bool ends_with(const char* s, const char* suf) {
    size_t ls = strlen(s), lsu = strlen(suf);
    if (lsu > ls) return false;
    return strcmp(s + ls - lsu, suf) == 0;
}

static void strlist_init(StrList* l) {
    l->items = NULL;
    l->count = 0;
    l->cap = 0;
}

static bool strlist_contains(const StrList* l, const char* s) {
    for (size_t i = 0; i < l->count; i++)
        if (strcmp(l->items[i], s) == 0) return true;
    return false;
}

static void strlist_add_unique(StrList* l, const char* s) {
    if (strlist_contains(l, s)) return;
    if (l->count == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 32;
        l->items = xrealloc(l->items, l->cap * sizeof(char*));
    }
    l->items[l->count++] = xstrdup(s);
}

static int cmp_strptr(const void* a, const void* b) {
    const char* sa = *(const char* const*)a;
    const char* sb = *(const char* const*)b;
    return strcmp(sa, sb);
}

static void strlist_sort(StrList* l) {
    if (l->count > 1) qsort(l->items, l->count, sizeof(char*), cmp_strptr);
}

static bool mkdir_p(const char* path) {
    char tmp[PATH_MAX];
    size_t len = strlen(path);
    if (len == 0 || len >= sizeof(tmp)) return false;
    memcpy(tmp, path, len + 1);
    if (tmp[len - 1] == '/') tmp[len - 1] = 0;
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return false;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return false;
    return true;
}

static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    char* buf = xmalloc((size_t)sz + 1);
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

static bool write_file(const char* path, const char* content) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    size_t len = strlen(content);
    bool ok = fwrite(content, 1, len, f) == len;
    if (fclose(f) != 0) ok = false;
    return ok;
}

static bool copy_file(const char* src, const char* dst) {
    FILE* in = fopen(src, "rb");
    if (!in) return false;
    FILE* out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return false;
    }
    char buf[8192];
    size_t n;
    bool ok = true;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            ok = false;
            break;
        }
    }
    if (ferror(in)) ok = false;
    fclose(in);
    if (fclose(out) != 0) ok = false;
    return ok;
}

// Copies every regular file in ASSETS_DIR to OUT_DIR/ASSETS_DIR, except the
// tags list, which is generator input rather than site content.
static bool copy_assets(void) {
    if (!mkdir_p(OUT_DIR "/" ASSETS_DIR)) return false;
    DIR* dir = opendir(ASSETS_DIR);
    if (!dir) return false;
    bool ok = true;
    struct dirent* de;
    while (ok && (de = readdir(dir)) != NULL) {
        if (de->d_name[0] == '.') continue;
        if (strcmp(de->d_name, TAGS_FILE) == 0) continue;
        if (strcmp(de->d_name, ROBOTS_FILE) == 0) {
            char src[PATH_MAX], dst[PATH_MAX];
            snprintf(src, sizeof(src), "%s/%s", ASSETS_DIR, de->d_name);
            snprintf(dst, sizeof(dst), "%s/%s", OUT_DIR, de->d_name);
            struct stat st;
            if (stat(src, &st) != 0 || !S_ISREG(st.st_mode)) continue;
            if (!copy_file(src, dst)) {
                fprintf(stderr, "Failed to copy %s to %s\n", src, dst);
                ok = false;
            }
        };
        char src[PATH_MAX], dst[PATH_MAX];
        snprintf(src, sizeof(src), "%s/%s", ASSETS_DIR, de->d_name);
        snprintf(dst, sizeof(dst), "%s/%s/%s", OUT_DIR, ASSETS_DIR, de->d_name);
        struct stat st;
        if (stat(src, &st) != 0 || !S_ISREG(st.st_mode)) continue;
        if (!copy_file(src, dst)) {
            fprintf(stderr, "Failed to copy %s to %s\n", src, dst);
            ok = false;
        }
    }
    closedir(dir);
    return ok;
}

static char* path_join2(const char* a, const char* b) {
    size_t la = strlen(a), lb = strlen(b);
    bool slash = la > 0 && a[la - 1] == '/';
    char* r = xmalloc(la + lb + 2);
    if (slash)
        sprintf(r, "%s%s", a, b);
    else
        sprintf(r, "%s/%s", a, b);
    return r;
}

static char* run_command_capture(const char* cmd) {
    FILE* p = popen(cmd, "r");
    if (!p) return NULL;
    char buf[4096];
    size_t cap = 256, len = 0;
    char* out = xmalloc(cap);
    out[0] = 0;
    while (fgets(buf, sizeof(buf), p)) {
        size_t bl = strlen(buf);
        if (len + bl + 1 > cap) {
            while (len + bl + 1 > cap) cap *= 2;
            out = xrealloc(out, cap);
        }
        memcpy(out + len, buf, bl);
        len += bl;
        out[len] = 0;
    }
    int st = pclose(p);
    (void)st;
    // trim trailing whitespace/newlines
    while (len > 0 && (out[len - 1] == '\n' || out[len - 1] == '\r' || out[len - 1] == ' ' || out[len - 1] == '\t')) {
        out[--len] = 0;
    }
    return out;
}

static char* get_last_edit_iso(const char* filepath_in_repo) {
    // filepath_in_repo relative to notes repo, e.g. essays/foo.md
    char cmd[PATH_MAX * 2];
    snprintf(cmd, sizeof(cmd), "git -C notes log -1 --format=%%aI -- \"%s\"", filepath_in_repo);
    char* out = run_command_capture(cmd);
    if (!out || out[0] == '\0') {
        if (out) free(out);
        return xstrdup("1970-01-01T00:00:00Z");
    }
    return out;
}

static char* get_created_iso(const char* filepath_in_repo) {
    // Commits that added the file, following renames, newest first; the last
    // line is the commit that first created it.
    char cmd[PATH_MAX * 2];
    snprintf(cmd, sizeof(cmd), "git -C notes log --follow --diff-filter=A --format=%%aI -- \"%s\"", filepath_in_repo);
    char* out = run_command_capture(cmd);
    if (!out || out[0] == '\0') {
        if (out) free(out);
        return xstrdup("1970-01-01T00:00:00Z");
    }
    char* last_line = strrchr(out, '\n');
    if (!last_line) return out;
    char* created = xstrdup(last_line + 1);
    free(out);
    return created;
}

static void derive_date_parts(Entry* e) {
    // iso like 2026-11-03T11:40:00+08:00 or ...Z
    memset(e->date_ymd, 0, sizeof(e->date_ymd));
    if (strlen(e->iso_date) >= 10) {
        memcpy(e->date_ymd, e->iso_date, 10);
        e->date_ymd[10] = 0;
    } else {
        strcpy(e->date_ymd, "1970-01-01");
    }
    if (strlen(e->created_iso) >= 10) {
        memcpy(e->year, e->created_iso, 4);
        e->year[4] = 0;
        memcpy(e->month, e->created_iso + 5, 2);
        e->month[2] = 0;
        memcpy(e->day, e->created_iso + 8, 2);
        e->day[2] = 0;
    } else {
        strcpy(e->year, "1970");
        strcpy(e->month, "01");
        strcpy(e->day, "01");
    }
}

static char* make_slug_from_stem(const char* stem) {
    size_t n = strlen(stem);
    char* s = xmalloc(n + 1);
    size_t j = 0;
    int last_dash = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)stem[i];
        if (isalnum(c)) {
            s[j++] = (char)tolower(c);
            last_dash = 0;
        } else if (c == '-' || c == '_' || isspace(c)) {
            if (j > 0 && !last_dash) {
                s[j++] = '-';
                last_dash = 1;
            }
        } else {
            if (j > 0 && !last_dash) {
                s[j++] = '-';
                last_dash = 1;
            }
        }
    }
    while (j > 0 && s[j - 1] == '-') j--;
    if (j == 0) {
        strcpy(s, "entry");
        return s;
    }
    s[j] = 0;
    return s;
}

static char* make_title_from_stem(const char* stem) {
    size_t n = strlen(stem);
    char* t = xmalloc(n + 1);
    int cap = 1;
    size_t j = 0;
    for (size_t i = 0; i < n; i++) {
        char c = stem[i];
        if (c == '-' || c == '_') {
            if (j > 0 && t[j - 1] != ' ') t[j++] = ' ';
            cap = 1;
        } else {
            if (cap && isalpha((unsigned char)c)) {
                t[j++] = (char)toupper((unsigned char)c);
                cap = 0;
            } else {
                t[j++] = c;
                if (!isspace((unsigned char)c)) cap = 0;
            }
        }
    }
    t[j] = 0;
    // trim
    while (j > 0 && t[j - 1] == ' ') t[--j] = 0;
    return t;
}

static void add_words_from_text(StrList* words, const char* text) {
    char buf[MAX_WORD_LEN];
    size_t bi = 0;
    for (const unsigned char* p = (const unsigned char*)text;; p++) {
        unsigned char c = *p;
        if (isalnum(c)) {
            if (bi + 1 < sizeof(buf)) buf[bi++] = (char)tolower(c);
        } else {
            if (bi > 0) {
                buf[bi] = 0;
                strlist_add_unique(words, buf);
                bi = 0;
            }
            if (c == 0) break;
        }
    }
}

static void load_asset_tags(StrList* words) {
    char* content = read_file(TAGS_PATH);
    if (!content) return;
    char buf[MAX_WORD_LEN];
    size_t bi = 0;
    for (const unsigned char* p = (const unsigned char*)content;; p++) {
        unsigned char c = *p;
        if (c && !isspace(c)) {
            if (bi + 1 < sizeof(buf)) buf[bi++] = (char)tolower(c);
        } else {
            if (bi > 0) {
                buf[bi] = 0;
                strlist_add_unique(words, buf);
                bi = 0;
            }
            if (!c) break;
        }
    }
    free(content);
}

static char* html_escape(const char* s) {
    size_t extra = 0;
    for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
        switch (*p) {
            case '&':
                extra += 4;
                break;  // &amp;
            case '<':
                extra += 3;
                break;  // &lt;
            case '>':
                extra += 3;
                break;  // &gt;
            case '"':
                extra += 5;
                break;  // &quot;
            default:
                break;
        }
    }
    char* o = xmalloc(strlen(s) + extra + 1);
    char* d = o;
    for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
        switch (*p) {
            case '&':
                memcpy(d, "&amp;", 5);
                d += 5;
                break;
            case '<':
                memcpy(d, "&lt;", 4);
                d += 4;
                break;
            case '>':
                memcpy(d, "&gt;", 4);
                d += 4;
                break;
            case '"':
                memcpy(d, "&quot;", 6);
                d += 6;
                break;
            default:
                *d++ = (char)*p;
                break;
        }
    }
    *d = 0;
    return o;
}

typedef struct {
    char* src;
    tinytemplate_instr_t* program;
} Template;

typedef struct {
    FILE* out;
    bool write_failed;
    void* view;
} RenderCtx;

static bool template_load(Template* t, const char* path) {
    t->program = NULL;
    t->src = read_file(path);
    if (!t->src) {
        fprintf(stderr, "Failed to read template %s\n", path);
        return false;
    }
    char errmsg[256] = "";
    size_t max_instr = 128;
    for (;;) {
        t->program = xrealloc(t->program, max_instr * sizeof(*t->program));
        tinytemplate_status_t status =
            tinytemplate_compile(t->src, strlen(t->src), t->program, max_instr, NULL, errmsg, sizeof(errmsg));
        if (status == TINYTEMPLATE_STATUS_DONE) return true;
        if (status != TINYTEMPLATE_STATUS_EMEMORY) {
            fprintf(stderr, "Failed to compile template %s: %s\n", path, errmsg);
            return false;
        }
        max_instr *= 2;
    }
}

static void render_write(void* userp, const char* str, size_t len) {
    RenderCtx* ctx = userp;
    if (fwrite(str, 1, len, ctx->out) != len) ctx->write_failed = true;
}

static bool template_render(const Template* t, const char* out_path, tinytemplate_getter_t params, void* view) {
    RenderCtx ctx = {.out = fopen(out_path, "wb"), .write_failed = false, .view = view};
    if (!ctx.out) {
        fprintf(stderr, "Failed to open %s: %s\n", out_path, strerror(errno));
        return false;
    }
    char errmsg[256] = "";
    bool ok = tinytemplate_eval(t->src, t->program, &ctx, params, render_write, errmsg, sizeof(errmsg)) ==
              TINYTEMPLATE_STATUS_DONE;
    if (!ok) fprintf(stderr, "Failed to render %s: %s\n", out_path, errmsg);
    if (fclose(ctx.out) != 0 || ctx.write_failed) {
        fprintf(stderr, "Failed to write %s\n", out_path);
        ok = false;
    }
    return ok;
}

static bool key_is(const char* key, size_t len, const char* name) {
    return strlen(name) == len && memcmp(key, name, len) == 0;
}

static void set_cstring(tinytemplate_value_t* value, const char* s) { tinytemplate_set_string(value, s, strlen(s)); }

// tinytemplate writes strings as-is, so every string in a view is HTML-escaped
// before it is handed to a template.
typedef struct {
    char* title;
    char* created;
    char* published;
    char* markdown_url;
    char* markdown;
    char** tags;
    size_t tags_count;
    size_t tags_next;
} EntryView;

static bool entry_view_next_tag(void* data, tinytemplate_value_t* value) {
    EntryView* v = data;
    if (v->tags_next >= v->tags_count) return false;
    set_cstring(value, v->tags[v->tags_next++]);
    return true;
}

static bool entry_view_get(void* userp, const char* key, size_t len, tinytemplate_value_t* value) {
    EntryView* v = ((RenderCtx*)userp)->view;
    if (key_is(key, len, "title"))
        set_cstring(value, v->title);
    else if (key_is(key, len, "created"))
        set_cstring(value, v->created);
    else if (key_is(key, len, "published"))
        set_cstring(value, v->published);
    else if (key_is(key, len, "markdown_url"))
        set_cstring(value, v->markdown_url);
    else if (key_is(key, len, "markdown"))
        set_cstring(value, v->markdown);
    else if (key_is(key, len, "tags")) {
        v->tags_next = 0;
        tinytemplate_set_array(value, v, entry_view_next_tag);
    } else
        return false;
    return true;
}

static bool render_entry(const Template* t, const Entry* e) {
    char md_url[PATH_MAX + sizeof(ENTRY_MD_FILE)];
    snprintf(md_url, sizeof(md_url), "%s%s", e->url, ENTRY_MD_FILE);

    EntryView v = {
        .title = html_escape(e->title),
        .created = html_escape(e->created_iso),
        .published = html_escape(e->iso_date),
        .markdown_url = html_escape(md_url),
        .markdown = html_escape(e->content),
        .tags = xmalloc((e->tags_count ? e->tags_count : 1) * sizeof(char*)),
        .tags_count = e->tags_count,
    };
    for (size_t i = 0; i < e->tags_count; i++) v.tags[i] = html_escape(e->tags[i]);

    bool ok = template_render(t, e->out_rel, entry_view_get, &v);

    for (size_t i = 0; i < v.tags_count; i++) free(v.tags[i]);
    free(v.tags);
    free(v.title);
    free(v.created);
    free(v.published);
    free(v.markdown_url);
    free(v.markdown);
    return ok;
}

typedef struct {
    char* url;
    char* title;
    char* date;
} IndexItem;

typedef struct {
    IndexItem* items;
    size_t count;
    size_t next;
} IndexView;

static bool index_item_get(void* data, const char* key, size_t len, tinytemplate_value_t* value) {
    IndexItem* item = data;
    if (key_is(key, len, "url"))
        set_cstring(value, item->url);
    else if (key_is(key, len, "title"))
        set_cstring(value, item->title);
    else if (key_is(key, len, "date"))
        set_cstring(value, item->date);
    else
        return false;
    return true;
}

static bool index_view_next_entry(void* data, tinytemplate_value_t* value) {
    IndexView* v = data;
    if (v->next >= v->count) return false;
    tinytemplate_set_dict(value, &v->items[v->next++], index_item_get);
    return true;
}

static bool index_view_get(void* userp, const char* key, size_t len, tinytemplate_value_t* value) {
    IndexView* v = ((RenderCtx*)userp)->view;
    if (!key_is(key, len, "entries")) return false;
    v->next = 0;
    tinytemplate_set_array(value, v, index_view_next_entry);
    return true;
}

static bool render_index(const Template* t, const char* out_path, Entry* entries, size_t n) {
    // sort entries by date desc, then title
    size_t* idx = xmalloc((n ? n : 1) * sizeof(size_t));
    for (size_t i = 0; i < n; i++) idx[i] = i;
    for (size_t i = 1; i < n; i++) {
        size_t key = idx[i];
        size_t j = i;
        while (j > 0) {
            Entry *a = &entries[idx[j - 1]], *b = &entries[key];
            int c = strcmp(b->iso_date, a->iso_date);  // desc by date
            if (c == 0) c = strcmp(a->title, b->title);
            if (c <= 0) break;
            idx[j] = idx[j - 1];
            j--;
        }
        idx[j] = key;
    }

    IndexView v = {.items = xmalloc((n ? n : 1) * sizeof(IndexItem)), .count = n, .next = 0};
    for (size_t i = 0; i < n; i++) {
        Entry* e = &entries[idx[i]];
        v.items[i].url = html_escape(e->url);
        v.items[i].title = html_escape(e->title);
        v.items[i].date = html_escape(e->date_ymd);
    }

    bool ok = template_render(t, out_path, index_view_get, &v);

    for (size_t i = 0; i < n; i++) {
        free(v.items[i].url);
        free(v.items[i].title);
        free(v.items[i].date);
    }
    free(v.items);
    free(idx);
    return ok;
}

static int cmp_entries_name(const void* a, const void* b) {
    const Entry *ea = (const Entry*)a, *eb = (const Entry*)b;
    return strcmp(ea->filename, eb->filename);
}

int main(void) {
    Template entry_tmpl, index_tmpl;
    if (!template_load(&entry_tmpl, TEMPLATES_DIR "/entry.html")) return 1;
    if (!template_load(&index_tmpl, TEMPLATES_DIR "/index.html")) return 1;

    // collect markdown entries
    DIR* dir = opendir(ENTRIES_DIR);
    if (!dir) {
        fprintf(stderr, "Failed to open %s: %s\n", ENTRIES_DIR, strerror(errno));
        fprintf(stderr, "Ensure submodule is initialized and contains notes/essays.\n");
        return 1;
    }
    Entry* entries = NULL;
    size_t n = 0, cap = 0;
    struct dirent* de;
    while ((de = readdir(dir)) != NULL) {
        if (de->d_name[0] == '.') continue;
        if (!ends_with(de->d_name, ".md")) continue;
        if (n == cap) {
            cap = cap ? cap * 2 : 16;
            entries = xrealloc(entries, cap * sizeof(Entry));
        }
        Entry* e = &entries[n++];
        memset(e, 0, sizeof(*e));
        e->filename = xstrdup(de->d_name);
    }
    closedir(dir);
    qsort(entries, n, sizeof(Entry), cmp_entries_name);

    for (size_t i = 0; i < n; i++) {
        Entry* e = &entries[i];
        size_t fl = strlen(e->filename);
        e->stem = xmalloc(fl - 2);
        memcpy(e->stem, e->filename, fl - 3);
        e->stem[fl - 3] = 0;
        e->slug = make_slug_from_stem(e->stem);
        e->title = make_title_from_stem(e->stem);

        char rel_in_repo[PATH_MAX];
        snprintf(rel_in_repo, sizeof(rel_in_repo), "essays/%s", e->filename);
        e->iso_date = get_last_edit_iso(rel_in_repo);
        e->created_iso = get_created_iso(rel_in_repo);
        derive_date_parts(e);

        char* fullpath = path_join2(ENTRIES_DIR, e->filename);
        e->content = read_file(fullpath);
        free(fullpath);
        if (!e->content) {
            fprintf(stderr, "Failed to read %s/%s\n", ENTRIES_DIR, e->filename);
            return 1;
        }

        StrList asset_tags;
        strlist_init(&asset_tags);
        StrList md_words;
        strlist_init(&md_words);
        load_asset_tags(&asset_tags);
        add_words_from_text(&md_words, e->content);
        StrList tags;
        strlist_init(&tags);
        for (size_t ti = 0; ti < asset_tags.count; ti++) {
            if (strlist_contains(&md_words, asset_tags.items[ti])) {
                strlist_add_unique(&tags, asset_tags.items[ti]);
            }
        }
        strlist_sort(&tags);
        e->tags = tags.items;
        e->tags_count = tags.count;
        for (size_t ti = 0; ti < asset_tags.count; ti++) free(asset_tags.items[ti]);
        free(asset_tags.items);
        for (size_t ti = 0; ti < md_words.count; ti++) free(md_words.items[ti]);
        free(md_words.items);

        char out_rel[PATH_MAX];
        snprintf(out_rel, sizeof(out_rel), "%s/%s/%s/%s/%s/%s/index.html", OUT_DIR, OUT_ARCHIVE_DIR, e->year, e->month,
                 e->day, e->slug);
        e->out_rel = xstrdup(out_rel);
        char url[PATH_MAX];
        snprintf(url, sizeof(url), "/%s/%s/%s/%s/%s/", OUT_ARCHIVE_DIR, e->year, e->month, e->day, e->slug);
        e->url = xstrdup(url);
    }

    if (!copy_assets()) {
        fprintf(stderr, "Failed to copy %s to %s\n", ASSETS_DIR, OUT_DIR "/" ASSETS_DIR);
        return 1;
    }

    // write entry pages
    for (size_t i = 0; i < n; i++) {
        Entry* e = &entries[i];
        char dirpath[PATH_MAX];
        snprintf(dirpath, sizeof(dirpath), "%s/%s/%s/%s/%s/%s", OUT_DIR, OUT_ARCHIVE_DIR, e->year, e->month, e->day,
                 e->slug);
        if (!mkdir_p(dirpath)) {
            fprintf(stderr, "Failed to create dir %s\n", dirpath);
            return 1;
        }
        if (!render_entry(&entry_tmpl, e)) return 1;

        char md_path[PATH_MAX + sizeof(ENTRY_MD_FILE) + 1];
        snprintf(md_path, sizeof(md_path), "%s/%s", dirpath, ENTRY_MD_FILE);
        if (!write_file(md_path, e->content)) {
            fprintf(stderr, "Failed to write %s\n", md_path);
            return 1;
        }
    }

    // index
    if (!render_index(&index_tmpl, OUT_DIR "/index.html", entries, n)) return 1;

    printf("Generated %zu entries and index.html in %s\n", n, OUT_DIR);
    return 0;
}
