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

#ifndef BLTS_SQLITE_PERF_H
#define BLTS_SQLITE_PERF_H

#include <stdbool.h>

int test_insert(const char *tag_base, const char *db_file, bool in_transaction, bool with_index,
        int n_rows);
int test_select(const char *tag_base, const char *db_file, int table_size, bool with_index,
        int n_selects);
int test_select_compare_strings(const char *tag_base, const char *db_file, int table_size,
        int n_selects);
int test_create_index(const char *tag_base, const char *db_file, int table_size);
int test_update(const char *tag_base, const char *db_file, int table_size, bool with_index,
        int n_rows);
int test_update_strings(const char *tag_base, const char *db_file, int table_size, int n_rows);
int test_insert_from_select(const char *tag_base, const char *db_file, int table_size);
int test_delete(const char *tag_base, const char *db_file, int table_size, bool with_index);
int test_big_insert_after_big_delete(const char *tag_base, const char *db_file, int table_size);
int test_small_inserts_after_big_delete(const char *tag_base, const char *db_file, int table_size,
        int n_rows);
int test_drop_table(const char *tag_base, const char *db_file, int table_size);

#endif // BLTS_SQLITE_PERF_H
