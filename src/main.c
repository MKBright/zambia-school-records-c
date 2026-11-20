#include "storage.h"
#include "student.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/*
 * Menu for the offline student register.
 *
 * Input is read with fgets, not scanf. Names have spaces, and scanf would
 * stop at the first one and leave the rest of the line sitting in the
 * buffer for the next question. fgets takes the whole line.
 *
 * Prompts are flushed before each read. When the test script pipes commands
 * in, stdout is block-buffered, so a prompt with no newline would otherwise
 * sit there unseen while the program waits.
 *
 * The register stays in memory until Save (or the exit question). That way
 * a mistake does not overwrite the CSV until someone asks it to.
 */

#define SCHOOL_NAME "Riverside Day Secondary School"
#define SCHOOL_TOWN "Lusaka"

static int read_line(const char *prompt, char *buf, int size) {
    for (;;) {
        size_t n;

        if (prompt != NULL && prompt[0] != '\0') {
            fputs(prompt, stdout);
            fflush(stdout);
        }
        if (fgets(buf, size, stdin) == NULL) {
            buf[0] = '\0';
            return 0;
        }
        n = strlen(buf);
        if (n > 0 && buf[n - 1] == '\n') {
            buf[n - 1] = '\0';
            if (n > 1 && buf[n - 2] == '\r') {
                buf[n - 2] = '\0';
            }
            return 1;
        }
        /* The line did not fit. Drop the rest so the next read stays in step. */
        if (!feof(stdin)) {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) {
            }
            printf("That line is too long. Please try a shorter value.\n");
            continue;
        }
        return 1;
    }
}

static int read_yes_no(const char *prompt, int *yes) {
    char buf[32];

    for (;;) {
        if (!read_line(prompt, buf, (int)sizeof buf)) {
            return 0;
        }
        trim_inplace(buf);
        if (str_icmp(buf, "y") == 0 || str_icmp(buf, "yes") == 0) {
            *yes = 1;
            return 1;
        }
        if (str_icmp(buf, "n") == 0 || str_icmp(buf, "no") == 0) {
            *yes = 0;
            return 1;
        }
        printf("Please type y or n.\n");
    }
}

static int read_int_range(const char *prompt, int min, int max, int *out) {
    char buf[64];

    for (;;) {
        int value;

        if (!read_line(prompt, buf, (int)sizeof buf)) {
            return 0;
        }
        trim_inplace(buf);
        if (!parse_whole_number(buf, &value) || value < min || value > max) {
            printf("Enter a whole number from %d to %d.\n", min, max);
            continue;
        }
        *out = value;
        return 1;
    }
}

static int prompt_text(const char *prompt, char *dst, int dst_size) {
    char buf[160];

    for (;;) {
        if (!read_line(prompt, buf, (int)sizeof buf)) {
            return 0;
        }
        trim_inplace(buf);
        if (buf[0] == '\0') {
            printf("This value is required.\n");
            continue;
        }
        if (strlen(buf) > (size_t)(dst_size - 1)) {
            printf("That is too long (max %d characters).\n", dst_size - 1);
            continue;
        }
        copy_str(dst, dst_size, buf);
        return 1;
    }
}

/* Pressing Enter keeps the text already in dst. Returns 0 on end of input. */
static int prompt_keep(const char *label, char *dst, int dst_size) {
    char buf[160];
    char prompt[128];

    snprintf(prompt, sizeof prompt, "%.24s [%.60s]: ", label, dst);
    for (;;) {
        if (!read_line(prompt, buf, (int)sizeof buf)) {
            return 0;
        }
        trim_inplace(buf);
        if (buf[0] == '\0') {
            return 1;
        }
        if (strlen(buf) > (size_t)(dst_size - 1)) {
            printf("That is too long (max %d characters).\n", dst_size - 1);
            continue;
        }
        copy_str(dst, dst_size, buf);
        return 1;
    }
}

/* 1 = found, 0 = not in the register, -1 = end of input. */
static int prompt_existing(const Register *reg, int *index) {
    char buf[64];
    int found;

    if (!read_line("Learner number: ", buf, (int)sizeof buf)) {
        return -1;
    }
    trim_inplace(buf);
    found = register_find(reg, buf);
    if (found < 0) {
        printf("No learner with number \"%s\".\n", buf);
        return 0;
    }
    *index = found;
    return 1;
}

static void format_today(char *buf, int buf_size) {
    static const char *months[12] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    time_t now;
    struct tm *when;

    if (buf == NULL || buf_size <= 0) {
        return;
    }
    now = time(NULL);
    when = localtime(&now);
    if (when == NULL || when->tm_mon < 0 || when->tm_mon > 11) {
        copy_str(buf, buf_size, "today");
        return;
    }
    /* Worst case still fits: two long integers plus a 12-letter month. */
    snprintf(buf, (size_t)buf_size, "%02d %.12s %04d",
             when->tm_mday, months[when->tm_mon], when->tm_year + 1900);
}

static void print_learner_header(void) {
    printf("%-4s %-14s %-24s %-10s %-8s %-8s %s\n",
           "No.", "Learner No", "Name", "Grade", "Class", "Gender",
           "Guardian phone");
}

static void print_learner_line(const Student *s, int number) {
    char grade_text[24];

    snprintf(grade_text, sizeof grade_text, "Grade %d", s->grade);
    printf("%-4d %-14.14s %-24.24s %-10s %-8.8s %-8s %s\n",
           number,
           s->learner_no,
           s->name,
           grade_text,
           s->class_name,
           s->gender == 'M' ? "Male" : "Female",
           s->guardian_phone);
}

static void print_menu(const Register *reg, const char *path) {
    printf("\n");
    printf("============================================================\n");
    printf("  %s, %s\n", SCHOOL_NAME, SCHOOL_TOWN);
    printf("  Student register and term results (offline)\n");
    printf("============================================================\n");
    printf("  Learners in memory: %d\n", reg->count);
    printf("  File: %s\n", path);
    printf("\n");
    printf("   1   Add a learner\n");
    printf("   2   Edit a learner\n");
    printf("   3   Delete a learner\n");
    printf("   4   Search learners\n");
    printf("   5   List all learners\n");
    printf("   6   Record subject marks\n");
    printf("   7   View one learner's results\n");
    printf("   8   Class report and positions\n");
    printf("   9   Save to CSV\n");
    printf("  10   Load from CSV\n");
    printf("   0   Exit\n");
    printf("------------------------------------------------------------\n");
}

static int do_add(Register *reg, int *dirty) {
    Student s;
    char error[512];
    char phone_raw[160];

    student_clear(&s);
    if (!prompt_text("Learner number: ", s.learner_no, MAX_LEARNER_NO)) {
        return -1;
    }
    if (!prompt_text("Full name: ", s.name, MAX_NAME)) {
        return -1;
    }
    if (!read_int_range("Grade (8-12): ", MIN_GRADE, MAX_GRADE, &s.grade)) {
        return -1;
    }
    if (!prompt_text("Class (for example A): ", s.class_name, MAX_CLASS)) {
        return -1;
    }
    for (;;) {
        char buf[64];
        if (!read_line("Gender (M or F): ", buf, (int)sizeof buf)) {
            return -1;
        }
        if (parse_gender(buf, &s.gender)) {
            break;
        }
        printf("Gender must be M or F.\n");
    }
    for (;;) {
        if (!read_line("Guardian phone: ", phone_raw, (int)sizeof phone_raw)) {
            return -1;
        }
        trim_inplace(phone_raw);
        if (phone_raw[0] == '\0') {
            printf("Guardian phone is required.\n");
            continue;
        }
        if (prepare_phone(phone_raw, s.guardian_phone, MAX_PHONE) != 0) {
            printf("That phone number is too long.\n");
            continue;
        }
        break;
    }

    if (register_add(reg, &s, error, (int)sizeof error) != 0) {
        printf("%s\n", error);
        return 1;
    }
    *dirty = 1;
    {
        const Student *saved = &reg->items[reg->count - 1];
        printf("Added %s (%s). Use option 6 to record marks.\n",
               saved->name, saved->learner_no);
    }
    return 1;
}

static int do_edit(Register *reg, int *dirty) {
    int index;
    int found;
    Student edited;
    char error[512];
    char buf[160];

    found = prompt_existing(reg, &index);
    if (found <= 0) {
        return found;
    }
    edited = reg->items[index];
    printf("Editing %s (%s).\n", edited.name, edited.learner_no);
    printf("Press Enter to keep the value shown in brackets.\n");

    if (!prompt_keep("Learner number", edited.learner_no, MAX_LEARNER_NO)) {
        return -1;
    }
    if (!prompt_keep("Full name", edited.name, MAX_NAME)) {
        return -1;
    }

    snprintf(buf, sizeof buf, "Grade (8-12) [%d]: ", edited.grade);
    for (;;) {
        char typed[64];
        int grade;

        if (!read_line(buf, typed, (int)sizeof typed)) {
            return -1;
        }
        trim_inplace(typed);
        if (typed[0] == '\0') {
            break;
        }
        if (!parse_whole_number(typed, &grade) ||
            grade < MIN_GRADE || grade > MAX_GRADE) {
            printf("Enter a whole number from %d to %d.\n", MIN_GRADE, MAX_GRADE);
            continue;
        }
        edited.grade = grade;
        break;
    }

    if (!prompt_keep("Class", edited.class_name, MAX_CLASS)) {
        return -1;
    }

    snprintf(buf, sizeof buf, "Gender (M or F) [%c]: ", edited.gender);
    for (;;) {
        char typed[64];
        char gender;

        if (!read_line(buf, typed, (int)sizeof typed)) {
            return -1;
        }
        trim_inplace(typed);
        if (typed[0] == '\0') {
            break;
        }
        if (!parse_gender(typed, &gender)) {
            printf("Gender must be M or F.\n");
            continue;
        }
        edited.gender = gender;
        break;
    }

    snprintf(buf, sizeof buf, "Guardian phone [%.16s]: ", edited.guardian_phone);
    for (;;) {
        char typed[160];

        if (!read_line(buf, typed, (int)sizeof typed)) {
            return -1;
        }
        trim_inplace(typed);
        if (typed[0] == '\0') {
            break;
        }
        if (prepare_phone(typed, edited.guardian_phone, MAX_PHONE) != 0) {
            printf("That phone number is too long.\n");
            continue;
        }
        break;
    }

    if (student_equal(&edited, &reg->items[index])) {
        printf("No changes made.\n");
        return 1;
    }
    if (register_update(reg, index, &edited, error, (int)sizeof error) != 0) {
        printf("%s\n", error);
        return 1;
    }
    *dirty = 1;
    printf("Updated %s (%s).\n",
           reg->items[index].name, reg->items[index].learner_no);
    return 1;
}

static int do_delete(Register *reg, int *dirty) {
    int index;
    int found;
    char name[MAX_NAME];
    char typed[32];
    char error[256];

    found = prompt_existing(reg, &index);
    if (found <= 0) {
        return found;
    }
    copy_str(name, MAX_NAME, reg->items[index].name);
    printf("Delete %s (%s)?\n", name, reg->items[index].learner_no);
    if (!read_line("Type YES to delete this learner: ", typed, (int)sizeof typed)) {
        return -1;
    }
    trim_inplace(typed);
    if (strcmp(typed, "YES") != 0) {
        printf("Nothing was deleted.\n");
        return 1;
    }
    if (register_delete(reg, index, error, (int)sizeof error) != 0) {
        printf("%s\n", error);
        return 1;
    }
    *dirty = 1;
    printf("Deleted %s.\n", name);
    return 1;
}

static int do_search(const Register *reg) {
    char query[MAX_NAME];
    int matches[MAX_STUDENTS];
    int n;
    int i;

    if (!read_line("Name or learner number: ", query, (int)sizeof query)) {
        return -1;
    }
    trim_inplace(query);
    if (query[0] == '\0') {
        printf("Type a name or a learner number.\n");
        return 1;
    }
    n = register_search(reg, query, matches, MAX_STUDENTS);
    if (n == 0) {
        printf("No learner matched \"%s\".\n", query);
        return 1;
    }
    printf("%d match%s.\n", n, n == 1 ? "" : "es");
    print_learner_header();
    for (i = 0; i < n; i++) {
        print_learner_line(&reg->items[matches[i]], matches[i] + 1);
    }
    return 1;
}

static int do_list(const Register *reg) {
    int i;

    if (reg->count == 0) {
        printf("The register is empty. Choose 1 to add a learner.\n");
        return 1;
    }
    printf("%d learner%s.\n", reg->count, reg->count == 1 ? "" : "s");
    print_learner_header();
    for (i = 0; i < reg->count; i++) {
        print_learner_line(&reg->items[i], i + 1);
    }
    return 1;
}

static int do_marks(Register *reg, int *dirty) {
    int index;
    int found;
    int term = 0;
    int subject;
    int changed = 0;
    Student edited;
    char error[512];

    found = prompt_existing(reg, &index);
    if (found <= 0) {
        return found;
    }
    if (!read_int_range("Term (1-3): ", 1, NUM_TERMS, &term)) {
        return -1;
    }
    term -= 1;
    edited = reg->items[index];
    printf("Enter a mark from 0 to 100.\n");
    printf("Press Enter to keep the current mark, or type - to clear it.\n");

    for (subject = 0; subject < NUM_SUBJECTS; subject++) {
        char shown[16];
        char prompt[96];
        int mark = edited.marks[term][subject];

        if (mark == MARK_NONE) {
            copy_str(shown, (int)sizeof shown, "-");
        } else {
            snprintf(shown, sizeof shown, "%d", mark);
        }
        snprintf(prompt, sizeof prompt, "%.40s [%.4s]: ",
                 subject_name(subject), shown);
        for (;;) {
            char typed[32];
            int value;

            if (!read_line(prompt, typed, (int)sizeof typed)) {
                return -1;
            }
            trim_inplace(typed);
            if (typed[0] == '\0') {
                break;
            }
            if (strcmp(typed, "-") == 0) {
                edited.marks[term][subject] = MARK_NONE;
                break;
            }
            if (!parse_whole_number(typed, &value) || value < 0 || value > 100) {
                printf("Enter a mark from 0 to 100, or - to clear it.\n");
                continue;
            }
            edited.marks[term][subject] = value;
            break;
        }
        if (edited.marks[term][subject] != reg->items[index].marks[term][subject]) {
            changed = 1;
        }
    }

    if (!changed) {
        printf("No marks were changed.\n");
        return 1;
    }
    if (register_update(reg, index, &edited, error, (int)sizeof error) != 0) {
        printf("%s\n", error);
        return 1;
    }
    *dirty = 1;
    {
        int counted = 0;
        int total = marks_total(&reg->items[index], term, &counted);
        if (counted == 0) {
            printf("Marks cleared for term %d.\n", term + 1);
        } else {
            char avg[8];
            int band = ecz_grade_band(rounded_average_mark(total, counted));
            format_tenths(average_tenths(total, counted), avg, (int)sizeof avg);
            printf("Marks saved for term %d. Total: %d  Average: %s  Approx. grade: %d %s\n",
                   term + 1, total, avg, band, ecz_descriptor(band));
        }
    }
    return 1;
}

static int do_view(const Register *reg) {
    int index;
    int found;
    int term;
    const Student *s;

    found = prompt_existing(reg, &index);
    if (found <= 0) {
        return found;
    }
    s = &reg->items[index];
    printf("\n%s\n", s->name);
    printf("Learner number: %s\n", s->learner_no);
    printf("Grade %d class %s    Gender: %s    Guardian: %s\n",
           s->grade,
           s->class_name,
           s->gender == 'M' ? "Male" : "Female",
           s->guardian_phone);

    for (term = 0; term < NUM_TERMS; term++) {
        int counted = 0;
        int total = marks_total(s, term, &counted);
        int subject;

        printf("\nTerm %d\n", term + 1);
        if (counted == 0) {
            printf("  No marks entered for this term.\n");
            continue;
        }
        for (subject = 0; subject < NUM_SUBJECTS; subject++) {
            int mark = s->marks[term][subject];
            if (mark == MARK_NONE) {
                printf("  %-22s %4s\n", subject_name(subject), "-");
            } else {
                int band = ecz_grade_band(mark);
                printf("  %-22s %4d   %d %s\n",
                       subject_name(subject), mark, band, ecz_descriptor(band));
            }
        }
        {
            char avg[8];
            int band = ecz_grade_band(rounded_average_mark(total, counted));
            format_tenths(average_tenths(total, counted), avg, (int)sizeof avg);
            printf("  Total: %d    Average: %s    Approx. grade: %d %s\n",
                   total, avg, band, ecz_descriptor(band));
        }
    }
    printf("\nGrade bands are a school approximation of ECZ grades, not an official result.\n");
    return 1;
}

static int write_class_report(FILE *out, const Register *reg, int grade,
                              const char *class_name, int term_index,
                              const ResultRow *rows, int n) {
    int i;
    int subject;
    int ranked = 0;
    int sum_tenths = 0;
    char today[48];

    format_today(today, (int)sizeof today);
    fprintf(out, "%s, %s\n", SCHOOL_NAME, SCHOOL_TOWN);
    fprintf(out, "Class report\n");
    fprintf(out, "Grade %d class %s | Term %d\n", grade, class_name, term_index + 1);
    fprintf(out, "Printed: %s\n", today);
    fprintf(out, "------------------------------------------------------------\n");

    if (n == 0) {
        fprintf(out, "No learners in Grade %d class %s.\n", grade, class_name);
    } else {
        fprintf(out, "%-4s %-12s %-22s", "Pos", "Learner No", "Name");
        for (subject = 0; subject < NUM_SUBJECTS; subject++) {
            fprintf(out, " %4s", subject_short_name(subject));
        }
        fprintf(out, " %5s %6s  %s\n", "Total", "Avg", "Band");

        for (i = 0; i < n; i++) {
            const Student *s = &reg->items[rows[i].index];
            if (rows[i].position == 0) {
                fprintf(out, "%-4s ", "-");
            } else {
                fprintf(out, "%-4d ", rows[i].position);
            }
            fprintf(out, "%-12.12s %-22.22s", s->learner_no, s->name);
            for (subject = 0; subject < NUM_SUBJECTS; subject++) {
                int mark = s->marks[term_index][subject];
                if (mark == MARK_NONE) {
                    fprintf(out, " %4s", "-");
                } else {
                    fprintf(out, " %4d", mark);
                }
            }
            if (rows[i].counted == 0) {
                fprintf(out, " %5s %6s  %s\n", "-", "-", "not ranked");
            } else {
                char avg[8];
                int band = ecz_grade_band(
                    rounded_average_mark(rows[i].total, rows[i].counted));
                format_tenths(average_tenths(rows[i].total, rows[i].counted),
                              avg, (int)sizeof avg);
                fprintf(out, " %5d %6s  %d %s\n",
                        rows[i].total, avg, band, ecz_descriptor(band));
                ranked++;
                sum_tenths += average_tenths(rows[i].total, rows[i].counted);
            }
        }
        fprintf(out, "------------------------------------------------------------\n");
        fprintf(out, "Learners in this class: %d\n", n);
        fprintf(out, "Ranked (at least one mark): %d\n", ranked);
        if (ranked > 0) {
            char avg[8];
            int class_tenths = (sum_tenths + ranked / 2) / ranked;
            format_tenths(class_tenths, avg, (int)sizeof avg);
            fprintf(out, "Class average: %s\n", avg);
        }
    }

    fprintf(out, "\nSubjects:\n");
    for (subject = 0; subject < NUM_SUBJECTS; subject++) {
        fprintf(out, "  %-4s %s\n", subject_short_name(subject), subject_name(subject));
    }
    fprintf(out, "Total adds up the marks that were entered. Average divides that\n");
    fprintf(out, "total by how many subjects have a mark. A blank is not a zero.\n");
    fprintf(out, "Positions: the highest average is 1. A tie shares a position\n");
    fprintf(out, "and the next one is skipped (for example 1, 1, 3).\n");
    fprintf(out, "Grade bands approximate the ECZ 1-9 scale for school use.\n");
    fprintf(out, "They are not an official Examinations Council of Zambia result.\n");
    return ferror(out) ? -1 : 0;
}

static int do_report(const Register *reg) {
    int grade;
    int term;
    char class_name[MAX_CLASS];
    ResultRow rows[MAX_STUDENTS];
    int n;
    int yes;

    if (!read_int_range("Grade (8-12): ", MIN_GRADE, MAX_GRADE, &grade)) {
        return -1;
    }
    if (!prompt_text("Class: ", class_name, MAX_CLASS)) {
        return -1;
    }
    if (!read_int_range("Term (1-3): ", 1, NUM_TERMS, &term)) {
        return -1;
    }
    n = build_class_results(reg, grade, class_name, term - 1, rows, MAX_STUDENTS);
    printf("\n");
    if (write_class_report(stdout, reg, grade, class_name, term - 1, rows, n) != 0) {
        printf("Could not finish the report on screen.\n");
        return 1;
    }
    if (!read_yes_no("Save a text copy? (y/n): ", &yes)) {
        return -1;
    }
    if (!yes) {
        return 1;
    }
    {
        char path[CSV_PATH_MAX];
        FILE *fp;
        if (!read_line("Text file to write: ", path, (int)sizeof path)) {
            return -1;
        }
        trim_inplace(path);
        if (path[0] == '\0') {
            printf("No text file written.\n");
            return 1;
        }
        fp = fopen(path, "w");
        if (fp == NULL) {
            printf("Could not write %s (%s).\n", path, strerror(errno));
            return 1;
        }
        if (write_class_report(fp, reg, grade, class_name, term - 1, rows, n) != 0 ||
            fclose(fp) == EOF) {
            printf("Could not finish writing %s.\n", path);
            return 1;
        }
        printf("Wrote the report to %s.\n", path);
    }
    return 1;
}

static int do_save(const Register *reg, const char *path, int *dirty) {
    char error[512];

    if (save_register_csv(reg, path, error, (int)sizeof error) != STORAGE_OK) {
        printf("%s\n", error);
        return 1;
    }
    *dirty = 0;
    printf("Saved %d learner%s to %s.\n",
           reg->count, reg->count == 1 ? "" : "s", path);
    return 1;
}

static int do_load(Register *reg, char *path, int *dirty) {
    char prompt[240];
    char typed[CSV_PATH_MAX];
    char chosen[CSV_PATH_MAX];
    char error[512];
    int yes;
    int rc;

    snprintf(prompt, sizeof prompt, "CSV file to load [%.180s]: ", path);
    if (!read_line(prompt, typed, (int)sizeof typed)) {
        return -1;
    }
    trim_inplace(typed);
    if (typed[0] == '\0') {
        copy_str(chosen, CSV_PATH_MAX, path);
    } else {
        copy_str(chosen, CSV_PATH_MAX, typed);
    }
    if (*dirty) {
        if (!read_yes_no("Loading replaces learners in memory. Continue? (y/n): ",
                         &yes)) {
            return -1;
        }
        if (!yes) {
            printf("Load cancelled.\n");
            return 1;
        }
    }
    rc = load_register_csv(reg, chosen, error, (int)sizeof error);
    if (rc != STORAGE_OK) {
        printf("%s\n", error);
        printf("The learners already in memory were kept.\n");
        return 1;
    }
    copy_str(path, CSV_PATH_MAX, chosen);
    *dirty = 0;
    printf("Loaded %d learner%s from %s.\n",
           reg->count, reg->count == 1 ? "" : "s", path);
    return 1;
}

static int do_exit(Register *reg, const char *path, int *dirty) {
    int yes;
    char prompt[240];
    char error[512];

    if (!*dirty) {
        printf("Goodbye.\n");
        return 0;
    }
    snprintf(prompt, sizeof prompt,
             "Save changes to %.160s before exit? (y/n): ", path);
    if (!read_yes_no(prompt, &yes)) {
        return -1;
    }
    if (!yes) {
        printf("Changes were not saved. Goodbye.\n");
        return 0;
    }
    if (save_register_csv(reg, path, error, (int)sizeof error) != STORAGE_OK) {
        printf("%s\n", error);
        printf("Still in the register. Fix the problem, then exit again.\n");
        return 1;
    }
    *dirty = 0;
    printf("Saved %d learner%s to %s.\n",
           reg->count, reg->count == 1 ? "" : "s", path);
    printf("Goodbye.\n");
    return 0;
}

static void startup_load(Register *reg, const char *path) {
    char error[512];
    int rc;

    register_init(reg);
    rc = load_register_csv(reg, path, error, (int)sizeof error);
    if (rc == STORAGE_OK) {
        printf("Loaded %d learner%s from %s.\n",
               reg->count, reg->count == 1 ? "" : "s", path);
        return;
    }
    register_init(reg);
    if (rc == STORAGE_NOT_FOUND) {
        printf("No saved register at %s.\n", path);
        printf("Starting with an empty register. Add learners, then choose Save.\n");
        return;
    }
    printf("%s\n", error);
    printf("Starting with an empty register.\n");
}

int main(int argc, char *argv[]) {
    Register reg;
    char path[CSV_PATH_MAX];
    int dirty = 0;

    setvbuf(stdout, NULL, _IOLBF, 0);

    if (argc > 2) {
        fprintf(stderr, "Usage: %s [csv-file]\n", argv[0]);
        return 1;
    }
    if (argc == 2) {
        if (strlen(argv[1]) + 1 > (size_t)CSV_PATH_MAX) {
            fprintf(stderr, "That file path is too long.\n");
            return 1;
        }
        copy_str(path, CSV_PATH_MAX, argv[1]);
    } else {
        copy_str(path, CSV_PATH_MAX, "data/learners.csv");
    }

    startup_load(&reg, path);

    for (;;) {
        char buf[32];
        int choice;
        int action = 1;

        print_menu(&reg, path);
        if (!read_line("Choice: ", buf, (int)sizeof buf)) {
            printf("\n");
            if (dirty) {
                printf("Unsaved changes were not written.\n");
                return 1;
            }
            printf("Goodbye.\n");
            return 0;
        }
        trim_inplace(buf);
        if (buf[0] == '\0') {
            continue;
        }
        if (!parse_whole_number(buf, &choice)) {
            printf("Please enter a number from the menu.\n");
            continue;
        }

        switch (choice) {
        case 0:
            action = do_exit(&reg, path, &dirty);
            break;
        case 1:
            action = do_add(&reg, &dirty);
            break;
        case 2:
            action = do_edit(&reg, &dirty);
            break;
        case 3:
            action = do_delete(&reg, &dirty);
            break;
        case 4:
            action = do_search(&reg);
            break;
        case 5:
            action = do_list(&reg);
            break;
        case 6:
            action = do_marks(&reg, &dirty);
            break;
        case 7:
            action = do_view(&reg);
            break;
        case 8:
            action = do_report(&reg);
            break;
        case 9:
            action = do_save(&reg, path, &dirty);
            break;
        case 10:
            action = do_load(&reg, path, &dirty);
            break;
        default:
            printf("That choice is not on the menu.\n");
            break;
        }

        if (action == 0) {
            return 0;
        }
        if (action < 0) {
            printf("\n");
            if (dirty) {
                printf("Unsaved changes were not written.\n");
                return 1;
            }
            printf("Goodbye.\n");
            return 0;
        }
    }
}
