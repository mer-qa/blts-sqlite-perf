/*
 * blts-sqlite-perf - performance test suite for sqlite3
 *
 * Copyright (C) 2013 Jolla Ltd.
 * Contact: Martin Kampas <martin.kampas@jollamobile.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
 * The test_* functions, when passed appropriate arguments, implement all the test cases as
 * documented on http://www.sqlite.org/speed.html.
 */

#define _GNU_SOURCE
#include <assert.h>
#include <blts_log.h>
#include <blts_reporting.h>
#include <blts_timing.h>
#include <errno.h>
#include <limits.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blts-sqlite-perf.h"

/*
 * Intentionally SQL queries are not executed using prepared statements in actual test code. This
 * follows the approach chosen in the original sqlite benchmark where all queries are read from
 * files. The utility sql_sprintf() is to make SQL query building easier.
 */

enum { SQL_MAX = 256 };

static void sql_sprintf(char *str, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int n_written = vsnprintf(str, SQL_MAX, format, ap);
    assert(n_written < SQL_MAX);
}

/*
 * generate_test_data() allocates array of test_data_row instances filled with random values.
 */

enum { TEST_DATA_STRING_MAX = 256 };

typedef struct {
    int number;
    char string[TEST_DATA_STRING_MAX];
} test_data_row;

static test_data_row *generate_test_data(int n_rows);

static const char *digits[] = {
    "zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine"
};

/*
 * Logging-friendly encapsulation of common db manipulation routines.
 */

typedef struct {
    const char *function;
    int file_line;
} caller_info;
#define CALLER_INFO ((caller_info){__FUNCTION__, __LINE__})

#define db_open_truncate(db, db_file) db_open_truncate_(db, db_file, CALLER_INFO)
#define db_close(db) db_close_(db, CALLER_INFO)
#define db_create_table(db, name, n_rows) db_create_table_(db, name, n_rows, CALLER_INFO)
#define db_create_index(db, spec) db_create_index_(db, spec, CALLER_INFO)
#define db_exec(db, sql) db_exec_(db, sql, CALLER_INFO)
#define db_begin_transaction(db) db_begin_transaction_(db, CALLER_INFO)
#define db_commit_transaction(db) db_commit_transaction_(db, CALLER_INFO)

static bool db_open_truncate_(sqlite3 **db, const char *db_file, caller_info caller);
static void db_close_(sqlite3 *db, caller_info caller);
static bool db_create_table_(sqlite3 *db, const char *name, int n_rows, caller_info caller);
static bool db_create_index_(sqlite3 *db, const char *spec, caller_info caller);
static bool db_exec_(sqlite3 *db, const char *sql, caller_info caller);
static bool db_begin_transaction_(sqlite3 *db, caller_info caller);
static bool db_commit_transaction_(sqlite3 *db, caller_info caller);

/*
 * Other utilities
 */

// wrapper around blts_report_extended_result() combining tag_base with tag
static int report_extended_result(const char *tag_base, char *tag, double value, char *unit);

/*
 * Actual tests
 */

int test_insert(const char *tag_base, const char *db_file, bool in_transaction, bool with_index,
        int n_rows)
{
    BLTS_DEBUG("START %s(in_transaction=%d, with_index=%d, n_rows=%d)\n", __FUNCTION__,
            in_transaction, with_index, n_rows);

    int retv = EXIT_FAILURE;
    int i;
    sqlite3 *db = NULL;

    test_data_row *data = generate_test_data(n_rows);

    char (*inserts)[SQL_MAX] = calloc(n_rows, sizeof(char[SQL_MAX]));
    for (i = 0; i < n_rows; ++i) {
        sql_sprintf(inserts[i], "INSERT INTO t1 VALUES(%d, %d, '%s');", i, data[i].number,
                data[i].string);
    }

    if (!db_open_truncate(&db, db_file)) {
        goto fail;
    }

    timing_start();

    if (!db_create_table(db, "t1", 0)) {
        goto fail;
    }

    if (with_index && !db_create_index(db, "i1 on t1(c)")) {
        goto fail;
    }

    if (in_transaction && !db_begin_transaction(db)) {
        goto fail;
    }

    for (i = 0; i < n_rows; ++i) {
        if (!db_exec(db, inserts[i])) {
            goto fail;
        }
    }

    if (in_transaction && !db_commit_transaction(db)) {
        goto fail;
    }

    timing_stop();

    report_extended_result(tag_base, "elapsed", timing_elapsed(), "s");

    retv = EXIT_SUCCESS;

fail:
    db_close(db);
    free(inserts);
    free(data);

    return retv;
}

int test_select(const char *tag_base, const char *db_file, int table_size, bool with_index,
        int n_selects)
{
    BLTS_DEBUG("START %s(table_size=%d, with_index=%d, n_selects=%d)\n", __FUNCTION__, table_size,
            with_index, n_selects);

    int retv = EXIT_FAILURE;
    int i;
    sqlite3 *db = NULL;

    char (*selects)[SQL_MAX] = calloc(n_selects, sizeof(char[SQL_MAX]));
    for (i = 0; i < n_selects; ++i) {
        sql_sprintf(selects[i],  "SELECT count(*), avg(b) FROM t1 WHERE b >= %d AND b < %d;",
                i * 100, 1000 + i * 100);
    }

    if (!db_open_truncate(&db, db_file)) {
        goto fail;
    }

    if (!db_create_table(db, "t1", table_size)) {
        goto fail;
    }

    if (with_index && !db_create_index(db, "i1 on t1(b)")) {
        goto fail;
    }

    timing_start();

    if (!db_begin_transaction(db)) {
        goto fail;
    }

    for (i = 0; i < n_selects; ++i) {
        if (!db_exec(db, selects[i])) {
            goto fail;
        }
    }

    if (!db_commit_transaction(db)) {
        goto fail;
    }

    timing_stop();

    report_extended_result(tag_base, "elapsed", timing_elapsed(), "s");

    retv = EXIT_SUCCESS;

fail:
    db_close(db);
    free(selects);

    return retv;
}

int test_select_compare_strings(const char *tag_base, const char *db_file, int table_size,
        int n_selects)
{
    BLTS_DEBUG("START %s(table_size=%d, n_selects=%d)\n", __FUNCTION__, table_size, n_selects);

    assert(n_selects < 1000);

    int retv = EXIT_FAILURE;
    int i;
    sqlite3 *db = NULL;

    char (*selects)[SQL_MAX] = calloc(n_selects, sizeof(char[SQL_MAX]));
    for (i = 0; i < n_selects; ++i) {
        sql_sprintf(selects[i],  "SELECT count(*), avg(b) FROM t1 WHERE c LIKE '%%%s %s %s%%';",
                digits[(i / 100) % 10],
                digits[(i / 10) % 10],
                digits[(i / 1) % 10]);
    }

    if (!db_open_truncate(&db, db_file)) {
        goto fail;
    }

    if (!db_create_table(db, "t1", table_size)) {
        goto fail;
    }

    timing_start();

    if (!db_begin_transaction(db)) {
        goto fail;
    }

    for (i = 0; i < n_selects; ++i) {
        if (!db_exec(db, selects[i])) {
            goto fail;
        }
    }

    if (!db_commit_transaction(db)) {
        goto fail;
    }

    timing_stop();

    report_extended_result(tag_base, "elapsed", timing_elapsed(), "s");

    retv = EXIT_SUCCESS;

fail:
    db_close(db);
    free(selects);

    return retv;
}

int test_create_index(const char *tag_base, const char *db_file, int table_size)
{
    BLTS_DEBUG("START %s(table_size=%d)\n", __FUNCTION__, table_size);

    int retv = EXIT_FAILURE;
    sqlite3 *db = NULL;

    if (!db_open_truncate(&db, db_file)) {
        goto fail;
    }

    if (!db_create_table(db, "t1", table_size)) {
        goto fail;
    }

    timing_start();

    if (!db_exec(db, "CREATE INDEX i1a on t1(a);")) {
        goto fail;
    }

    if (!db_exec(db, "CREATE INDEX i1b on t1(b);")) {
        goto fail;
    }

    timing_stop();

    report_extended_result(tag_base, "elapsed", timing_elapsed(), "s");

    retv = EXIT_SUCCESS;

fail:
    db_close(db);

    return retv;
}

int test_update(const char *tag_base, const char *db_file, int table_size, bool with_index,
        int n_rows)
{
    BLTS_DEBUG("START %s(table_size=%d, with_index=%d, n_rows=%d)\n", __FUNCTION__, table_size,
            with_index, n_rows);

    int retv = EXIT_FAILURE;
    int i;
    sqlite3 *db = NULL;

    char (*updates)[SQL_MAX] = calloc(n_rows, sizeof(char[SQL_MAX]));
    for (i = 0; i < n_rows; ++i) {
        sql_sprintf(updates[i],  "UPDATE t1 SET b=b*2 WHERE a >= %d AND a < %d;",
                i * 10, (i + 1) * 10);
    }

    if (!db_open_truncate(&db, db_file)) {
        goto fail;
    }

    if (!db_create_table(db, "t1", table_size)) {
        goto fail;
    }

    if (with_index && !db_create_index(db, "i1a on t1(a)")) {
        goto fail;
    }

    if (with_index && !db_create_index(db, "i1b on t1(b)")) {
        goto fail;
    }

    timing_start();

    if (!db_begin_transaction(db)) {
        goto fail;
    }

    for (i = 0; i < n_rows; ++i) {
        if (!db_exec(db, updates[i])) {
            goto fail;
        }
    }

    if (!db_commit_transaction(db)) {
        goto fail;
    }

    timing_stop();

    report_extended_result(tag_base, "elapsed", timing_elapsed(), "s");

    retv = EXIT_SUCCESS;

fail:
    db_close(db);
    free(updates);

    return retv;
}

int test_update_strings(const char *tag_base, const char *db_file, int table_size, int n_rows)
{
    BLTS_DEBUG("START %s(table_size=%d, n_rows=%d)\n", __FUNCTION__, table_size, n_rows);

    int retv = EXIT_FAILURE;
    int i;
    sqlite3 *db = NULL;

    test_data_row *data = generate_test_data(n_rows);

    char (*updates)[SQL_MAX] = calloc(n_rows, sizeof(char[SQL_MAX]));
    for (i = 0; i < n_rows; ++i) {
        sql_sprintf(updates[i],  "UPDATE t1 SET c='%s' WHERE a = %d;", data[i].string, i);
    }

    if (!db_open_truncate(&db, db_file)) {
        goto fail;
    }

    if (!db_create_table(db, "t1", table_size)) {
        goto fail;
    }

    if (!db_create_index(db, "i1a on t1(a)")) {
        goto fail;
    }

    if (!db_create_index(db, "i1b on t1(b)")) {
        goto fail;
    }

    timing_start();

    if (!db_begin_transaction(db)) {
        goto fail;
    }

    for (i = 0; i < n_rows; ++i) {
        if (!db_exec(db, updates[i])) {
            goto fail;
        }
    }

    if (!db_commit_transaction(db)) {
        goto fail;
    }

    timing_stop();

    report_extended_result(tag_base, "elapsed", timing_elapsed(), "s");

    retv = EXIT_SUCCESS;

fail:
    db_close(db);
    free(updates);
    free(data);

    return retv;
}

int test_insert_from_select(const char *tag_base, const char *db_file, int table_size)
{
    BLTS_DEBUG("START %s(table_size=%d)\n", __FUNCTION__, table_size);

    int retv = EXIT_FAILURE;
    sqlite3 *db = NULL;

    if (!db_open_truncate(&db, db_file)) {
        goto fail;
    }

    if (!db_create_table(db, "t1", table_size)) {
        goto fail;
    }

    if (!db_create_table(db, "t2", table_size)) {
        goto fail;
    }

    if (!db_create_index(db, "i2a on t2(a)")) {
        goto fail;
    }

    if (!db_create_index(db, "i2b on t2(b)")) {
        goto fail;
    }

    timing_start();

    if (!db_begin_transaction(db)) {
        goto fail;
    }

    if (!db_exec(db, "INSERT INTO t1 SELECT b, a, c FROM t2")) {
        goto fail;
    }

    if (!db_exec(db, "INSERT INTO t2 SELECT b, a, c FROM t1")) {
        goto fail;
    }

    if (!db_commit_transaction(db)) {
        goto fail;
    }

    timing_stop();

    report_extended_result(tag_base, "elapsed", timing_elapsed(), "s");

    retv = EXIT_SUCCESS;

fail:
    db_close(db);

    return retv;
}

int test_delete(const char *tag_base, const char *db_file, int table_size, bool with_index)
{
    BLTS_DEBUG("START %s(table_size=%d, with_index=%d)\n", __FUNCTION__, table_size, with_index);

    int retv = EXIT_FAILURE;
    sqlite3 *db = NULL;

    if (!db_open_truncate(&db, db_file)) {
        goto fail;
    }

    if (!db_create_table(db, "t1", table_size)) {
        goto fail;
    }

    if (!db_create_index(db, "i1a on t1(a)")) {
        goto fail;
    }

    if (!db_create_index(db, "i1b on t1(b)")) {
        goto fail;
    }

    timing_start();

    if (!db_exec(db, with_index
                ? "DELETE FROM t1 WHERE a > 10 AND a < 20000;"
                : "DELETE FROM t1 WHERE c LIKE '%50%';")) {
        goto fail;
    }

    timing_stop();

    report_extended_result(tag_base, "elapsed", timing_elapsed(), "s");

    retv = EXIT_SUCCESS;

fail:
    db_close(db);

    return retv;
}

int test_big_insert_after_big_delete(const char *tag_base, const char *db_file, int table_size)
{
    BLTS_DEBUG("START %s(table_size=%d)\n", __FUNCTION__, table_size);

    int retv = EXIT_FAILURE;
    sqlite3 *db = NULL;

    if (!db_open_truncate(&db, db_file)) {
        goto fail;
    }

    if (!db_create_table(db, "t1", table_size)) {
        goto fail;
    }

    if (!db_create_table(db, "t2", table_size)) {
        goto fail;
    }

    if (!db_create_index(db, "i2a on t2(a)")) {
        goto fail;
    }

    if (!db_create_index(db, "i2b on t2(b)")) {
        goto fail;
    }

    if (!db_exec(db, "DELETE FROM t2 WHERE a > 10 AND A < 20000;")) {
        goto fail;
    }

    timing_start();

    if (!db_exec(db, "INSERT INTO t2 SELECT * FROM t1")) {
        goto fail;
    }

    timing_stop();

    report_extended_result(tag_base, "elapsed", timing_elapsed(), "s");

    retv = EXIT_SUCCESS;

fail:
    db_close(db);

    return retv;
}

int test_small_inserts_after_big_delete(const char *tag_base, const char *db_file, int table_size,
        int n_rows)
{
    BLTS_DEBUG("START %s(table_size=%d, n_rows=%d)\n", __FUNCTION__, table_size, n_rows);

    int retv = EXIT_FAILURE;
    int i;
    sqlite3 *db = NULL;

    test_data_row *data = generate_test_data(n_rows);

    char (*inserts)[SQL_MAX] = calloc(n_rows, sizeof(char[SQL_MAX]));
    for (i = 0; i < n_rows; ++i) {
        sql_sprintf(inserts[i], "INSERT INTO t1 VALUES(%d, %d, '%s');", i, data[i].number,
                data[i].string);
    }

    if (!db_open_truncate(&db, db_file)) {
        goto fail;
    }

    if (!db_create_table(db, "t1", table_size)) {
        goto fail;
    }

    if (!db_create_index(db, "i1a on t1(a)")) {
        goto fail;
    }

    if (!db_create_index(db, "i1b on t1(b)")) {
        goto fail;
    }

    timing_start();

    if (!db_begin_transaction(db)) {
        goto fail;
    }

    if (!db_exec(db, "DELETE FROM t1;")) {
        goto fail;
    }

    for (i = 0; i < n_rows; ++i) {
        if (!db_exec(db, inserts[i])) {
            goto fail;
        }
    }

    if (!db_commit_transaction(db)) {
        goto fail;
    }

    timing_stop();

    report_extended_result(tag_base, "elapsed", timing_elapsed(), "s");

    retv = EXIT_SUCCESS;

fail:
    db_close(db);
    free(inserts);
    free(data);

    return retv;
}

int test_drop_table(const char *tag_base, const char *db_file, int table_size)
{
    BLTS_DEBUG("START %s(table_size=%d)\n", __FUNCTION__, table_size);

    int retv = EXIT_FAILURE;
    sqlite3 *db = NULL;

    if (!db_open_truncate(&db, db_file)) {
        goto fail;
    }

    if (!db_create_table(db, "t1", table_size)) {
        goto fail;
    }

    if (!db_create_table(db, "t2", table_size)) {
        goto fail;
    }

    if (!db_create_table(db, "t3", table_size)) {
        goto fail;
    }

    if (!db_create_index(db, "i2a on t2(a)")) {
        goto fail;
    }

    if (!db_create_index(db, "i2b on t2(b)")) {
        goto fail;
    }

    if (!db_create_index(db, "i3 on t3(c)")) {
        goto fail;
    }

    timing_start();

    if (!db_exec(db, "DROP TABLE t1")) {
        goto fail;
    }

    if (!db_exec(db, "DROP TABLE t2")) {
        goto fail;
    }

    if (!db_exec(db, "DROP TABLE t3")) {
        goto fail;
    }

    timing_stop();

    report_extended_result(tag_base, "elapsed", timing_elapsed(), "s");

    retv = EXIT_SUCCESS;

fail:
    db_close(db);

    return retv;
}

/*
 * Utility function
 */

static test_data_row *generate_test_data(int n_rows)
{
    test_data_row *data = calloc(n_rows, sizeof(test_data_row));

    int i;
    for (i = 0; i < n_rows; ++i) {
        data[i].number = random() % 1000000;
        int n_written = snprintf(data[i].string, TEST_DATA_STRING_MAX, "%s %s %s %s %s %s",
                digits[(data[i].number / 100000) % 10],
                digits[(data[i].number / 10000) % 10],
                digits[(data[i].number / 1000) % 10],
                digits[(data[i].number / 100) % 10],
                digits[(data[i].number / 10) % 10],
                digits[(data[i].number / 1) % 10]);
        assert(n_written < TEST_DATA_STRING_MAX);
    }

    return data;
}

static bool db_open_truncate_(sqlite3 **db, const char *db_file, caller_info caller)
{
    int rc;

    if (access(db_file, F_OK) == 0) {
        rc = unlink(db_file);
        if (rc != 0) {
            BLTS_ERROR("%s:%d: %s: unlink(\"%s\") failed: %s\n", __FILE__, caller.file_line,
                    caller.function, db_file, strerror(errno));
            return false;
        }
    }

    rc = sqlite3_open_v2(db_file, db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 0);
    if (rc != SQLITE_OK) {
        BLTS_ERROR("%s:%d: %s: sqlite3_open_v2(\"%s\") failed: %s\n", __FILE__, caller.file_line,
                caller.function, db_file, sqlite3_errstr(rc));
        return false;
    }

    return true;
}

static void db_close_(sqlite3 *db, caller_info caller)
{
    int rc;

    rc = sqlite3_close(db);
    if (rc != SQLITE_OK) {
        BLTS_ERROR("%s:%d: %s: sqlite3_close() failed: %s\n", __FILE__, caller.file_line,
                caller.function, sqlite3_errstr(rc));
    }
}

static bool db_create_table_(sqlite3 *db, const char *name, int n_rows, caller_info caller)
{
    bool retv = false;
    int rc = 0;
    sqlite3_stmt *stmt = NULL;

    test_data_row *data = generate_test_data(n_rows);

    char create_table_sql[SQL_MAX];
    sql_sprintf(create_table_sql, "CREATE TABLE %s (a INTEGER, b INTEGER, c VARCHAR(100));", name);
    if (!db_exec_(db, create_table_sql, caller)) {
        goto fail;
    }

    // create an empty table?
    if (n_rows != 0) {
        if (!db_exec_(db, "BEGIN", caller)) {
            goto fail;
        }

        char insert_sql[SQL_MAX];
        sql_sprintf(insert_sql, "INSERT INTO %s VALUES(?1, ?2, ?3);", name);
        rc = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            BLTS_ERROR("%s:%d: %s: sqlite3_prepare(\"%s\") failed: %s\n", __FILE__,
                    caller.file_line, caller.function, insert_sql, sqlite3_errstr(rc));
            goto fail;
        }

        int i;
        for (i = 0; i < n_rows; ++i) {
            sqlite3_bind_int(stmt, 1, i);
            sqlite3_bind_int(stmt, 2, data[i].number);
            sqlite3_bind_text(stmt, 3, data[i].string, -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                BLTS_ERROR("%s:%d: %s: sqlite3_step(\"%s\") failed: %s\n", __FILE__,
                        caller.file_line, caller.function, insert_sql, sqlite3_errstr(rc));
                goto fail;
            }
        }

        if (!db_exec_(db, "COMMIT", caller)) {
            goto fail;
        }
    }

    retv = true;

fail:
    sqlite3_finalize(stmt);
    free(data);

    return retv;
}

static bool db_create_index_(sqlite3 *db, const char *spec, caller_info caller)
{
    char sql[SQL_MAX];
    sql_sprintf(sql, "CREATE INDEX %s;", spec);

    return db_exec_(db, sql, caller);
}

static bool db_exec_(sqlite3 *db, const char *sql, caller_info caller)
{
    int rc;
    char *errstr;

    rc = sqlite3_exec(db, sql, NULL, NULL, &errstr);
    if (rc != SQLITE_OK) {
        BLTS_ERROR("%s:%d: %s: sqlite3_exec(\"%s\") failed: %s\n", __FILE__, caller.file_line,
                caller.function, sql, sqlite3_errstr(rc));
        sqlite3_free(errstr);
        return false;
    }

    return true;
}

static bool db_begin_transaction_(sqlite3 *db, caller_info caller)
{
    return db_exec_(db, "BEGIN", caller);
}

static bool db_commit_transaction_(sqlite3 *db, caller_info caller)
{
    return db_exec_(db, "COMMIT", caller);
}

static int report_extended_result(const char *tag_base, char *tag, double value, char *unit)
{
    char full_tag[256];
    int n_written = snprintf(full_tag, 256, "%s.%s", tag_base, tag);
    assert(n_written < 256);

    return blts_report_extended_result(full_tag, value, unit, 0);
}
