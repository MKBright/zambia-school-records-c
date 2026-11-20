#ifndef STUDENT_H
#define STUDENT_H

/*
 * The register holds learners and their term marks.
 *
 * Marks are whole numbers from 0 to 100. MARK_NONE means "no mark entered".
 * It must not be 0, because 0 is a real score a learner can earn.
 *
 * Fixed arrays are used on purpose. A day-school class list fits in memory,
 * and the program does not need malloc to do its job.
 */

#define MAX_STUDENTS 250
#define MAX_NAME 64
#define MAX_LEARNER_NO 20
#define MAX_CLASS 16
#define MAX_PHONE 16
#define NUM_SUBJECTS 8
#define NUM_TERMS 3
#define MARK_NONE (-1)
#define MIN_GRADE 8
#define MAX_GRADE 12

typedef struct {
    char learner_no[MAX_LEARNER_NO];
    char name[MAX_NAME];
    int grade; /* 8 to 12 */
    char class_name[MAX_CLASS];
    char gender; /* 'M' or 'F' */
    char guardian_phone[MAX_PHONE];
    int marks[NUM_TERMS][NUM_SUBJECTS];
} Student;

typedef struct {
    Student items[MAX_STUDENTS];
    int count;
} Register;

/* One row of a class report, before or after positions are filled in. */
typedef struct {
    int index;    /* position of this learner inside Register.items */
    int total;    /* sum of marks that were actually entered */
    int counted;  /* how many subjects have a mark; 0 means not ranked */
    int position; /* 1 is top; 0 means not ranked */
} ResultRow;

void student_clear(Student *s);

const char *subject_name(int index);
const char *subject_short_name(int index);
const char *subject_key(int index);

/* ECZ-style band 1..9 for a percentage, or 0 if the mark is not usable. */
int ecz_grade_band(int mark);
const char *ecz_descriptor(int band);

/* Nearest whole number of total/counted. Half rounds away from zero. */
int rounded_average_mark(int total, int counted);

/* One-decimal average as tenths: 816 means 81.6. Returns -1 if counted is 0. */
int average_tenths(int total, int counted);
void format_tenths(int tenths, char *buf, int buf_size);

int marks_total(const Student *s, int term_index, int *counted_out);

void register_init(Register *reg);
int register_find(const Register *reg, const char *learner_no);

/*
 * Add or update a learner. On failure the register is left unchanged and
 * error (when not NULL) receives a short message. ignore_index is used by
 * update so a learner may keep their own number. Pass -1 when adding.
 */
int register_add(Register *reg, const Student *s, char *error, int error_size);
int register_update(Register *reg, int index, const Student *s,
                    char *error, int error_size);
int register_delete(Register *reg, int index, char *error, int error_size);

/* Case-insensitive match on name or learner number. Returns how many. */
int register_search(const Register *reg, const char *query,
                    int *out_indices, int max_out);

/*
 * Fill rows with every learner in this grade and class, highest average
 * first, and set competition positions (a tie shares a place and the next
 * place is skipped: 1, 1, 3). Returns the number of rows.
 */
int build_class_results(const Register *reg, int grade, const char *class_name,
                        int term_index, ResultRow *rows, int max_rows);

int student_equal(const Student *a, const Student *b);

int str_icmp(const char *a, const char *b);
void trim_inplace(char *s);
int copy_str(char *dst, int dst_size, const char *src);
int parse_whole_number(const char *text, int *out);
int parse_gender(const char *text, char *out);
int prepare_phone(const char *src, char *dst, int dst_size);

#endif
