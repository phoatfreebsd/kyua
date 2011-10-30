// Copyright 2011 Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors
//   may be used to endorse or promote products derived from this software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <sstream>
#include <string>

#include "cli/cmd_db_exec.hpp"
#include "cli/common.ipp"
#include "store/backend.hpp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/exceptions.hpp"
#include "utils/sqlite/statement.hpp"

namespace cmdline = utils::cmdline;
namespace sqlite = utils::sqlite;
namespace user_files = engine::user_files;

using cli::cmd_db_exec;


namespace {


/// Concatenates a vector into a string using ' ' as a separator.
///
/// \param args The objects to join.  This cannot be empty.
///
/// \return The concatenation of all the objects in the set.
static std::string
flatten_args(const cmdline::args_vector& args)
{
    std::ostringstream output;
    std::copy(args.begin(), args.end(),
              std::ostream_iterator< std::string >(output, " "));

    std::string result = output.str();
    result.erase(result.end() - 1);
    return result;
}


/// Formats the column names of a statement for output as CSV.
///
/// \param stmt The statement whose columns to format.
///
/// \return A comma-separated list of column names.
static std::string
format_headers(sqlite::statement& stmt)
{
    std::string output;
    int i = 0;
    for (; i < stmt.column_count() - 1; ++i)
        output += stmt.column_name(i) + ',';
    output += stmt.column_name(i);
    return output;
}


/// Formats a particular cell of a statement result.
///
/// \param stmt The statement whose cell to format.
/// \param index The index of the cell to format.
///
/// \return A textual representation of the cell.
static std::string
format_cell(sqlite::statement& stmt, const int index)
{
    if (stmt.column_type(index) == sqlite::type_integer) {
        return F("%d") % stmt.column_int64(index);
    } else if (stmt.column_type(index) == sqlite::type_text) {
        return stmt.column_text(index);
    } else {
        return "REPRESENTATION-NOT-IMPLEMENTED";
    }
}


/// Formats a row of a statement for output as CSV.
///
/// \param stmt The statement whose current row to format.
///
/// \return A comma-separated list of values.
static std::string
format_row(sqlite::statement& stmt)
{
    std::string output;
    int i = 0;
    for (; i < stmt.column_count() - 1; ++i)
        output += format_cell(stmt, i) + ',';
    output += format_cell(stmt, i);
    return output;
}


}  // anonymous namespace


/// Default constructor for cmd_db_exec.
cmd_db_exec::cmd_db_exec(void) : cli_command(
    "db-exec", "sql_statement", 1, -1,
    "Executes an arbitrary SQL statement in the store database and prints "
    "the resulting table")
{
    add_option(store_option);
    add_option(cmdline::bool_option("no-headers", "Do not show headers in the "
                                    "output table"));
}


/// Entry point for the "db-exec" subcommand.
///
/// \param ui Object to interact with the I/O of the program.
/// \param cmdline Representation of the command line to the subcommand.
/// \param unused_config The runtime configuration of the program.
///
/// \return 0 if everything is OK, 1 if the statement is invalid or if there is
/// any other problem.
int
cmd_db_exec::run(cmdline::ui* ui, const cmdline::parsed_cmdline& cmdline,
                 const user_files::config& UTILS_UNUSED_PARAM(config))
{
    try {
        store::backend backend = store::backend::open_rw(
            cli::store_path(cmdline));
        sqlite::statement stmt = backend.database().create_statement(
            flatten_args(cmdline.arguments()));

        if (stmt.step()) {
            if (!cmdline.has_option("no-headers"))
                ui->out(format_headers(stmt));
            do
                ui->out(format_row(stmt));
            while (stmt.step());
        }

        return EXIT_SUCCESS;
    } catch (const sqlite::error& e) {
        cmdline::print_error(ui, F("SQLite error: %s") % e.what());
        return EXIT_FAILURE;
    }
}