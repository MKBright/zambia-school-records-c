#ifndef STORAGE_H
#define STORAGE_H

/*
 * Save and load the register as CSV so a school office can open the same
 * file in Excel, and so the data is still there after the program closes.
 */

#include "student.h"

#define CSV_PATH_MAX 260

#define STORAGE_OK 0
#define STORAGE_ERR -1
#define STORAGE_NOT_FOUND -2

int save_register_csv(const Register *reg, const char *path,
                      char *error, int error_size);
int load_register_csv(Register *reg, const char *path,
                      char *error, int error_size);

#endif
