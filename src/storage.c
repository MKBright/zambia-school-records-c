#include "storage.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

/*
 * CSV layout (one row per learner):
 *   learner_number, full_name, grade, class, gender, guardian_phone,
 *   then 8 subject marks for term 1, term 2 and term 3.
 * An empty mark cell means "not entered" (MARK_NONE), not zero.
 *
 * Fields that contain a comma or a quote are wrapped in quotes, and a quote
 * inside a name is stored as two quotes. That is the usual CSV rule, and it
 * is what keeps a name like "Banda, Luyando" in one Excel cell.
 *
 * Saving writes a temporary file and then renames it, so a failed save does
 * not wipe the previous register.
 */

#define CSV_ID_COLUMNS 6
#define CSV_COLUMNS (CSV_ID_COLUMNS + (NUM_TERMS * NUM_SUBJECTS))
#define CSV_FIELD_MAX 96

#define ROW_EOF 0
#define ROW_BLANK -1
#define ROW_ERROR -2

static void set_msg(char *error, int error_size, const char *message) {
    copy_str(error, error_size, message);
}

static void line_msg(char *error, int error_size, int line_no, const char *message) {
    char buf[400];

    /* The caps on %d and %.300s keep this inside buf. */
    snprintf(buf, sizeof buf, "Line %d: %.300s", line_no, message);
    copy_str(error, error_size, buf);
}

static void io_msg(char *error, int error_size, const char *action, const char *path) {
    char buf[512];
    const char *why;

    why = strerror(errno);
    snprintf(buf, sizeof buf, "%.48s %.220s: %.80s", action, path, why);
    copy_str(error, error_size, buf);
}

static void expected_column(int col, char *buf, int buf_size) {
    int offset;
    int term;
    int subject;

    if (col == 0) {
        copy_str(buf, buf_size, "learner_number");
        return;
    }
    if (col == 1) {
        copy_str(buf, buf_size, "full_name");
        return;
    }
    if (col == 2) {
        copy_str(buf, buf_size, "grade");
        return;
    }
    if (col == 3) {
        copy_str(buf, buf_size, "class");
        return;
    }
    if (col == 4) {
        copy_str(buf, buf_size, "gender");
        return;
    }
    if (col == 5) {
        copy_str(buf, buf_size, "guardian_phone");
        return;
    }
    offset = col - CSV_ID_COLUMNS;
    term = offset / NUM_SUBJECTS;
    subject = offset % NUM_SUBJECTS;
    snprintf(buf, (size_t)buf_size, "t%d_%.40s", term + 1, subject_key(subject));
}

static int write_field(FILE *fp, const char *text) {
    int needs_quote = 0;
    const char *p;

    if (text == NULL) {
        text = "";
    }
    for (p = text; *p != '\0'; p++) {
        if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r') {
            needs_quote = 1;
            break;
        }
    }
    if (!needs_quote) {
        if (fputs(text, fp) == EOF) {
            return -1;
        }
        return 0;
    }
    if (fputc('"', fp) == EOF) {
        return -1;
    }
    for (p = text; *p != '\0'; p++) {
        if (*p == '"') {
            if (fputc('"', fp) == EOF) {
                return -1;
            }
        }
        if (fputc(*p, fp) == EOF) {
            return -1;
        }
    }
    if (fputc('"', fp) == EOF) {
        return -1;
    }
    return 0;
}

static int write_header(FILE *fp) {
    int col;
    char name[64];

    for (col = 0; col < CSV_COLUMNS; col++) {
        if (col > 0 && fputc(',', fp) == EOF) {
            return -1;
        }
        expected_column(col, name, (int)sizeof name);
        if (fputs(name, fp) == EOF) {
            return -1;
        }
    }
    if (fputc('\n', fp) == EOF) {
        return -1;
    }
    return 0;
}

static int write_student_row(FILE *fp, const Student *s) {
    int term;
    int subject;
    char gender_text[2];

    gender_text[0] = s->gender;
    gender_text[1] = '\0';

    if (write_field(fp, s->learner_no) != 0) {
        return -1;
    }
    if (fputc(',', fp) == EOF) {
        return -1;
    }
    if (write_field(fp, s->name) != 0) {
        return -1;
    }
    if (fprintf(fp, ",%d,", s->grade) < 0) {
        return -1;
    }
    if (write_field(fp, s->class_name) != 0) {
        return -1;
    }
    if (fputc(',', fp) == EOF) {
        return -1;
    }
    if (write_field(fp, gender_text) != 0) {
        return -1;
    }
    if (fputc(',', fp) == EOF) {
        return -1;
    }
    if (write_field(fp, s->guardian_phone) != 0) {
        return -1;
    }
    for (term = 0; term < NUM_TERMS; term++) {
        for (subject = 0; subject < NUM_SUBJECTS; subject++) {
            int mark = s->marks[term][subject];
            if (fputc(',', fp) == EOF) {
                return -1;
            }
            if (mark != MARK_NONE && fprintf(fp, "%d", mark) < 0) {
                return -1;
            }
        }
    }
    if (fputc('\n', fp) == EOF) {
        return -1;
    }
    return 0;
}

static void make_temp_path(const char *path, char *tmp_path) {
    size_t len = strlen(path);
    memcpy(tmp_path, path, len);
    memcpy(tmp_path + len, ".tmp", 5);
}

int save_register_csv(const Register *reg, const char *path,
                      char *error, int error_size) {
    FILE *fp;
    char tmp_path[CSV_PATH_MAX];
    int i;
    size_t len;

    if (reg == NULL || path == NULL || path[0] == '\0') {
        set_msg(error, error_size, "Enter a file name to save to.");
        return STORAGE_ERR;
    }
    len = strlen(path);
    if (len + 5 > (size_t)CSV_PATH_MAX) {
        set_msg(error, error_size, "That file path is too long.");
        return STORAGE_ERR;
    }
    make_temp_path(path, tmp_path);

    fp = fopen(tmp_path, "w");
    if (fp == NULL) {
        io_msg(error, error_size, "Could not write", path);
        return STORAGE_ERR;
    }
    if (write_header(fp) != 0) {
        fclose(fp);
        remove(tmp_path);
        set_msg(error, error_size, "Could not write the header row.");
        return STORAGE_ERR;
    }
    for (i = 0; i < reg->count; i++) {
        if (write_student_row(fp, &reg->items[i]) != 0) {
            fclose(fp);
            remove(tmp_path);
            set_msg(error, error_size, "Could not write a learner row.");
            return STORAGE_ERR;
        }
    }
    if (fclose(fp) == EOF) {
        int saved = errno;
        remove(tmp_path);
        errno = saved;
        io_msg(error, error_size, "Could not finish writing", path);
        return STORAGE_ERR;
    }
    if (rename(tmp_path, path) != 0) {
        int saved = errno;
        remove(tmp_path);
        errno = saved;
        io_msg(error, error_size, "Could not replace", path);
        return STORAGE_ERR;
    }
    return STORAGE_OK;
}

static void discard_rest_of_line(FILE *fp, int *line_no) {
    int ch;

    while ((ch = fgetc(fp)) != EOF && ch != '\n') {
        /* Skip the rest of a damaged row so the line counter stays honest. */
    }
    if (ch == '\n') {
        (*line_no)++;
    }
}

static int push_char(char fields[][CSV_FIELD_MAX], int field, int *len, int ch,
                     char *error, int error_size, int line_no) {
    if (field < 0 || field >= CSV_COLUMNS) {
        line_msg(error, error_size, line_no,
                 "Too many columns. Keep the header this program writes.");
        return -1;
    }
    if (*len >= CSV_FIELD_MAX - 1) {
        line_msg(error, error_size, line_no, "A value is too long.");
        return -1;
    }
    fields[field][*len] = (char)ch;
    (*len)++;
    fields[field][*len] = '\0';
    return 0;
}

/*
 * Read one CSV row. Returns the field count, ROW_BLANK, ROW_EOF or ROW_ERROR.
 * *line_no is the line about to be read, and moves on when a row ends.
 */
static int read_csv_row(FILE *fp, char fields[][CSV_FIELD_MAX], int *line_no,
                        char *error, int error_size) {
    int in_quotes = 0;
    int field = 0;
    int len = 0;
    int any_char = 0;
    int ch;
    int this_line;
    int i;

    this_line = *line_no;
    for (i = 0; i < CSV_COLUMNS; i++) {
        fields[i][0] = '\0';
    }

    while ((ch = fgetc(fp)) != EOF) {
        if (in_quotes) {
            if (ch == '"') {
                int next = fgetc(fp);
                if (next == '"') {
                    if (push_char(fields, field, &len, '"', error, error_size,
                                  this_line) != 0) {
                        discard_rest_of_line(fp, line_no);
                        return ROW_ERROR;
                    }
                } else {
                    in_quotes = 0;
                    if (next != EOF && ungetc(next, fp) == EOF) {
                        line_msg(error, error_size, this_line,
                                 "Could not read a quoted value.");
                        return ROW_ERROR;
                    }
                }
            } else if (ch == '\r') {
                continue;
            } else if (ch == '\n') {
                line_msg(error, error_size, this_line,
                         "A quoted value cannot cross onto the next line.");
                *line_no = this_line + 1;
                return ROW_ERROR;
            } else if (push_char(fields, field, &len, ch, error, error_size,
                                 this_line) != 0) {
                discard_rest_of_line(fp, line_no);
                return ROW_ERROR;
            }
            continue;
        }

        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            *line_no = this_line + 1;
            if (!any_char) {
                return ROW_BLANK;
            }
            return field + 1;
        }

        any_char = 1;
        if (ch == '"') {
            if (len == 0) {
                in_quotes = 1;
                continue;
            }
        }
        if (ch == ',') {
            if (field + 1 >= CSV_COLUMNS) {
                line_msg(error, error_size, this_line,
                         "Too many columns. Keep the header this program writes.");
                discard_rest_of_line(fp, line_no);
                return ROW_ERROR;
            }
            field++;
            len = 0;
            fields[field][0] = '\0';
            continue;
        }
        if (push_char(fields, field, &len, ch, error, error_size, this_line) != 0) {
            discard_rest_of_line(fp, line_no);
            return ROW_ERROR;
        }
    }

    if (in_quotes) {
        line_msg(error, error_size, this_line, "A quoted value was not closed.");
        return ROW_ERROR;
    }
    if (!any_char) {
        return ROW_EOF;
    }
    *line_no = this_line + 1;
    return field + 1;
}

static int skip_bom(FILE *fp, char *error, int error_size) {
    int first;
    int second;
    int third;

    first = fgetc(fp);
    if (first == EOF) {
        set_msg(error, error_size, "The file is empty.");
        return -1;
    }
    /* Excel sometimes saves a UTF-8 BOM in front of the header. */
    if (first != 0xEF) {
        if (ungetc(first, fp) == EOF) {
            set_msg(error, error_size, "Could not read the file.");
            return -1;
        }
        return 0;
    }
    second = fgetc(fp);
    third = fgetc(fp);
    if (second == 0xBB && third == 0xBF) {
        return 0;
    }
    set_msg(error, error_size, "The file does not look like a CSV register.");
    return -1;
}

static int header_ok(char fields[][CSV_FIELD_MAX], int count,
                     char *error, int error_size) {
    int col;
    char expected[64];

    if (count != CSV_COLUMNS) {
        set_msg(error, error_size,
                "The header row does not match this program. Do not rename or delete columns.");
        return -1;
    }
    for (col = 0; col < CSV_COLUMNS; col++) {
        char buf[160];
        trim_inplace(fields[col]);
        expected_column(col, expected, (int)sizeof expected);
        if (str_icmp(fields[col], expected) != 0) {
            snprintf(buf, sizeof buf, "Column %d should be named %.40s.",
                     col + 1, expected);
            set_msg(error, error_size, buf);
            return -1;
        }
    }
    return 0;
}

static int parse_data_row(char fields[][CSV_FIELD_MAX], int count, Student *s,
                          char *error, int error_size) {
    int col;
    int term;
    int subject;
    int grade;
    int mark;
    char gender;

    if (count != CSV_COLUMNS) {
        char buf[80];
        snprintf(buf, sizeof buf, "Expected %d columns but found %d.",
                 CSV_COLUMNS, count);
        set_msg(error, error_size, buf);
        return -1;
    }

    student_clear(s);
    for (col = 0; col < CSV_ID_COLUMNS; col++) {
        trim_inplace(fields[col]);
    }
    if (strlen(fields[0]) >= (size_t)MAX_LEARNER_NO) {
        set_msg(error, error_size, "Learner number is too long.");
        return -1;
    }
    copy_str(s->learner_no, MAX_LEARNER_NO, fields[0]);

    if (strlen(fields[1]) >= (size_t)MAX_NAME) {
        set_msg(error, error_size, "Name is too long.");
        return -1;
    }
    copy_str(s->name, MAX_NAME, fields[1]);

    if (!parse_whole_number(fields[2], &grade)) {
        set_msg(error, error_size, "Grade must be a number from 8 to 12.");
        return -1;
    }
    s->grade = grade;

    if (strlen(fields[3]) >= (size_t)MAX_CLASS) {
        set_msg(error, error_size, "Class name is too long.");
        return -1;
    }
    copy_str(s->class_name, MAX_CLASS, fields[3]);

    if (!parse_gender(fields[4], &gender)) {
        s->gender = '\0';
    } else {
        s->gender = gender;
    }

    if (prepare_phone(fields[5], s->guardian_phone, MAX_PHONE) != 0) {
        set_msg(error, error_size, "Guardian phone is too long.");
        return -1;
    }

    col = CSV_ID_COLUMNS;
    for (term = 0; term < NUM_TERMS; term++) {
        for (subject = 0; subject < NUM_SUBJECTS; subject++) {
            trim_inplace(fields[col]);
            if (fields[col][0] == '\0') {
                s->marks[term][subject] = MARK_NONE;
            } else if (!parse_whole_number(fields[col], &mark) ||
                       mark < 0 || mark > 100) {
                char buf[160];
                snprintf(buf, sizeof buf,
                         "Mark for %.40s in term %d must be from 0 to 100, or blank.",
                         subject_name(subject), term + 1);
                set_msg(error, error_size, buf);
                return -1;
            } else {
                s->marks[term][subject] = mark;
            }
            col++;
        }
    }
    return 0;
}

int load_register_csv(Register *reg, const char *path,
                      char *error, int error_size) {
    FILE *fp;
    Register temp;
    Student student;
    char fields[CSV_COLUMNS][CSV_FIELD_MAX];
    char detail[400];
    int line_no = 1;
    int row;

    if (reg == NULL || path == NULL || path[0] == '\0') {
        set_msg(error, error_size, "Enter a file name to load.");
        return STORAGE_ERR;
    }

    fp = fopen(path, "r");
    if (fp == NULL) {
        if (errno == ENOENT) {
            set_msg(error, error_size, "That file was not found.");
            return STORAGE_NOT_FOUND;
        }
        io_msg(error, error_size, "Could not open", path);
        return STORAGE_ERR;
    }
    if (skip_bom(fp, detail, (int)sizeof detail) != 0) {
        set_msg(error, error_size, detail);
        fclose(fp);
        return STORAGE_ERR;
    }

    register_init(&temp);
    row = read_csv_row(fp, fields, &line_no, detail, (int)sizeof detail);
    if (row == ROW_ERROR) {
        set_msg(error, error_size, detail);
        fclose(fp);
        return STORAGE_ERR;
    }
    if (row == ROW_EOF || row == ROW_BLANK) {
        set_msg(error, error_size, "The file is missing its header row.");
        fclose(fp);
        return STORAGE_ERR;
    }
    if (header_ok(fields, row, detail, (int)sizeof detail) != 0) {
        set_msg(error, error_size, detail);
        fclose(fp);
        return STORAGE_ERR;
    }

    for (;;) {
        int row_line = line_no;
        row = read_csv_row(fp, fields, &line_no, detail, (int)sizeof detail);
        if (row == ROW_EOF) {
            break;
        }
        if (row == ROW_BLANK) {
            continue;
        }
        if (row == ROW_ERROR) {
            set_msg(error, error_size, detail);
            fclose(fp);
            return STORAGE_ERR;
        }
        if (parse_data_row(fields, row, &student, detail, (int)sizeof detail) != 0 ||
            register_add(&temp, &student, detail, (int)sizeof detail) != 0) {
            line_msg(error, error_size, row_line, detail);
            fclose(fp);
            return STORAGE_ERR;
        }
    }

    if (fclose(fp) == EOF) {
        set_msg(error, error_size, "Could not finish reading the file.");
        return STORAGE_ERR;
    }
    *reg = temp;
    return STORAGE_OK;
}
