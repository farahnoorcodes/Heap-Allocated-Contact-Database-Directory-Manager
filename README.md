# Heap-Allocated Contact Database Directory Manager

A memory-efficient console application for managing a contact directory using **manual heap allocation** (`malloc`, `realloc`, `free`). The database grows and shrinks dynamically at runtime — no fixed-size arrays, no static limits.

---

## Features

- **Custom data structures** — contacts modeled with `struct`, stored in a heap-allocated dynamic array
- **Dynamic memory management** — array doubles via `realloc` when full, shrinks when usage drops below 25%
- **Search** — case-insensitive substring match across all fields (name, phone, email, company, city, category)
- **Custom sorting** — 7 sort modes via `qsort` with hand-written comparators (last name, first name, ID, company, city, favourites-first)
- **Flat-file I/O** — export/import contacts to a pipe-delimited text file; every action is timestamped to an audit log
- **Memory diagnostics** — live view of heap usage, slot count, and allocation overhead

---

## Requirements

- GCC (or any C11-compliant compiler)
- POSIX-compatible system (Linux/macOS) — uses `strcasecmp` and ANSI color codes
  - On Windows, build with **WSL** or **MinGW + a POSIX layer**

---

## Build

```bash
gcc -Wall -Wextra -pedantic -std=c11 -o contact_db contact_db.c
```

Compiles with **zero warnings** under strict flags.

## Run

```bash
./contact_db
```

---

## Usage

On launch you'll see a menu:

```
[1] List all contacts
[2] Add contact
[3] View contact (by ID)
[4] Edit contact
[5] Delete contact
[6] Search contacts
[7] Sort contacts
[8] Export to file  (contacts_export.txt)
[9] Import from file
[M] Memory diagnostics
[0] Exit
```

Enter the number (or `M`) and follow the prompts.

### Adding a contact
You'll be asked for first/last name, phone, email, company, city, category, and whether to mark it a favourite (`y`/`n`). Each contact is auto-assigned an incrementing ID.

### Editing a contact
Enter the ID to edit, then press **Enter** on any field to keep its current value, or type a new one to overwrite it.

### Searching
Type any text — it's matched (case-insensitively) against every field at once.

### Sorting
Pick from 7 strategies, including a "favourites first" composite sort.

### Export / Import
- **Export** writes all contacts to `contacts_export.txt` in a pipe-delimited format with a header.
- **Import** reads that same file back in, growing the heap array as needed and continuing the ID sequence from the highest imported ID.

### Memory Diagnostics (`M`)
Shows `sizeof` for each struct, how many heap slots are allocated vs. used, total bytes allocated, and the percentage of unused ("slack") memory — useful for understanding how the doubling/shrinking strategy behaves.

---

## Generated Files

| File | Purpose |
|---|---|
| `contacts_export.txt` | Flat-file export/import of all contact records |
| `contacts.log` | Timestamped audit trail of every ADD / EDIT / DELETE / SEARCH / SORT / EXPORT / IMPORT action |

Both are created in the working directory the program is run from.

---

## Code Structure

| Section | Contents |
|---|---|
| **Data structures** | `Contact`, `Database`, `SearchResult` |
| **Heap management** | `db_create`, `db_grow`, `db_destroy`, `sr_destroy` |
| **Utilities** | string trimming, case-insensitive substring match, safe field input |
| **Display** | table and full-record formatted printing |
| **CRUD** | `db_add`, `db_list`, `db_view_by_id`, `db_edit`, `db_delete` |
| **Search** | `db_search`, `db_search_menu` |
| **Sorting** | 7 `qsort` comparators + `db_sort_menu` |
| **File I/O** | `db_export`, `db_import`, `log_action` |
| **Diagnostics** | `db_mem_info` |
| **Entry point** | `main()` — menu loop |

---

## Design Notes

- **Growth strategy**: array capacity doubles (4 → 8 → 16 …) on overflow, amortizing `realloc` cost to O(1) per insert on average.
- **Shrink strategy**: capacity halves after deletions once usage falls to ≤25%, preventing unbounded memory retention — but never below the initial 4-slot floor.
- **No memory leaks**: every `malloc`/`realloc`'d block (`Database`, `records`, `SearchResult`, `matches`) has a matching `free`, called either immediately after use (search results) or at program exit (`db_destroy`).
- **Deletion** uses `memmove` to compact the array in place rather than leaving gaps, keeping the structure dense and sort/search-friendly.

