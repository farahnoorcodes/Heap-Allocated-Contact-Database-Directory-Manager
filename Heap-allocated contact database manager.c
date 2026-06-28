/*
 * ============================================================
 *   Heap-Allocated Contact Database Directory Manager
 *   A memory-efficient console registry using dynamic heap
 *   allocation  with search, sort,
 *   and flat-file export capabilities.
 * ============================================================
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>   /* strcasecmp */
#include <ctype.h>
#include <time.h>

/* ─── Constants ─────────────────────────────────────────── */
#define INITIAL_CAPACITY   4        /* starting heap slots   */
#define FIELD_MAX          64       /* max chars per field   */
#define PHONE_MAX          20
#define EMAIL_MAX          80
#define LOG_FILE           "contacts.log"
#define EXPORT_FILE        "contacts_export.txt"

/* ─── Color Codes (ANSI) ────────────────────────────────── */
#define CLR_RESET  "\033[0m"
#define CLR_BOLD   "\033[1m"
#define CLR_CYAN   "\033[36m"
#define CLR_GREEN  "\033[32m"
#define CLR_YELLOW "\033[33m"
#define CLR_RED    "\033[31m"
#define CLR_BLUE   "\033[34m"
#define CLR_MAG    "\033[35m"

/* ═══════════════════════════════════════════════════════════
 *  DATA STRUCTURE DEFINITIONS
 * ═══════════════════════════════════════════════════════════ */

/* Primary contact record template */
typedef struct {
    int    id;
    char   first_name[FIELD_MAX];
    char   last_name[FIELD_MAX];
    char   phone[PHONE_MAX];
    char   email[EMAIL_MAX];
    char   company[FIELD_MAX];
    char   city[FIELD_MAX];
    char   category[FIELD_MAX];   /* e.g. Work, Family, Friend */
    int    is_favourite;
} Contact;

/* Heap-managed database container */
typedef struct {
    Contact *records;    /* heap-allocated array of contacts */
    int      count;      /* current number of contacts       */
    int      capacity;   /* allocated slots on the heap      */
    int      next_id;    /* monotonically increasing ID      */
} Database;

/* Search result list (dynamically allocated) */
typedef struct {
    Contact **matches;   /* pointers into the main heap      */
    int       count;
} SearchResult;

/* ═══════════════════════════════════════════════════════════
 *  HEAP MANAGEMENT
 * ═══════════════════════════════════════════════════════════ */

/* Initialise the database on the heap */
Database *db_create(void) {
    Database *db = (Database *)malloc(sizeof(Database));
    if (!db) { perror("malloc Database"); exit(EXIT_FAILURE); }

    db->records  = (Contact *)malloc(INITIAL_CAPACITY * sizeof(Contact));
    if (!db->records) { perror("malloc records"); exit(EXIT_FAILURE); }

    db->count    = 0;
    db->capacity = INITIAL_CAPACITY;
    db->next_id  = 1;
    return db;
}

/* Grow the heap array when full (doubling strategy) */
static void db_grow(Database *db) {
    int new_cap = db->capacity * 2;
    Contact *tmp = (Contact *)realloc(db->records, new_cap * sizeof(Contact));
    if (!tmp) { perror("realloc records"); exit(EXIT_FAILURE); }
    db->records  = tmp;
    db->capacity = new_cap;
    printf(CLR_YELLOW "[MEM] Heap reallocated → capacity now %d slots (%zu bytes)\n" CLR_RESET,
           new_cap, (size_t)(new_cap * sizeof(Contact)));
}

/* Release all heap memory */
void db_destroy(Database *db) {
    if (db) {
        free(db->records);
        free(db);
    }
}

/* Free a search-result structure */
void sr_destroy(SearchResult *sr) {
    if (sr) {
        free(sr->matches);
        free(sr);
    }
}

/* ═══════════════════════════════════════════════════════════
 *  UTILITY HELPERS
 * ═══════════════════════════════════════════════════════════ */

/* Trim trailing newline from fgets() input */
static void trim_newline(char *s) {
    size_t len = strlen(s);
    if (len > 0 && s[len - 1] == '\n') s[len - 1] = '\0';
}

/* Case-insensitive substring search */
static int istr_contains(const char *haystack, const char *needle) {
    if (!needle || needle[0] == '\0') return 1;
    size_t nlen = strlen(needle);
    for (size_t i = 0; haystack[i]; i++) {
        size_t j;
        for (j = 0; j < nlen; j++) {
            if (tolower((unsigned char)haystack[i + j]) !=
                tolower((unsigned char)needle[j])) break;
        }
        if (j == nlen) return 1;
    }
    return 0;
}

/* Prompt and read a string field safely */
static void read_field(const char *prompt, char *buf, size_t size) {
    printf("  %s", prompt);
    fflush(stdout);
    if (!fgets(buf, (int)size, stdin)) buf[0] = '\0';
    trim_newline(buf);
}

/* Print a horizontal rule */
static void hr(void) {
    printf(CLR_CYAN "  ────────────────────────────────────────────────────\n" CLR_RESET);
}

/* Log an action to the flat text log file */
static void log_action(const char *action, const char *detail) {
    FILE *f = fopen(LOG_FILE, "a");
    if (!f) return;
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(f, "[%s] %-12s | %s\n", ts, action, detail);
    fclose(f);
}

/* Build a log-detail string for a contact safely */
static void contact_detail(char *buf, size_t size, const Contact *c) {
    snprintf(buf, size, "ID=%d  %.40s %.40s", c->id, c->first_name, c->last_name);
}

/* ═══════════════════════════════════════════════════════════
 *  DISPLAY
 * ═══════════════════════════════════════════════════════════ */

static void print_contact_row(const Contact *c) {
    printf("  " CLR_BOLD CLR_BLUE "%-4d" CLR_RESET
           "  %-20s %-20s  %-18s  %-28s\n",
           c->id,
           c->first_name,
           c->last_name,
           c->phone,
           c->email);
    if (c->company[0] || c->city[0]) {
        printf("        " CLR_MAG "%-20s" CLR_RESET "  %s%s\n",
               c->company, c->city,
               c->is_favourite ? "  ★ Favourite" : "");
    }
    printf("        " CLR_YELLOW "[%s]" CLR_RESET "\n", c->category[0] ? c->category : "Uncategorised");
}

static void print_contact_full(const Contact *c) {
    hr();
    printf("  " CLR_BOLD CLR_CYAN "Contact #%d" CLR_RESET "%s\n",
           c->id, c->is_favourite ? "  ★ Favourite" : "");
    hr();
    printf("  Name     : %s %s\n", c->first_name, c->last_name);
    printf("  Phone    : %s\n", c->phone);
    printf("  Email    : %s\n", c->email);
    printf("  Company  : %s\n", c->company[0] ? c->company : "—");
    printf("  City     : %s\n", c->city[0]    ? c->city    : "—");
    printf("  Category : %s\n", c->category[0]? c->category: "Uncategorised");
    hr();
}

static void print_table_header(void) {
    printf("\n  " CLR_BOLD CLR_CYAN
           "%-4s  %-20s %-20s  %-18s  %-28s\n" CLR_RESET,
           "ID", "First Name", "Last Name", "Phone", "Email");
    hr();
}

/* ═══════════════════════════════════════════════════════════
 *  CRUD OPERATIONS
 * ═══════════════════════════════════════════════════════════ */

void db_add(Database *db) {
    if (db->count == db->capacity) db_grow(db);

    Contact *c = &db->records[db->count];
    memset(c, 0, sizeof(Contact));
    c->id = db->next_id++;

    printf("\n" CLR_BOLD CLR_GREEN "  ── Add New Contact ──\n" CLR_RESET);
    read_field("First Name  : ", c->first_name, FIELD_MAX);
    read_field("Last Name   : ", c->last_name,  FIELD_MAX);
    read_field("Phone       : ", c->phone,       PHONE_MAX);
    read_field("Email       : ", c->email,       EMAIL_MAX);
    read_field("Company     : ", c->company,     FIELD_MAX);
    read_field("City        : ", c->city,        FIELD_MAX);
    read_field("Category    : ", c->category,    FIELD_MAX);

    char fav[4];
    read_field("Favourite? (y/n): ", fav, sizeof(fav));
    c->is_favourite = (fav[0] == 'y' || fav[0] == 'Y') ? 1 : 0;

    db->count++;

    char detail[128];
    contact_detail(detail, sizeof(detail), c);
    log_action("ADD", detail);

    printf(CLR_GREEN "  ✔ Contact #%d added successfully.\n" CLR_RESET, c->id);
    printf(CLR_YELLOW "  [MEM] Database: %d / %d slots used (%zu bytes allocated)\n" CLR_RESET,
           db->count, db->capacity, (size_t)(db->capacity * sizeof(Contact)));
}

void db_list(const Database *db) {
    if (db->count == 0) {
        printf(CLR_YELLOW "  No contacts in database.\n" CLR_RESET);
        return;
    }
    print_table_header();
    for (int i = 0; i < db->count; i++)
        print_contact_row(&db->records[i]);
    printf("\n  Total: %d contact(s)  |  Heap slots: %d\n", db->count, db->capacity);
}

void db_view_by_id(const Database *db) {
    int id;
    printf("  Enter Contact ID: ");
    if (scanf("%d", &id) != 1) { while (getchar() != '\n'); return; }
    while (getchar() != '\n');

    for (int i = 0; i < db->count; i++) {
        if (db->records[i].id == id) {
            print_contact_full(&db->records[i]);
            return;
        }
    }
    printf(CLR_RED "  Contact #%d not found.\n" CLR_RESET, id);
}

void db_edit(Database *db) {
    int id;
    printf("  Enter Contact ID to edit: ");
    if (scanf("%d", &id) != 1) { while (getchar() != '\n'); return; }
    while (getchar() != '\n');

    for (int i = 0; i < db->count; i++) {
        if (db->records[i].id == id) {
            Contact *c = &db->records[i];
            printf(CLR_YELLOW "  (Press ENTER to keep existing value)\n\n" CLR_RESET);

            char buf[EMAIL_MAX];

#define EDIT_FIELD(prompt, field, size) \
    printf("  %s [%s]: ", prompt, field); fflush(stdout); \
    if (fgets(buf, size, stdin) && buf[0] != '\n') { \
        trim_newline(buf); strncpy(field, buf, size - 1); field[size-1]='\0'; }

            EDIT_FIELD("First Name", c->first_name, FIELD_MAX);
            EDIT_FIELD("Last Name",  c->last_name,  FIELD_MAX);
            EDIT_FIELD("Phone",      c->phone,      PHONE_MAX);
            EDIT_FIELD("Email",      c->email,      EMAIL_MAX);
            EDIT_FIELD("Company",    c->company,    FIELD_MAX);
            EDIT_FIELD("City",       c->city,       FIELD_MAX);
            EDIT_FIELD("Category",   c->category,   FIELD_MAX);
#undef EDIT_FIELD

            char fav[4];
            printf("  Favourite? (y/n) [%s]: ", c->is_favourite ? "y" : "n");
            fflush(stdout);
            if (fgets(fav, sizeof(fav), stdin) && fav[0] != '\n')
                c->is_favourite = (fav[0] == 'y' || fav[0] == 'Y') ? 1 : 0;

            char detail[128];
            contact_detail(detail, sizeof(detail), c);
            log_action("EDIT", detail);

            printf(CLR_GREEN "  ✔ Contact #%d updated.\n" CLR_RESET, id);
            return;
        }
    }
    printf(CLR_RED "  Contact #%d not found.\n" CLR_RESET, id);
}

void db_delete(Database *db) {
    int id;
    printf("  Enter Contact ID to delete: ");
    if (scanf("%d", &id) != 1) { while (getchar() != '\n'); return; }
    while (getchar() != '\n');

    for (int i = 0; i < db->count; i++) {
        if (db->records[i].id == id) {
            char detail[128];
            contact_detail(detail, sizeof(detail), &db->records[i]);

            /* Shift left — no fragmentation, O(n) */
            memmove(&db->records[i], &db->records[i + 1],
                    (size_t)(db->count - i - 1) * sizeof(Contact));
            db->count--;

            /* Optionally shrink heap if usage drops below 25% */
            if (db->count > 0 && db->count <= db->capacity / 4 && db->capacity > INITIAL_CAPACITY) {
                int new_cap = db->capacity / 2;
                Contact *tmp = (Contact *)realloc(db->records, new_cap * sizeof(Contact));
                if (tmp) {
                    db->records  = tmp;
                    db->capacity = new_cap;
                    printf(CLR_YELLOW "[MEM] Heap shrunk → capacity now %d slots\n" CLR_RESET, new_cap);
                }
            }

            log_action("DELETE", detail);
            printf(CLR_GREEN "  ✔ Contact #%d deleted.\n" CLR_RESET, id);
            return;
        }
    }
    printf(CLR_RED "  Contact #%d not found.\n" CLR_RESET, id);
}

/* ═══════════════════════════════════════════════════════════
 *  SEARCH ENGINE
 * ═══════════════════════════════════════════════════════════ */

SearchResult *db_search(const Database *db, const char *query) {
    /* Allocate result structure on heap */
    SearchResult *sr = (SearchResult *)malloc(sizeof(SearchResult));
    if (!sr) { perror("malloc SearchResult"); return NULL; }

    sr->matches = (Contact **)malloc((size_t)db->count * sizeof(Contact *));
    if (!sr->matches) { perror("malloc matches"); free(sr); return NULL; }
    sr->count = 0;

    for (int i = 0; i < db->count; i++) {
        const Contact *c = &db->records[i];
        if (istr_contains(c->first_name, query) ||
            istr_contains(c->last_name,  query) ||
            istr_contains(c->phone,      query) ||
            istr_contains(c->email,      query) ||
            istr_contains(c->company,    query) ||
            istr_contains(c->city,       query) ||
            istr_contains(c->category,   query)) {
            sr->matches[sr->count++] = (Contact *)c;
        }
    }
    return sr;
}

void db_search_menu(const Database *db) {
    char query[FIELD_MAX];
    read_field("Search query: ", query, FIELD_MAX);

    SearchResult *sr = db_search(db, query);
    if (!sr) return;

    printf("\n  " CLR_BOLD "%d result(s) for \"%s\"\n" CLR_RESET, sr->count, query);
    if (sr->count > 0) {
        print_table_header();
        for (int i = 0; i < sr->count; i++)
            print_contact_row(sr->matches[i]);
    }

    char detail[128];
    snprintf(detail, sizeof(detail), "query=\"%s\" results=%d", query, sr->count);
    log_action("SEARCH", detail);

    sr_destroy(sr);
}

/* ═══════════════════════════════════════════════════════════
 *  SORTING (custom comparators + qsort on heap array)
 * ═══════════════════════════════════════════════════════════ */

static int cmp_last_asc(const void *a, const void *b) {
    return strcasecmp(((const Contact *)a)->last_name,
                      ((const Contact *)b)->last_name);
}
static int cmp_last_desc(const void *a, const void *b) {
    return strcasecmp(((const Contact *)b)->last_name,
                      ((const Contact *)a)->last_name);
}
static int cmp_first_asc(const void *a, const void *b) {
    return strcasecmp(((const Contact *)a)->first_name,
                      ((const Contact *)b)->first_name);
}
static int cmp_id_asc(const void *a, const void *b) {
    return ((const Contact *)a)->id - ((const Contact *)b)->id;
}
static int cmp_company_asc(const void *a, const void *b) {
    return strcasecmp(((const Contact *)a)->company,
                      ((const Contact *)b)->company);
}
static int cmp_city_asc(const void *a, const void *b) {
    return strcasecmp(((const Contact *)a)->city,
                      ((const Contact *)b)->city);
}
static int cmp_fav_first(const void *a, const void *b) {
    /* Favourites first, then by last name */
    int fa = ((const Contact *)a)->is_favourite;
    int fb = ((const Contact *)b)->is_favourite;
    if (fa != fb) return fb - fa;
    return strcasecmp(((const Contact *)a)->last_name,
                      ((const Contact *)b)->last_name);
}

void db_sort_menu(Database *db) {
    printf("\n" CLR_BOLD "  Sort by:\n" CLR_RESET);
    printf("  [1] Last Name  (A→Z)\n");
    printf("  [2] Last Name  (Z→A)\n");
    printf("  [3] First Name (A→Z)\n");
    printf("  [4] ID         (asc)\n");
    printf("  [5] Company    (A→Z)\n");
    printf("  [6] City       (A→Z)\n");
    printf("  [7] Favourites first\n");
    printf("  Choice: ");
    fflush(stdout);

    int ch;
    if (scanf("%d", &ch) != 1) { while (getchar() != '\n'); return; }
    while (getchar() != '\n');

    typedef int (*Cmp)(const void *, const void *);
    Cmp fns[] = { NULL, cmp_last_asc, cmp_last_desc, cmp_first_asc,
                  cmp_id_asc, cmp_company_asc, cmp_city_asc, cmp_fav_first };

    if (ch < 1 || ch > 7) { printf(CLR_RED "  Invalid choice.\n" CLR_RESET); return; }

    qsort(db->records, (size_t)db->count, sizeof(Contact), fns[ch]);
    log_action("SORT", "records re-sorted");
    printf(CLR_GREEN "  ✔ Contacts sorted.\n" CLR_RESET);
    db_list(db);
}

/* ═══════════════════════════════════════════════════════════
 *  FLAT-FILE EXPORT / IMPORT
 * ═══════════════════════════════════════════════════════════ */

void db_export(const Database *db) {
    FILE *f = fopen(EXPORT_FILE, "w");
    if (!f) { perror("fopen export"); return; }

    time_t t = time(NULL);
    fprintf(f, "# Contact Database Export\n");
    fprintf(f, "# Generated : %s", ctime(&t));
    fprintf(f, "# Records   : %d\n", db->count);
    fprintf(f, "# Format    : ID|FirstName|LastName|Phone|Email|Company|City|Category|Favourite\n");
    fprintf(f, "#─────────────────────────────────────────────────────────────────────────────\n\n");

    for (int i = 0; i < db->count; i++) {
        const Contact *c = &db->records[i];
        fprintf(f, "%d|%s|%s|%s|%s|%s|%s|%s|%d\n",
                c->id, c->first_name, c->last_name,
                c->phone, c->email, c->company,
                c->city, c->category, c->is_favourite);
    }

    fclose(f);
    log_action("EXPORT", EXPORT_FILE);
    printf(CLR_GREEN "  ✔ Exported %d contact(s) → %s\n" CLR_RESET, db->count, EXPORT_FILE);
}

void db_import(Database *db) {
    FILE *f = fopen(EXPORT_FILE, "r");
    if (!f) {
        printf(CLR_RED "  Cannot open %s for import.\n" CLR_RESET, EXPORT_FILE);
        return;
    }

    char line[512];
    int imported = 0;

    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        trim_newline(line);

        if (db->count == db->capacity) db_grow(db);
        Contact *c = &db->records[db->count];
        memset(c, 0, sizeof(Contact));

        if (sscanf(line, "%d|%63[^|]|%63[^|]|%19[^|]|%79[^|]|%63[^|]|%63[^|]|%63[^|]|%d",
                   &c->id, c->first_name, c->last_name,
                   c->phone, c->email, c->company,
                   c->city, c->category, &c->is_favourite) >= 1) {
            if (c->id >= db->next_id) db->next_id = c->id + 1;
            db->count++;
            imported++;
        }
    }
    fclose(f);

    char detail[64];
    snprintf(detail, sizeof(detail), "%d records from %s", imported, EXPORT_FILE);
    log_action("IMPORT", detail);
    printf(CLR_GREEN "  ✔ Imported %d contact(s) from %s\n" CLR_RESET, imported, EXPORT_FILE);
}

/* ═══════════════════════════════════════════════════════════
 *  MEMORY DIAGNOSTICS
 * ═══════════════════════════════════════════════════════════ */

void db_mem_info(const Database *db) {
    size_t used  = (size_t)db->count    * sizeof(Contact);
    size_t alloc = (size_t)db->capacity * sizeof(Contact);
    double frag  = db->capacity > 0
                   ? 100.0 * (db->capacity - db->count) / db->capacity
                   : 0.0;

    printf("\n" CLR_BOLD CLR_CYAN "  ── Heap Memory Diagnostics ──\n" CLR_RESET);
    printf("  sizeof(Contact)    : %zu bytes\n", sizeof(Contact));
    printf("  sizeof(Database)   : %zu bytes\n", sizeof(Database));
    printf("  Records stored     : %d\n",   db->count);
    printf("  Heap slots alloc'd : %d\n",   db->capacity);
    printf("  Bytes in use       : %zu B  (%.2f KB)\n", used,  used  / 1024.0);
    printf("  Bytes allocated    : %zu B  (%.2f KB)\n", alloc, alloc / 1024.0);
    printf("  Slack (unused)     : %zu B  (%.1f%% overhead)\n",
           alloc - used, frag);
    printf("  Next ID            : %d\n", db->next_id);
    hr();
}

/* ═══════════════════════════════════════════════════════════
 *  MAIN MENU
 * ═══════════════════════════════════════════════════════════ */

static void print_banner(void) {
    printf(CLR_BOLD CLR_CYAN);
    printf("\n");
    printf("  ╔══════════════════════════════════════════════════╗\n");
    printf("  ║     HEAP-ALLOCATED CONTACT DATABASE MANAGER      ║\n");
    printf("  ║      Dynamic Memory  ·  Search  ·  Sort  ·  I/O  ║\n");
    printf("  ╚══════════════════════════════════════════════════╝\n");
    printf(CLR_RESET "\n");
}

static void print_menu(const Database *db) {
    printf(CLR_BOLD "\n  ── Main Menu  [%d contact(s)] ──\n" CLR_RESET, db->count);
    printf("  [1] List all contacts\n");
    printf("  [2] Add contact\n");
    printf("  [3] View contact (by ID)\n");
    printf("  [4] Edit contact\n");
    printf("  [5] Delete contact\n");
    printf("  [6] Search contacts\n");
    printf("  [7] Sort contacts\n");
    printf("  [8] Export to file  (%s)\n", EXPORT_FILE);
    printf("  [9] Import from file\n");
    printf("  [M] Memory diagnostics\n");
    printf("  [0] Exit\n");
    printf("\n  Choice: ");
    fflush(stdout);
}

int main(void) {
    print_banner();

    /* ── Allocate the database on the heap ── */
    Database *db = db_create();
    printf(CLR_YELLOW "  [MEM] Database initialised: %d heap slots (%zu bytes)\n\n" CLR_RESET,
           db->capacity, (size_t)(db->capacity * sizeof(Contact)));

    log_action("START", "Database manager launched");

    char input[8];
    int running = 1;

    while (running) {
        print_menu(db);

        if (!fgets(input, sizeof(input), stdin)) break;
        trim_newline(input);

        printf("\n");

        switch (input[0]) {
            case '1': db_list(db);        break;
            case '2': db_add(db);         break;
            case '3': db_view_by_id(db);  break;
            case '4': db_edit(db);        break;
            case '5': db_delete(db);      break;
            case '6': db_search_menu(db); break;
            case '7': db_sort_menu(db);   break;
            case '8': db_export(db);      break;
            case '9': db_import(db);      break;
            case 'm': case 'M': db_mem_info(db); break;
            case '0':
                running = 0;
                printf(CLR_GREEN "  Goodbye! Freeing heap memory…\n" CLR_RESET);
                break;
            default:
                printf(CLR_RED "  Unknown option '%s'.\n" CLR_RESET, input);
        }
    }

    log_action("EXIT", "Database manager closed");

    /* ── Free all heap memory ── */
    db_destroy(db);
    printf(CLR_YELLOW "  [MEM] All heap memory freed. Bye!\n\n" CLR_RESET);

    return EXIT_SUCCESS;
}