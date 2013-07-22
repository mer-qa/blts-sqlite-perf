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

#define _GNU_SOURCE
#include <blts_cli_frontend.h>
#include <blts_log.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "blts-sqlite-perf.h"

typedef struct
{
    char db_file[PATH_MAX];
} test_execution_params;

static void help(const char* help_msg_base)
{
    fprintf(stdout, help_msg_base,
        "-f db-file"
        ,
        "-f: Database file path to use (pass ':memory:' to test on an in-memory database instance)\n"
        );
}

static void *argument_processor(int argc, char **argv)
{
    int i;
    test_execution_params* params = malloc(sizeof(test_execution_params));
    memset(params, 0, sizeof(test_execution_params));

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0) {
            if (++i >= argc) {
                return NULL;
            }

            int n_written = snprintf(params->db_file, PATH_MAX, "%s", argv[i]);
            if (n_written >= PATH_MAX) {
                BLTS_ERROR("%s: PATH_MAX exceeded\n", argv[i]);
                goto error;
            }
        } else {
            goto error;
        }
    }

    if (params->db_file[0] == '\0') {
        goto error;
    }

    return params;

error:
    free(params);
    return NULL;
}

static void teardown(void *user_ptr)
{
    if (user_ptr) {
        test_execution_params* params = user_ptr;
        free(params);
    }
}

static int exec_test(void* user_ptr, int test_num);

static blts_cli_testcase test_cases[] =
{
    { "insert_no_transaction", exec_test, 160000 },
    { "insert", exec_test, 20000 },
    { "insert_indexed", exec_test, 20000 },
    { "select", exec_test, 20000 },
    { "select_compare_strings", exec_test, 40000 },

    { "create_index", exec_test, 20000 },
    { "select_indexed", exec_test, 20000 },
    { "update", exec_test, 100000 },
    { "update_indexed", exec_test, 80000 },
    { "update_strings_indexed", exec_test, 20000 },

    { "insert_from_select", exec_test, 20000 },
    { "delete", exec_test, 20000 },
    { "delete_indexed", exec_test, 20000 },
    { "big_insert_after_big_delete", exec_test, 20000 },
    { "small_inserts_after_big_delete", exec_test, 20000 },

    { "drop_table", exec_test, 20000 },

    BLTS_CLI_END_OF_LIST
};

static int exec_test(void* user_ptr, int test_num)
{
    srand(time(NULL));

    test_execution_params* params = user_ptr;
    const char *tag_base = test_cases[test_num - 1].case_name;
    int rc = 0;

    switch(test_num)
    {
    case 1:
        rc = test_insert(tag_base, params->db_file, false, false, 1000);
        break;
    case 2:
        rc = test_insert(tag_base, params->db_file, true, false, 25000);
        break;
    case 3:
        rc = test_insert(tag_base, params->db_file, true, true, 25000);
        break;
    case 4:
        rc = test_select(tag_base, params->db_file, 25000, false, 100);
        break;
    case 5:
        rc = test_select_compare_strings(tag_base, params->db_file, 25000, 100);
        break;
    case 6:
        rc = test_create_index(tag_base, params->db_file, 25000);
        break;
    case 7:
        rc = test_select(tag_base, params->db_file, 25000, true, 5000);
        break;
    case 8:
        rc = test_update(tag_base, params->db_file, 25000, false, 1000);
        break;
    case 9:
        rc = test_update(tag_base, params->db_file, 25000, true, 25000);
        break;
    case 10:
        rc = test_update_strings(tag_base, params->db_file, 25000, 25000);
        break;
    case 11:
        rc = test_insert_from_select(tag_base, params->db_file, 25000);
        break;
    case 12:
        rc = test_delete(tag_base, params->db_file, 25000, false);
        break;
    case 13:
        rc = test_delete(tag_base, params->db_file, 25000, true);
        break;
    case 14:
        rc = test_big_insert_after_big_delete(tag_base, params->db_file, 25000);
        break;
    case 15:
        rc = test_small_inserts_after_big_delete(tag_base, params->db_file, 25000, 12000);
        break;
    case 16:
        rc = test_drop_table(tag_base, params->db_file, 25000);
        break;
    default:
        rc = -EINVAL;
        break;
    }

    return rc;
}

static blts_cli cli =
{
    .test_cases = test_cases,
    .log_file = "blts_sqlite_perf.txt",
    .blts_cli_help = help,
    .blts_cli_process_arguments = argument_processor,
    .blts_cli_teardown = teardown
};

int main(int argc, char **argv)
{
    return blts_cli_main(&cli, argc, argv);
}
