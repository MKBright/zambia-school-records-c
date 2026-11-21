#include "student.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

/*
 * Rules for learner details, totals, and ECZ-style grade bands.
 *
 * The bands are a school approximation of the Examinations Council of Zambia
 * 1-9 scale. ECZ standardises each subject each year, so the same percentage
 * is not always the same national grade. See the README.
 */

static const char *SUBJECT_NAMES[NUM_SUBJECTS] = {
    "English Language",
    "Mathematics",
    "Integrated Science",
    "Social Studies",
    "Civic Education",
    "Religious Education",
    "Computer Studies",
    "Business Studies"
};

static const char *SUBJECT_SHORT[NUM_SUBJECTS] = {
    "Eng", "Mat", "Sci", "Soc", "Civ", "R.E", "Com", "Bus"
};

/* CSV column names. Keep the same order as SUBJECT_NAMES. */
static const char *SUBJECT_KEYS[NUM_SUBJECTS] = {
    "english",
    "mathematics",
    "integrated_science",
    "social_studies",
    "civic_education",
    "religious_education",
    "computer_studies",
    "business_studies"
};

static void set_error(char *error, int error_size, const char *message) {
    if (error == NULL || error_size <= 0) {
        return;
    }
    copy_str(error, error_size, message);
}

static int is_letter(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

int copy_str(char *dst, int dst_size, const char *src) {
    int i = 0;

    if (dst == NULL || dst_size <= 0) {
        return -1;
    }
    if (src == NULL) {
        src = "";
    }
    while (src[i] != '\0' && i < dst_size - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
    if (src[i] == '\0') {
        return 0;
    }
    return -1;
}

void trim_inplace(char *s) {
    size_t start = 0;
    size_t len;
    size_t i;

    if (s == NULL) {
        return;
    }
    while (s[start] == ' ' || s[start] == '\t' || s[start] == '\r') {
        start++;
    }
    if (start > 0) {
        i = 0;
        while (s[start + i] != '\0') {
            s[i] = s[start + i];
            i++;
        }
        s[i] = '\0';
    }
    len = strlen(s);
    while (len > 0 &&
           (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r')) {
        len--;
        s[len] = '\0';
    }
}

int str_icmp(const char *a, const char *b) {
    if (a == NULL) {
        a = "";
    }
    if (b == NULL) {
        b = "";
    }
    while (*a != '\0' && *b != '\0') {
        unsigned char ca = (unsigned char)*a;
        unsigned char cb = (unsigned char)*b;
        if (ca >= 'A' && ca <= 'Z') {
            ca = (unsigned char)(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = (unsigned char)(cb - 'A' + 'a');
        }
        if (ca != cb) {
            return (int)ca - (int)cb;
        }
        a++;
        b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

static int contains_ci(const char *haystack, const char *needle) {
    size_t i;

    if (needle == NULL || needle[0] == '\0') {
        return 0;
    }
    if (haystack == NULL) {
        return 0;
    }
    for (i = 0; haystack[i] != '\0'; i++) {
        size_t j = 0;
        while (needle[j] != '\0' && haystack[i + j] != '\0') {
            unsigned char hc = (unsigned char)haystack[i + j];
            unsigned char nc = (unsigned char)needle[j];
            if (hc >= 'A' && hc <= 'Z') {
                hc = (unsigned char)(hc - 'A' + 'a');
            }
            if (nc >= 'A' && nc <= 'Z') {
                nc = (unsigned char)(nc - 'A' + 'a');
            }
            if (hc != nc) {
                break;
            }
            j++;
        }
        if (needle[j] == '\0') {
            return 1;
        }
    }
    return 0;
}

void student_clear(Student *s) {
    int term;
    int subject;

    if (s == NULL) {
        return;
    }
    s->learner_no[0] = '\0';
    s->name[0] = '\0';
    s->grade = 0;
    s->class_name[0] = '\0';
    s->gender = '\0';
    s->guardian_phone[0] = '\0';
    for (term = 0; term < NUM_TERMS; term++) {
        for (subject = 0; subject < NUM_SUBJECTS; subject++) {
            s->marks[term][subject] = MARK_NONE;
        }
    }
}

const char *subject_name(int index) {
    if (index < 0 || index >= NUM_SUBJECTS || SUBJECT_NAMES[index] == NULL) {
        return "?";
    }
    return SUBJECT_NAMES[index];
}

const char *subject_short_name(int index) {
    if (index < 0 || index >= NUM_SUBJECTS || SUBJECT_SHORT[index] == NULL) {
        return "?";
    }
    return SUBJECT_SHORT[index];
}

const char *subject_key(int index) {
    if (index < 0 || index >= NUM_SUBJECTS || SUBJECT_KEYS[index] == NULL) {
        return "unknown";
    }
    return SUBJECT_KEYS[index];
}

int ecz_grade_band(int mark) {
    /* Cuts used by many schools as a stand-in for the ECZ 1-9 scale. */
    if (mark < 0 || mark > 100) {
        return 0;
    }
    if (mark >= 75) {
        return 1;
    }
    if (mark >= 70) {
        return 2;
    }
    if (mark >= 65) {
        return 3;
    }
    if (mark >= 60) {
        return 4;
    }
    if (mark >= 55) {
        return 5;
    }
    if (mark >= 50) {
        return 6;
    }
    if (mark >= 45) {
        return 7;
    }
    if (mark >= 40) {
        return 8;
    }
    return 9;
}

const char *ecz_descriptor(int band) {
    switch (band) {
    case 1:
    case 2:
        return "Distinction";
    case 3:
    case 4:
        return "Merit";
    case 5:
    case 6:
        return "Credit";
    case 7:
    case 8:
        return "Satisfactory";
    case 9:
        return "Unsatisfactory";
    default:
        return "No grade";
    }
}

int rounded_average_mark(int total, int counted) {
    if (counted <= 0) {
        return 0;
    }
    /* Adding half the divisor rounds .5 upward for positive marks. */
    return (total + counted / 2) / counted;
}

int average_tenths(int total, int counted) {
    if (counted <= 0) {
        return -1;
    }
    return (total * 10 + counted / 2) / counted;
}

void format_tenths(int tenths, char *buf, int buf_size) {
    int whole;
    int frac;

    if (buf == NULL || buf_size <= 0) {
        return;
    }
    /* A percentage average is 0.0 to 100.0, so tenths run from 0 to 1000. */
    if (tenths < 0 || tenths > 1000 || buf_size < 6) {
        copy_str(buf, buf_size, "-");
        return;
    }
    whole = tenths / 10;
    frac = tenths % 10;
    if (whole == 100) {
        buf[0] = '1';
        buf[1] = '0';
        buf[2] = '0';
        buf[3] = '.';
        buf[4] = (char)('0' + frac);
        buf[5] = '\0';
        return;
    }
    if (whole >= 10) {
        buf[0] = (char)('0' + whole / 10);
        buf[1] = (char)('0' + whole % 10);
        buf[2] = '.';
        buf[3] = (char)('0' + frac);
        buf[4] = '\0';
        return;
    }
    buf[0] = (char)('0' + whole);
    buf[1] = '.';
    buf[2] = (char)('0' + frac);
    buf[3] = '\0';
}

int marks_total(const Student *s, int term_index, int *counted_out) {
    int subject;
    int total = 0;
    int counted = 0;

    if (counted_out != NULL) {
        *counted_out = 0;
    }
    if (s == NULL || term_index < 0 || term_index >= NUM_TERMS) {
        return 0;
    }
    for (subject = 0; subject < NUM_SUBJECTS; subject++) {
        int mark = s->marks[term_index][subject];
        if (mark == MARK_NONE) {
            continue;
        }
        total += mark;
        counted++;
    }
    if (counted_out != NULL) {
        *counted_out = counted;
    }
    return total;
}

void register_init(Register *reg) {
    int i;

    if (reg == NULL) {
        return;
    }
    reg->count = 0;
    for (i = 0; i < MAX_STUDENTS; i++) {
        student_clear(&reg->items[i]);
    }
}

int register_find(const Register *reg, const char *learner_no) {
    int i;

    if (reg == NULL || learner_no == NULL) {
        return -1;
    }
    for (i = 0; i < reg->count; i++) {
        if (str_icmp(reg->items[i].learner_no, learner_no) == 0) {
            return i;
        }
    }
    return -1;
}

int parse_whole_number(const char *text, int *out) {
    char *end;
    long value;

    if (text == NULL || text[0] == '\0' || out == NULL) {
        return 0;
    }
    errno = 0;
    value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || errno == ERANGE) {
        return 0;
    }
    if (value > 2147483647L || value < -2147483647L - 1) {
        return 0;
    }
    *out = (int)value;
    return 1;
}

int parse_gender(const char *text, char *out) {
    char buf[32];

    if (out == NULL) {
        return 0;
    }
    *out = '\0';
    if (text == NULL) {
        return 0;
    }
    if (copy_str(buf, (int)sizeof buf, text) != 0) {
        return 0;
    }
    trim_inplace(buf);
    if (str_icmp(buf, "m") == 0 || str_icmp(buf, "male") == 0) {
        *out = 'M';
        return 1;
    }
    if (str_icmp(buf, "f") == 0 || str_icmp(buf, "female") == 0) {
        *out = 'F';
        return 1;
    }
    return 0;
}

int prepare_phone(const char *src, char *dst, int dst_size) {
    int w = 0;

    if (dst == NULL || dst_size <= 0) {
        return -1;
    }
    if (src == NULL) {
        src = "";
    }
    while (*src != '\0') {
        char c = *src;
        /* Clerks often type 0971 000 001 or 0971-000-001. Store digits only. */
        if (c != ' ' && c != '-' && c != '\t') {
            if (w >= dst_size - 1) {
                dst[0] = '\0';
                return -1;
            }
            dst[w] = c;
            w++;
        }
        src++;
    }
    dst[w] = '\0';
    return 0;
}

static int valid_name_char(char c) {
    return is_letter(c) || c == ' ' || c == '-' || c == '\'' || c == ',' ||
           c == '.' || c == '"';
}

static int valid_id_char(char c) {
    return is_letter(c) || is_digit(c) || c == '-' || c == '/';
}

static int valid_class_char(char c) {
    return is_letter(c) || is_digit(c) || c == ' ' || c == '-';
}

static int valid_phone_text(const char *phone) {
    int i = 0;
    int digits = 0;

    if (phone[0] == '+') {
        i = 1;
    }
    if (phone[i] == '\0') {
        return 0;
    }
    for (; phone[i] != '\0'; i++) {
        if (!is_digit(phone[i])) {
            return 0;
        }
        digits++;
    }
    /* 0971234567 (10 digits) or +260971234567 (12 digits after the check). */
    return digits >= 9 && digits <= 13;
}

static void student_normalize(Student *s) {
    char phone[MAX_PHONE];

    trim_inplace(s->learner_no);
    trim_inplace(s->name);
    trim_inplace(s->class_name);
    trim_inplace(s->guardian_phone);
    if (prepare_phone(s->guardian_phone, phone, MAX_PHONE) == 0) {
        copy_str(s->guardian_phone, MAX_PHONE, phone);
    }
    if (s->gender >= 'a' && s->gender <= 'z') {
        s->gender = (char)(s->gender - 'a' + 'A');
    }
}

static int validate_student(const Student *s, const Register *reg,
                            int ignore_index, char *error, int error_size) {
    int i;
    int letters = 0;
    int found;
    int term;
    int subject;

    if (s->learner_no[0] == '\0') {
        set_error(error, error_size, "Enter a learner number.");
        return -1;
    }
    for (i = 0; s->learner_no[i] != '\0'; i++) {
        if (!valid_id_char(s->learner_no[i])) {
            set_error(error, error_size,
                      "A learner number may only use letters, digits, '-' and '/'.");
            return -1;
        }
    }
    found = register_find(reg, s->learner_no);
    if (found >= 0 && found != ignore_index) {
        set_error(error, error_size,
                  "That learner number is already in the register.");
        return -1;
    }

    if (s->name[0] == '\0') {
        set_error(error, error_size, "Enter the learner's name.");
        return -1;
    }
    for (i = 0; s->name[i] != '\0'; i++) {
        if (!valid_name_char(s->name[i])) {
            set_error(error, error_size,
                      "A name may only use letters, spaces, '-', apostrophes, commas and full stops.");
            return -1;
        }
        if (is_letter(s->name[i])) {
            letters++;
        }
    }
    if (letters == 0) {
        set_error(error, error_size, "Enter the learner's name.");
        return -1;
    }

    if (s->grade < MIN_GRADE || s->grade > MAX_GRADE) {
        set_error(error, error_size, "Grade must be from 8 to 12.");
        return -1;
    }

    if (s->class_name[0] == '\0') {
        set_error(error, error_size, "Enter a class, for example A.");
        return -1;
    }
    for (i = 0; s->class_name[i] != '\0'; i++) {
        if (!valid_class_char(s->class_name[i])) {
            set_error(error, error_size,
                      "A class name may only use letters, digits, spaces and '-'.");
            return -1;
        }
    }

    if (s->gender != 'M' && s->gender != 'F') {
        set_error(error, error_size, "Gender must be M or F.");
        return -1;
    }

    if (!valid_phone_text(s->guardian_phone)) {
        set_error(error, error_size,
                  "Guardian phone should look like 0971234567 or +260971234567.");
        return -1;
    }

    for (term = 0; term < NUM_TERMS; term++) {
        for (subject = 0; subject < NUM_SUBJECTS; subject++) {
            int mark = s->marks[term][subject];
            if (mark == MARK_NONE) {
                continue;
            }
            if (mark < 0 || mark > 100) {
                set_error(error, error_size,
                          "Each mark must be from 0 to 100, or blank if not entered.");
                return -1;
            }
        }
    }
    return 0;
}

int register_add(Register *reg, const Student *s, char *error, int error_size) {
    Student copy;

    if (reg == NULL || s == NULL) {
        set_error(error, error_size, "Nothing to add.");
        return -1;
    }
    copy = *s;
    student_normalize(&copy);
    if (validate_student(&copy, reg, -1, error, error_size) != 0) {
        return -1;
    }
    if (reg->count >= MAX_STUDENTS) {
        set_error(error, error_size, "The register is full (250 learners).");
        return -1;
    }
    reg->items[reg->count] = copy;
    reg->count++;
    return 0;
}

int register_update(Register *reg, int index, const Student *s,
                    char *error, int error_size) {
    Student copy;

    if (reg == NULL || s == NULL || index < 0 || index >= reg->count) {
        set_error(error, error_size, "That learner is not in the register.");
        return -1;
    }
    copy = *s;
    student_normalize(&copy);
    if (validate_student(&copy, reg, index, error, error_size) != 0) {
        return -1;
    }
    reg->items[index] = copy;
    return 0;
}

int register_delete(Register *reg, int index, char *error, int error_size) {
    int i;

    if (reg == NULL || index < 0 || index >= reg->count) {
        set_error(error, error_size, "That learner is not in the register.");
        return -1;
    }
    for (i = index; i < reg->count - 1; i++) {
        reg->items[i] = reg->items[i + 1];
    }
    reg->count--;
    student_clear(&reg->items[reg->count]);
    return 0;
}

int register_search(const Register *reg, const char *query,
                    int *out_indices, int max_out) {
    int i;
    int found = 0;
    char needle[MAX_NAME];

    if (reg == NULL || query == NULL || out_indices == NULL || max_out <= 0) {
        return 0;
    }
    if (copy_str(needle, (int)sizeof needle, query) != 0) {
        /* A very long query cannot match a stored name or number. */
        return 0;
    }
    trim_inplace(needle);
    if (needle[0] == '\0') {
        return 0;
    }
    for (i = 0; i < reg->count && found < max_out; i++) {
        if (contains_ci(reg->items[i].name, needle) ||
            contains_ci(reg->items[i].learner_no, needle)) {
            out_indices[found] = i;
            found++;
        }
    }
    return found;
}

int student_equal(const Student *a, const Student *b) {
    int term;
    int subject;

    if (a == NULL || b == NULL) {
        return 0;
    }
    if (strcmp(a->learner_no, b->learner_no) != 0) {
        return 0;
    }
    if (strcmp(a->name, b->name) != 0) {
        return 0;
    }
    if (a->grade != b->grade) {
        return 0;
    }
    if (strcmp(a->class_name, b->class_name) != 0) {
        return 0;
    }
    if (a->gender != b->gender) {
        return 0;
    }
    if (strcmp(a->guardian_phone, b->guardian_phone) != 0) {
        return 0;
    }
    for (term = 0; term < NUM_TERMS; term++) {
        for (subject = 0; subject < NUM_SUBJECTS; subject++) {
            if (a->marks[term][subject] != b->marks[term][subject]) {
                return 0;
            }
        }
    }
    return 1;
}

static int result_before(const Register *reg, const ResultRow *a,
                         const ResultRow *b) {
    int a_ranked = a->counted > 0;
    int b_ranked = b->counted > 0;
    int cmp;

    if (a_ranked != b_ranked) {
        return a_ranked;
    }
    if (a_ranked) {
        long left = (long)a->total * (long)b->counted;
        long right = (long)b->total * (long)a->counted;
        if (left != right) {
            return left > right;
        }
    }
    cmp = str_icmp(reg->items[a->index].name, reg->items[b->index].name);
    if (cmp != 0) {
        return cmp < 0;
    }
    cmp = str_icmp(reg->items[a->index].learner_no,
                   reg->items[b->index].learner_no);
    return cmp < 0;
}

static int same_average(const ResultRow *a, const ResultRow *b) {
    if (a->counted <= 0 || b->counted <= 0) {
        return 0;
    }
    return (long)a->total * (long)b->counted == (long)b->total * (long)a->counted;
}

int build_class_results(const Register *reg, int grade, const char *class_name,
                        int term_index, ResultRow *rows, int max_rows) {
    int i;
    int n = 0;

    if (reg == NULL || class_name == NULL || rows == NULL || max_rows <= 0) {
        return 0;
    }
    if (term_index < 0 || term_index >= NUM_TERMS) {
        return 0;
    }

    for (i = 0; i < reg->count; i++) {
        const Student *s = &reg->items[i];
        int counted = 0;
        int total;

        if (s->grade != grade || str_icmp(s->class_name, class_name) != 0) {
            continue;
        }
        if (n >= max_rows) {
            break;
        }
        total = marks_total(s, term_index, &counted);
        rows[n].index = i;
        rows[n].total = total;
        rows[n].counted = counted;
        rows[n].position = 0;
        n++;
    }

    /*
     * Insertion sort is enough for a class of at most 250 and is easier to
     * read than qsort. Higher average first; equal averages go A to Z.
     */
    for (i = 1; i < n; i++) {
        ResultRow key = rows[i];
        int j = i - 1;
        while (j >= 0 && result_before(reg, &key, &rows[j])) {
            rows[j + 1] = rows[j];
            j--;
        }
        rows[j + 1] = key;
    }

    for (i = 0; i < n; i++) {
        if (rows[i].counted == 0) {
            rows[i].position = 0;
            continue;
        }
        if (i > 0 && same_average(&rows[i], &rows[i - 1])) {
            rows[i].position = rows[i - 1].position;
        } else {
            /* i is 0-based among rows, and unranked learners sort last. */
            rows[i].position = i + 1;
        }
    }
    return n;
}
