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

#define ENTRIES_DIR "notes/writing/essays"
#define TAGS_PATH "assets/tags"
#define OUT_ARCHIVE_DIR "archive"
#define MAX_WORDS 65536
#define MAX_WORD_LEN 128

typedef struct {
    char *filename;
    char *stem;
    char *slug;
    char *title;
    char *iso_date;
    char date_ymd[11]; // YYYY-MM-DD
    char year[5];
    char month[3];
    char day[3];
    char *content;
    char **tags;
    size_t tags_count;
    char *out_rel; // archive/YYYY/MM/DD/slug/index.html
    char *url;     // /archive/YYYY/MM/DD/slug/
} Entry;

typedef struct {
    char **items;
    size_t count;
    size_t cap;
} StrList;

static void die(const char *msg) {
    perror(msg);
    exit(1);
}

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) die("malloc");
    return p;
}

static void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n);
    if (!q) die("realloc");
    return q;
}

static char *xstrdup(const char *s) {
    char *p = strdup(s);
    if (!p) die("strdup");
    return p;
}

static bool ends_with(const char *s, const char *suf) {
    size_t ls = strlen(s), lsu = strlen(suf);
    if (lsu > ls) return false;
    return strcmp(s + ls - lsu, suf) == 0;
}

static void strlist_init(StrList *l) { l->items = NULL; l->count = 0; l->cap = 0; }

static bool strlist_contains(const StrList *l, const char *s) {
    for (size_t i = 0; i < l->count; i++) if (strcmp(l->items[i], s) == 0) return true;
    return false;
}

static void strlist_add_unique(StrList *l, const char *s) {
    if (strlist_contains(l, s)) return;
    if (l->count == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 32;
        l->items = xrealloc(l->items, l->cap * sizeof(char*));
    }
    l->items[l->count++] = xstrdup(s);
}

static int cmp_strptr(const void *a, const void *b) {
    const char *sa = *(const char* const*)a;
    const char *sb = *(const char* const*)b;
    return strcmp(sa, sb);
}

static void strlist_sort(StrList *l) {
    if (l->count > 1) qsort(l->items, l->count, sizeof(char*), cmp_strptr);
}

static bool mkdir_p(const char *path) {
    char tmp[PATH_MAX];
    size_t len = strlen(path);
    if (len == 0 || len >= sizeof(tmp)) return false;
    memcpy(tmp, path, len + 1);
    if (tmp[len - 1] == '/') tmp[len - 1] = 0;
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return false;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return false;
    return true;
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    rewind(f);
    char *buf = xmalloc((size_t)sz + 1);
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

static bool write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    size_t len = strlen(content);
    bool ok = fwrite(content, 1, len, f) == len;
    if (fclose(f) != 0) ok = false;
    return ok;
}

static char *path_join2(const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b);
    bool slash = la > 0 && a[la-1] == '/';
    char *r = xmalloc(la + lb + 2);
    if (slash) sprintf(r, "%s%s", a, b);
    else sprintf(r, "%s/%s", a, b);
    return r;
}

static char *run_command_capture(const char *cmd) {
    FILE *p = popen(cmd, "r");
    if (!p) return NULL;
    char buf[4096];
    size_t cap = 256, len = 0;
    char *out = xmalloc(cap);
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
    while (len > 0 && (out[len-1] == '\n' || out[len-1] == '\r' || out[len-1] == ' ' || out[len-1] == '\t')) {
        out[--len] = 0;
    }
    return out;
}

static char *get_last_edit_iso(const char *filepath_in_repo) {
    // filepath_in_repo relative to notes repo, e.g. writing/essays/foo.md
    char cmd[PATH_MAX * 2];
    snprintf(cmd, sizeof(cmd), "git -C notes log -1 --format=%%aI -- \"%s\"", filepath_in_repo);
    char *out = run_command_capture(cmd);
    if (!out || out[0] == '\0') {
        if (out) free(out);
        return xstrdup("1970-01-01T00:00:00Z");
    }
    return out;
}

static void derive_date_parts(Entry *e) {
    // iso like 2026-11-03T11:40:00+08:00 or ...Z
    memset(e->date_ymd, 0, sizeof(e->date_ymd));
    if (strlen(e->iso_date) >= 10) {
        memcpy(e->date_ymd, e->iso_date, 10);
        e->date_ymd[10] = 0;
        memcpy(e->year, e->iso_date, 4); e->year[4] = 0;
        memcpy(e->month, e->iso_date + 5, 2); e->month[2] = 0;
        memcpy(e->day, e->iso_date + 8, 2); e->day[2] = 0;
    } else {
        strcpy(e->date_ymd, "1970-01-01");
        strcpy(e->year, "1970"); strcpy(e->month, "01"); strcpy(e->day, "01");
    }
}

static char *make_slug_from_stem(const char *stem) {
    size_t n = strlen(stem);
    char *s = xmalloc(n + 1);
    size_t j = 0;
    int last_dash = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)stem[i];
        if (isalnum(c)) {
            s[j++] = (char)tolower(c);
            last_dash = 0;
        } else if (c == '-' || c == '_' || isspace(c)) {
            if (j > 0 && !last_dash) { s[j++] = '-'; last_dash = 1; }
        } else {
            if (j > 0 && !last_dash) { s[j++] = '-'; last_dash = 1; }
        }
    }
    while (j > 0 && s[j-1] == '-') j--;
    if (j == 0) {
        strcpy(s, "entry");
        return s;
    }
    s[j] = 0;
    return s;
}

static char *make_title_from_stem(const char *stem) {
    size_t n = strlen(stem);
    char *t = xmalloc(n + 1);
    int cap = 1;
    size_t j = 0;
    for (size_t i = 0; i < n; i++) {
        char c = stem[i];
        if (c == '-' || c == '_' ) {
            if (j > 0 && t[j-1] != ' ') t[j++] = ' ';
            cap = 1;
        } else {
            if (cap && isalpha((unsigned char)c)) { t[j++] = (char)toupper((unsigned char)c); cap = 0; }
            else { t[j++] = c; if (!isspace((unsigned char)c)) cap = 0; }
        }
    }
    t[j] = 0;
    // trim
    while (j > 0 && t[j-1] == ' ') t[--j] = 0;
    return t;
}

static void add_words_from_text(StrList *words, const char *text) {
    char buf[MAX_WORD_LEN];
    size_t bi = 0;
    for (const unsigned char *p = (const unsigned char*)text; ; p++) {
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

static void load_asset_tags(StrList *words) {
    char *content = read_file(TAGS_PATH);
    if (!content) return;
    char buf[MAX_WORD_LEN];
    size_t bi = 0;
    for (const unsigned char *p = (const unsigned char*)content; ; p++) {
        unsigned char c = *p;
        if (c && !isspace(c)) {
            if (bi + 1 < sizeof(buf)) buf[bi++] = (char)tolower(c);
        } else {
            if (bi > 0) { buf[bi] = 0; strlist_add_unique(words, buf); bi = 0; }
            if (!c) break;
        }
    }
    free(content);
}

static char *html_escape(const char *s) {
    size_t extra = 0;
    for (const unsigned char *p = (const unsigned char*)s; *p; p++) {
        switch (*p) {
            case '&': extra += 4; break; // &amp;
            case '<': extra += 3; break; // &lt;
            case '>': extra += 3; break; // &gt;
            case '"': extra += 5; break; // &quot;
            default: break;
        }
    }
    char *o = xmalloc(strlen(s) + extra + 1);
    char *d = o;
    for (const unsigned char *p = (const unsigned char*)s; *p; p++) {
        switch (*p) {
            case '&': memcpy(d, "&amp;", 5); d += 5; break;
            case '<': memcpy(d, "&lt;", 4); d += 4; break;
            case '>': memcpy(d, "&gt;", 4); d += 4; break;
            case '"': memcpy(d, "&quot;", 6); d += 6; break;
            default: *d++ = (char)*p; break;
        }
    }
    *d = 0;
    return o;
}

static char *json_escape_string(const char *s) {
    // returns escaped content without surrounding quotes
    size_t extra = 0;
    for (const unsigned char *p = (const unsigned char*)s; *p; p++) {
        if (*p == '\\' || *p == '"' ) extra += 1;
        else if (*p == '\n' || *p == '\r' || *p == '\t') extra += 1;
        else if (*p < 0x20) extra += 5;
    }
    char *o = xmalloc(strlen(s) + extra + 1);
    char *d = o;
    for (const unsigned char *p = (const unsigned char*)s; *p; p++) {
        if (*p == '\\') { *d++='\\'; *d++='\\'; }
        else if (*p == '"') { *d++='\\'; *d++='"'; }
        else if (*p == '\n') { *d++='\\'; *d++='n'; }
        else if (*p == '\r') { *d++='\\'; *d++='r'; }
        else if (*p == '\t') { *d++='\\'; *d++='t'; }
        else if (*p < 0x20) { sprintf(d, "\\u%04x", *p); d += 6; }
        else { *d++ = (char)*p; }
    }
    *d = 0;
    return o;
}

static char *build_entry_html(const Entry *e) {
    char *title_e = html_escape(e->title);
    char *date_e = html_escape(e->iso_date);
    char *md_json = json_escape_string(e->content);
    // tags html
    size_t tags_cap = 64;
    char *tags_html = xmalloc(tags_cap);
    tags_html[0] = 0;
    strcat(tags_html, "");
    size_t tlen = 0;
    for (size_t i = 0; i < e->tags_count; i++) {
        char *te = html_escape(e->tags[i]);
        size_t need = tlen + strlen(te) + 32;
        if (need > tags_cap) { while (need > tags_cap) tags_cap *= 2; tags_html = xrealloc(tags_html, tags_cap); }
        tlen += sprintf(tags_html + tlen, "<span class=\"tag\">%s</span>", te);
        free(te);
    }

    const char *tmpl1 =
        "<!doctype html>\n<html lang=\"en\">\n<head>\n  <meta charset=\"utf-8\">\n"
        "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        "  <title>";
    const char *tmpl2 =
        "</title>\n  <link rel=\"stylesheet\" href=\"/assets/style.css\">\n"
        "</head>\n<body>\n  <main class=\"page entry-page\">\n"
        "    <a class=\"back\" href=\"/\">&larr; index</a>\n"
        "    <header class=\"entry-header\">\n      <h1>";
    const char *tmpl3 = "</h1>\n      <div class=\"meta\">\n        <div class=\"published\">Published: <time datetime=\"";
    const char *tmpl4 = "\">";
    const char *tmpl5 = "</time></div>\n        <div class=\"tags\">Tags: ";
    const char *tmpl6 = "</div>\n      </div>\n    </header>\n"
        "    <article id=\"content\" class=\"entry-content\"></article>\n"
        "    <noscript><pre class=\"entry-raw\">";
    const char *tmpl7 = "</pre></noscript>\n  </main>\n"
        "  <script>window.__ENTRY_MARKDOWN__ = \"";
    const char *tmpl8 = "\";</script>\n  <script src=\"/assets/entry.js\"></script>\n"
        "</body>\n</html>\n";

    char *md_html_esc = html_escape(e->content);
    size_t total = strlen(tmpl1)+strlen(title_e)+strlen(tmpl2)+strlen(title_e)+strlen(tmpl3)+
        strlen(date_e)+strlen(tmpl4)+strlen(date_e)+strlen(tmpl5)+strlen(tags_html)+
        strlen(tmpl6)+strlen(md_html_esc)+strlen(tmpl7)+strlen(md_json)+strlen(tmpl8)+1;
    char *out = xmalloc(total);
    sprintf(out, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s", tmpl1,title_e,tmpl2,title_e,tmpl3,
            date_e,tmpl4,date_e,tmpl5,tags_html,tmpl6,md_html_esc,tmpl7,md_json,tmpl8);
    free(title_e); free(date_e); free(md_json); free(tags_html); free(md_html_esc);
    return out;
}

static char *build_index_html(Entry *entries, size_t n) {
    // sort entries by date desc, then title
    // simple insertion copy of indices
    size_t *idx = xmalloc(n * sizeof(size_t));
    for (size_t i=0;i<n;i++) idx[i]=i;
    for (size_t i=1;i<n;i++) {
        size_t key=idx[i]; size_t j=i;
        while (j>0) {
            Entry *a=&entries[idx[j-1]], *b=&entries[key];
            int c = strcmp(b->iso_date, a->iso_date); // desc by date
            if (c == 0) c = strcmp(a->title, b->title);
            if (c <= 0) break;
            idx[j]=idx[j-1]; j--;
        }
        idx[j]=key;
    }
    const char *head =
        "<!doctype html>\n<html lang=\"en\">\n<head>\n  <meta charset=\"utf-8\">\n"
        "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        "  <title>Blog</title>\n  <link rel=\"stylesheet\" href=\"/assets/style.css\">\n"
        "</head>\n<body>\n  <main class=\"page\">\n    <h1>Index</h1>\n    <ul class=\"entry-list\">\n";
    const char *foot = "    </ul>\n  </main>\n</body>\n</html>\n";
    size_t cap = strlen(head)+strlen(foot)+128;
    char *out = xmalloc(cap);
    strcpy(out, head);
    size_t len = strlen(out);
    for (size_t ii=0; ii<n; ii++) {
        Entry *e = &entries[idx[ii]];
        char *t = html_escape(e->title);
        char *d = html_escape(e->date_ymd);
        char item[PATH_MAX + 512];
        snprintf(item, sizeof(item),
            "      <li><a href=\"%s\">%s</a> <span class=\"date\">%s</span></li>\n",
            e->url, t, d);
        size_t il = strlen(item);
        if (len + il + strlen(foot) + 1 > cap) { while (len + il + strlen(foot) + 1 > cap) cap *= 2; out = xrealloc(out, cap); }
        memcpy(out+len, item, il); len += il; out[len]=0;
        free(t); free(d);
    }
    if (len + strlen(foot) + 1 > cap) { cap = len + strlen(foot) + 1; out = xrealloc(out, cap); }
    strcat(out, foot);
    free(idx);
    return out;
}

static int cmp_entries_name(const void *a, const void *b) {
    const Entry *ea = (const Entry*)a, *eb = (const Entry*)b;
    return strcmp(ea->filename, eb->filename);
}

int main(void) {
    // collect markdown entries
    DIR *dir = opendir(ENTRIES_DIR);
    if (!dir) {
        fprintf(stderr, "Failed to open %s: %s\n", ENTRIES_DIR, strerror(errno));
        fprintf(stderr, "Ensure submodule is initialized and contains notes/writing/essays.\n");
        return 1;
    }
    Entry *entries = NULL;
    size_t n = 0, cap = 0;
    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        if (de->d_name[0] == '.') continue;
        if (!ends_with(de->d_name, ".md")) continue;
        if (n == cap) { cap = cap ? cap*2 : 16; entries = xrealloc(entries, cap * sizeof(Entry)); }
        Entry *e = &entries[n++];
        memset(e, 0, sizeof(*e));
        e->filename = xstrdup(de->d_name);
    }
    closedir(dir);
    qsort(entries, n, sizeof(Entry), cmp_entries_name);

    for (size_t i = 0; i < n; i++) {
        Entry *e = &entries[i];
        size_t fl = strlen(e->filename);
        e->stem = xmalloc(fl - 2);
        memcpy(e->stem, e->filename, fl - 3);
        e->stem[fl - 3] = 0;
        e->slug = make_slug_from_stem(e->stem);
        e->title = make_title_from_stem(e->stem);

        char rel_in_repo[PATH_MAX];
        snprintf(rel_in_repo, sizeof(rel_in_repo), "writing/essays/%s", e->filename);
        e->iso_date = get_last_edit_iso(rel_in_repo);
        derive_date_parts(e);

        char *fullpath = path_join2(ENTRIES_DIR, e->filename);
        e->content = read_file(fullpath);
        free(fullpath);
        if (!e->content) {
            fprintf(stderr, "Failed to read %s/%s\n", ENTRIES_DIR, e->filename);
            return 1;
        }

        StrList asset_tags; strlist_init(&asset_tags);
        StrList md_words; strlist_init(&md_words);
        load_asset_tags(&asset_tags);
        add_words_from_text(&md_words, e->content);
        StrList tags; strlist_init(&tags);
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
        snprintf(out_rel, sizeof(out_rel), "%s/%s/%s/%s/%s/index.html", OUT_ARCHIVE_DIR, e->year, e->month, e->day, e->slug);
        e->out_rel = xstrdup(out_rel);
        char url[PATH_MAX];
        snprintf(url, sizeof(url), "/%s/%s/%s/%s/%s/", OUT_ARCHIVE_DIR, e->year, e->month, e->day, e->slug);
        e->url = xstrdup(url);
    }

    // ensure base dirs
    mkdir_p("assets");
    mkdir_p(OUT_ARCHIVE_DIR);

    // write entry pages
    for (size_t i = 0; i < n; i++) {
        Entry *e = &entries[i];
        char dirpath[PATH_MAX];
        snprintf(dirpath, sizeof(dirpath), "%s/%s/%s/%s/%s", OUT_ARCHIVE_DIR, e->year, e->month, e->day, e->slug);
        if (!mkdir_p(dirpath)) {
            fprintf(stderr, "Failed to create dir %s\n", dirpath);
            return 1;
        }
        char *html = build_entry_html(e);
        if (!write_file(e->out_rel, html)) {
            fprintf(stderr, "Failed to write %s\n", e->out_rel);
            free(html);
            return 1;
        }
        free(html);
    }

    // index
    char *index = build_index_html(entries, n);
    if (!write_file("index.html", index)) {
        fprintf(stderr, "Failed to write index.html\n");
        return 1;
    }
    free(index);

    printf("Generated %zu entries and index.html\n", n);
    return 0;
}
