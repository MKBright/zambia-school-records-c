#include "storage.h"
#include "student.h"

#include <stdio.h>
#include <string.h>

/*
 * Checks for the register rules and the CSV file.
 * Run with: make test
 *
 * These are ordinary asserts written out by hand so the expected numbers
 * stay visible. Nothing here talks to the menu; tests/e2e.sh does that.
 */

static int g_passed = 0;
static int g_failed = 0;

static void expect_true(int condition, const char *message) {
    if (condition) {
        g_passed++;
        return;
    }
    g_failed++;
    fprintf(stderr, "FAIL: %s\n", message);
}

static void expect_int(int got, int want, const char *message) {
    if (got == want) {
        g_passed++;
        return;
    }
    g_failed++;
    fprintf(stderr, "FAIL: %s (got %d, want %d)\n", message, got, want);
}

static void expect_str(const char *got, const char *want, const char *message) {
    if (got != NULL && want != NULL && strcmp(got, want) == 0) {
        g_passed++;
        return;
    }
    g_failed++;
    fprintf(stderr, "FAIL: %s (got \"%s\", want \"%s\")\n",
            message,
            got == NULL ? "(null)" : got,
            want == NULL ? "(null)" : want);
}

static void make_student(Student *s, const char *number, const char *name,
                         int grade, const char *class_name, char gender,
                         const char *phone) {
    student_clear(s);
    copy_str(s->learner_no, MAX_LEARNER_NO, number);
    copy_str(s->name, MAX_NAME, name);
    s->grade = grade;
    copy_str(s->class_name, MAX_CLASS, class_name);
    s->gender = gender;
    copy_str(s->guardian_phone, MAX_PHONE, phone);
}

static int add_student(Register *reg, const Student *s, char *error, int error_size) {
    return register_add(reg, s, error, error_size) == 0;
}

static int position_named(const Register *reg, const ResultRow *rows, int n,
                          const char *name) {
    int i;
    for (i = 0; i < n; i++) {
        if (strcmp(reg->items[rows[i].index].name, name) == 0) {
            return rows[i].position;
        }
    }
    return -1;
}

static void test_grade_bands(void) {
    static const int marks[] = {
        100, 75, 74, 70, 69, 65, 64, 60, 59, 55, 54, 50,
        49, 45, 44, 40, 39, 0
    };
    static const int bands[] = {
        1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
        7, 7, 8, 8, 9, 9
    };
    int i;

    for (i = 0; i < 18; i++) {
        expect_int(ecz_grade_band(marks[i]), bands[i], "ECZ-style band boundary");
    }
    expect_int(ecz_grade_band(-1), 0, "missing mark is not a band");
    expect_int(ecz_grade_band(101), 0, "mark above 100 is rejected");
    expect_str(ecz_descriptor(1), "Distinction", "band 1 name");
    expect_str(ecz_descriptor(2), "Distinction", "band 2 name");
    expect_str(ecz_descriptor(3), "Merit", "band 3 name");
    expect_str(ecz_descriptor(6), "Credit", "band 6 name");
    expect_str(ecz_descriptor(8), "Satisfactory", "band 8 name");
    expect_str(ecz_descriptor(9), "Unsatisfactory", "band 9 name");
    expect_str(ecz_descriptor(0), "No grade", "missing band name");
}

static void test_averages(void) {
    Student s;
    int counted = -1;
    char text[8];

    /* 653/8 = 81.625 -> 81.6 on the sheet, and 82 as the whole-mark band. */
    expect_int(average_tenths(653, 8), 816, "Thandiwe tenths");
    expect_int(rounded_average_mark(653, 8), 82, "Thandiwe whole mark");
    expect_int(ecz_grade_band(rounded_average_mark(653, 8)), 1, "Thandiwe band");
    expect_int(average_tenths(555, 8), 694, "tied average tenths");
    expect_int(rounded_average_mark(386, 8), 48, "Joseph whole mark");
    expect_int(average_tenths(0, 0), -1, "no marks have no average");

    format_tenths(816, text, (int)sizeof text);
    expect_str(text, "81.6", "format 81.6");
    format_tenths(1000, text, (int)sizeof text);
    expect_str(text, "100.0", "format 100.0");
    format_tenths(50, text, (int)sizeof text);
    expect_str(text, "5.0", "format 5.0");

    make_student(&s, "1", "Zero Mark", 10, "A", 'M', "0971000001");
    s.marks[0][0] = 0;
    s.marks[0][1] = 10;
    expect_int(marks_total(&s, 0, &counted), 10, "zero is a real mark, blank is not");
    expect_int(counted, 2, "blank subjects are not counted");
    counted = 5;
    expect_int(marks_total(&s, 1, &counted), 0, "empty term total");
    expect_int(counted, 0, "empty term count");
}

static void test_validation_and_search(void) {
    Register reg;
    Student s;
    char error[256];
    int matches[8];
    int index;

    register_init(&reg);
    make_student(&s, "abc", "Luyando Banda", 10, "A", 'M', "0971 000 001");
    expect_true(add_student(&reg, &s, error, (int)sizeof error), "add a learner");
    expect_str(reg.items[0].guardian_phone, "0971000001", "phone spaces are removed");

    make_student(&s, "ABC", "Someone Else", 10, "A", 'F', "0971000002");
    expect_true(!add_student(&reg, &s, error, (int)sizeof error), "numbers are unique ignoring case");
    expect_str(error, "That learner number is already in the register.", "duplicate message");

    make_student(&s, "new-1", "Mwansa Phiri", 7, "A", 'F', "0971000002");
    expect_true(!add_student(&reg, &s, error, (int)sizeof error), "grade 7 is rejected");

    make_student(&s, "new-2", "Mwansa Phiri", 10, "A", 'F', "123");
    expect_true(!add_student(&reg, &s, error, (int)sizeof error), "short phone is rejected");

    make_student(&s, "new-3", "Mwansa Phiri", 10, "A", 'F', "0971000003");
    s.marks[0][0] = 101;
    expect_true(!add_student(&reg, &s, error, (int)sizeof error), "mark 101 is rejected");

    make_student(&s, "new-4", "Chipo Kalaba", 8, "North", 'f', "+260971234567");
    expect_true(add_student(&reg, &s, error, (int)sizeof error), "gender f and +260 phone");
    index = register_find(&reg, "NEW-4");
    expect_true(index >= 0, "find ignores case");
    expect_int(reg.items[index].gender, 'F', "gender stored as F");

    expect_int(register_search(&reg, "  BANDA ", matches, 8), 1, "search trims and ignores case");
    expect_int(matches[0], 0, "search returns Banda");
    expect_int(register_search(&reg, "", matches, 8), 0, "empty search matches nobody");
    expect_int(register_search(&reg, "zzz", matches, 8), 0, "unknown name matches nobody");

    expect_true(register_delete(&reg, 0, error, (int)sizeof error) == 0, "delete Banda");
    expect_int(register_find(&reg, "abc"), -1, "deleted number is gone");
    expect_int(reg.count, 1, "the other learner stays");
}

static void test_positions(void) {
    Register reg;
    Student s;
    ResultRow rows[8];
    int n;
    char error[128];

    register_init(&reg);

    /* Same average for Banda and Phiri. Zulu is lower. Aardvark has no marks. */
    make_student(&s, "1", "Phiri", 10, "A", 'F', "0971000001");
    s.marks[0][0] = 50;
    expect_true(add_student(&reg, &s, error, (int)sizeof error), "add Phiri");

    make_student(&s, "2", "Banda", 10, "A", 'M', "0971000002");
    s.marks[0][0] = 50;
    expect_true(add_student(&reg, &s, error, (int)sizeof error), "add Banda");

    make_student(&s, "3", "Zulu", 10, "A", 'M', "0971000003");
    s.marks[0][0] = 40;
    expect_true(add_student(&reg, &s, error, (int)sizeof error), "add Zulu");

    make_student(&s, "4", "Aardvark", 10, "A", 'F', "0971000004");
    expect_true(add_student(&reg, &s, error, (int)sizeof error), "add learner with no marks");

    make_student(&s, "5", "Other Class", 10, "B", 'M', "0971000005");
    s.marks[0][0] = 99;
    expect_true(add_student(&reg, &s, error, (int)sizeof error), "add another class");

    n = build_class_results(&reg, 10, "a", 0, rows, 8);
    expect_int(n, 4, "class match ignores case and skips class B");
    expect_int(position_named(&reg, rows, n, "Banda"), 1, "tie keeps position 1");
    expect_int(position_named(&reg, rows, n, "Phiri"), 1, "second tie keeps position 1");
    expect_int(position_named(&reg, rows, n, "Zulu"), 3, "next position is skipped to 3");
    expect_int(position_named(&reg, rows, n, "Aardvark"), 0, "no marks means not ranked");
    expect_str(reg.items[rows[n - 1].index].name, "Aardvark", "unranked learner sorts last");
}

static int read_file(const char *path, char *buf, int cap) {
    FILE *fp;
    size_t n;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }
    n = fread(buf, 1, (size_t)cap, fp);
    fclose(fp);
    if (n == (size_t)cap) {
        return -1;
    }
    buf[n] = '\0';
    return (int)n;
}

static int write_file(const char *path, const void *buf, int n) {
    FILE *fp;
    size_t wrote;

    fp = fopen(path, "wb");
    if (fp == NULL) {
        return 0;
    }
    wrote = fwrite(buf, 1, (size_t)n, fp);
    if (fclose(fp) == EOF || wrote != (size_t)n) {
        return 0;
    }
    return 1;
}

static int file_contains(const char *path, const char *needle) {
    char buf[8192];
    int n = read_file(path, buf, (int)sizeof buf - 1);
    if (n < 0) {
        return 0;
    }
    return strstr(buf, needle) != NULL;
}

static void test_csv_round_trip(void) {
    Register reg;
    Register loaded;
    Student s;
    char error[256];
    const char *path = "build/round_trip.csv";
    int index;
    int counted = 0;

    register_init(&reg);
    make_student(&s, "Q-1", "Banda, Luyando \"Luy\"", 11, "North", 'M', "0971000099");
    s.marks[0][0] = 0;
    s.marks[0][1] = 80;
    s.marks[2][7] = 15;
    expect_true(add_student(&reg, &s, error, (int)sizeof error), "add a quoted name");
    expect_true(save_register_csv(&reg, path, error, (int)sizeof error) == STORAGE_OK,
                "save csv");
    expect_true(file_contains(path, "\"Banda, Luyando \"\"Luy\"\"\""),
                "comma and quote are escaped for Excel");

    register_init(&loaded);
    make_student(&s, "keep", "Keep Me", 10, "A", 'F', "0971000008");
    expect_true(add_student(&loaded, &s, error, (int)sizeof error), "seed register before load");
    expect_true(load_register_csv(&loaded, path, error, (int)sizeof error) == STORAGE_OK,
                "load csv");
    expect_int(loaded.count, 1, "load replaces memory");
    index = register_find(&loaded, "Q-1");
    expect_true(index >= 0, "loaded learner number");
    expect_str(loaded.items[index].name, "Banda, Luyando \"Luy\"", "name round trip");
    expect_int(loaded.items[index].grade, 11, "grade round trip");
    expect_str(loaded.items[index].class_name, "North", "class round trip");
    expect_int(loaded.items[index].marks[0][0], 0, "a zero mark is not dropped");
    expect_int(loaded.items[index].marks[0][2], MARK_NONE, "blank mark stays blank");
    expect_int(marks_total(&loaded.items[index], 0, &counted), 80, "term total after load");
    expect_int(counted, 2, "two marks in term 1");
    expect_int(loaded.items[index].marks[2][7], 15, "term 3 mark round trip");
}

static void test_load_keeps_old_data_on_failure(void) {
    Register reg;
    Student s;
    char error[256];
    const char *bad = "build/bad_register.csv";
    const char *missing = "build/does_not_exist.csv";

    register_init(&reg);
    make_student(&s, "safe", "Safe Learner", 10, "A", 'M', "0971000001");
    expect_true(add_student(&reg, &s, error, (int)sizeof error), "seed before bad load");
    {
        const char *bad_text = "this,is,not,the,header\n";
        expect_true(write_file(bad, bad_text, (int)strlen(bad_text)), "write a bad csv");
    }
    expect_true(load_register_csv(&reg, bad, error, (int)sizeof error) == STORAGE_ERR,
                "bad csv fails");
    expect_int(reg.count, 1, "failed load does not wipe memory");
    expect_str(reg.items[0].name, "Safe Learner", "original learner still there");
    expect_int(load_register_csv(&reg, missing, error, (int)sizeof error),
               STORAGE_NOT_FOUND, "missing file is reported");
    expect_int(reg.count, 1, "missing file does not wipe memory");
}

static void test_bom_crlf_and_blank_line(void) {
    Register reg;
    Register loaded;
    Student s;
    char error[256];
    char original[4096];
    char rewritten[4200];
    int n;
    int i;
    int out_n = 3;
    int header_done = 0;
    int rc;
    const char *plain = "build/plain.csv";
    const char *fancy = "build/fancy.csv";

    register_init(&reg);
    register_init(&loaded);
    make_student(&s, "B-1", "Bupe Ngoma", 10, "B", 'F', "0971000009");
    s.marks[0][0] = 55;
    expect_true(add_student(&reg, &s, error, (int)sizeof error), "add Bupe");
    expect_true(save_register_csv(&reg, plain, error, (int)sizeof error) == STORAGE_OK,
                "save plain csv");
    n = read_file(plain, original, (int)sizeof original - 1);
    expect_true(n > 0, "read plain csv back");

    /*
     * Excel on Windows often rewrites a CSV with a UTF-8 BOM and CRLF
     * line endings. A blank line should be ignored, not treated as a learner.
     */
    rewritten[0] = (char)0xEF;
    rewritten[1] = (char)0xBB;
    rewritten[2] = (char)0xBF;
    for (i = 0; i < n && out_n < (int)sizeof rewritten - 4; i++) {
        if (original[i] == '\n') {
            rewritten[out_n++] = '\r';
            rewritten[out_n++] = '\n';
            if (!header_done) {
                rewritten[out_n++] = '\r';
                rewritten[out_n++] = '\n';
                header_done = 1;
            }
        } else {
            rewritten[out_n++] = original[i];
        }
    }
    expect_true(write_file(fancy, rewritten, out_n), "write bom file");
    error[0] = '\0';
    rc = load_register_csv(&loaded, fancy, error, (int)sizeof error);
    if (rc != STORAGE_OK) {
        fprintf(stderr, "FAIL: load BOM, CRLF and a blank line (%s)\n", error);
        g_failed++;
        return;
    }
    g_passed++;
    expect_int(loaded.count, 1, "fancy file has one learner");
    expect_str(loaded.items[0].name, "Bupe Ngoma", "name from fancy file");
    expect_int(loaded.items[0].marks[0][0], 55, "mark from fancy file");
}

static void test_capacity(void) {
    Register reg;
    Student s;
    char error[128];
    char number[32];
    int i;

    register_init(&reg);
    for (i = 0; i < MAX_STUDENTS; i++) {
        snprintf(number, sizeof number, "N%d", i);
        make_student(&s, number, "Learner", 10, "A", 'M', "0971000001");
        if (!add_student(&reg, &s, error, (int)sizeof error)) {
            fprintf(stderr, "FAIL: filling the register stopped early: %s\n", error);
            g_failed++;
            return;
        }
    }
    g_passed++;
    expect_int(reg.count, MAX_STUDENTS, "register holds 250 learners");
    make_student(&s, "overflow", "One More", 10, "A", 'F', "0971000002");
    expect_true(!add_student(&reg, &s, error, (int)sizeof error), "251st learner is rejected");
    expect_str(error, "The register is full (250 learners).", "full-register message");
    expect_int(reg.count, MAX_STUDENTS, "count stays at the limit");
}

static int first_line(const char *path, char *buf, int cap) {
    FILE *fp;

    fp = fopen(path, "r");
    if (fp == NULL) {
        return 0;
    }
    if (fgets(buf, cap, fp) == NULL) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return 1;
}

static void test_sample_file(void) {
    Register reg;
    Register empty;
    char error[256];
    char sample_header[1024];
    char saved_header[1024];
    ResultRow rows[MAX_STUDENTS];
    int n;
    int index;
    int counted = 0;
    int sum_tenths = 0;
    int ranked = 0;
    int i;
    const char *sample = "data/sample_learners.csv";
    const char *empty_path = "build/empty_register.csv";

    register_init(&reg);
    register_init(&empty);
    expect_true(save_register_csv(&empty, empty_path, error, (int)sizeof error) == STORAGE_OK,
                "save an empty register");
    expect_true(first_line(sample, sample_header, (int)sizeof sample_header),
                "read sample header");
    expect_true(first_line(empty_path, saved_header, (int)sizeof saved_header),
                "read empty header");
    {
        char *sample_nl = strchr(sample_header, '\n');
        char *saved_nl = strchr(saved_header, '\n');
        expect_true(sample_nl != NULL && saved_nl != NULL, "headers end with a newline");
        if (sample_nl != NULL) {
            *sample_nl = '\0';
        }
        if (saved_nl != NULL) {
            *saved_nl = '\0';
        }
        expect_str(sample_header, saved_header, "sample header matches the program");
    }

    error[0] = '\0';
    expect_true(load_register_csv(&reg, sample, error, (int)sizeof error) == STORAGE_OK,
                "load sample register");
    if (reg.count == 0) {
        fprintf(stderr, "  sample load said: %s\n", error);
    }
    expect_int(reg.count, 11, "sample has 11 fictional learners");

    index = register_find(&reg, "2410-004");
    expect_true(index >= 0, "find Thandiwe");
    if (index >= 0) {
        expect_int(marks_total(&reg.items[index], 0, &counted), 653, "Thandiwe term 1 total");
        expect_int(counted, 8, "Thandiwe sat all 8 subjects");
        expect_int(marks_total(&reg.items[index], 1, &counted), 673, "Thandiwe term 2 total");
    }

    index = register_find(&reg, "2410-009");
    expect_true(index >= 0, "find Bupe");
    if (index >= 0) {
        expect_int(marks_total(&reg.items[index], 0, &counted), 406, "Bupe total skips the blank");
        expect_int(counted, 7, "Bupe has one blank subject");
    }

    index = register_find(&reg, "2408-001");
    expect_true(index >= 0, "find Chipo");
    if (index >= 0) {
        expect_int(reg.items[index].grade, 8, "Chipo is in Grade 8");
    }
    index = register_find(&reg, "2412-001");
    expect_true(index >= 0, "find Blessings");
    if (index >= 0) {
        expect_int(reg.items[index].grade, 12, "Blessings is in Grade 12");
    }

    n = build_class_results(&reg, 10, "A", 0, rows, MAX_STUDENTS);
    expect_int(n, 6, "Grade 10A has 6 learners");
    expect_int(position_named(&reg, rows, n, "Thandiwe Zulu"), 1, "Thandiwe is first");
    expect_int(position_named(&reg, rows, n, "Luyando Banda"), 2, "Luyando is second");
    expect_int(position_named(&reg, rows, n, "Chanda Mwale"), 3, "Chanda shares third");
    expect_int(position_named(&reg, rows, n, "Mwansa Phiri"), 3, "Mwansa shares third");
    expect_int(position_named(&reg, rows, n, "Kondwani Tembo"), 5, "Kondwani is fifth");
    expect_int(position_named(&reg, rows, n, "Joseph Mulenga"), 6, "Joseph is sixth");

    for (i = 0; i < n; i++) {
        if (rows[i].counted > 0) {
            sum_tenths += average_tenths(rows[i].total, rows[i].counted);
            ranked++;
        }
    }
    expect_int(ranked, 6, "everyone in 10A term 1 is ranked");
    expect_int((sum_tenths + ranked / 2) / ranked, 675, "class average is 67.5");

    n = build_class_results(&reg, 10, "B", 0, rows, MAX_STUDENTS);
    expect_int(n, 3, "Grade 10B has 3 learners");
    expect_int(position_named(&reg, rows, n, "Natasha Daka"), 1, "Natasha leads 10B");
    expect_int(position_named(&reg, rows, n, "Bupe Ngoma"), 3, "Bupe is third in 10B");
}

int main(void) {
    test_grade_bands();
    test_averages();
    test_validation_and_search();
    test_positions();
    test_csv_round_trip();
    test_load_keeps_old_data_on_failure();
    test_bom_crlf_and_blank_line();
    test_capacity();
    test_sample_file();

    if (g_failed != 0) {
        fprintf(stderr, "%d checks failed, %d passed.\n", g_failed, g_passed);
        return 1;
    }
    printf("All %d checks passed.\n", g_passed);
    return 0;
}
