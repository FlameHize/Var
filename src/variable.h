// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#ifndef VAR_VARIABLE_H
#define VAR_VARIABLE_H

#include "net/base/noncopyable.h"
#include <ostream>
#include <string>
#include <vector>

namespace var {

// Bitwise masks of displayable targets
enum DisplayFilter {
    DISPLAY_ON_HTML = 1,
    DISPLAY_ON_PLAIN_TEXT = 2,
    DISPLAY_ON_ALL = 3,
};

// Implement this class to write variables into different places.
// If dump() returns false, Variable::dump_exposed() stops and returns -1
class Dumper {
public:
    virtual ~Dumper() {}
    virtual bool dump(const std::string& name, 
                      const std::string& description) = 0;
};

// Options for Variable::dump_exposed()
struct DumpOptions {
    // Constructed with default options.
    DumpOptions();

    // If this is true, string-type values will be quoted
    bool quote_string;

    // The ? in wildcards. Wildcards in URL need to use another
    // character becasuse ? is reserved.
    char question_mark;

    // Dump variables with matched display_filter.
    DisplayFilter display_filter;

    // Name matched by these wildcards (or exact names) are kept.
    std::string white_wildcards;

    // Name matched by these wildcards (or exact names) are skipped.
    std::string black_wildcards;
};

/**
 * @brief class of all var
 * @details About thread-safety
 *  var is thread-compatible:
 *      Namely you can create/destroy/expose/hide or do whatever you want
 *      to different var simultaneously in different threads.
 *  vart is NOT thread-safe:
 *      You should not operate one var from different threads simultaneously.
 *      If you need to, protect the ops with locks. Similarly with ordinary
 *      variables, const method are thread-safe, namely you can call 
 *      describle()/get_description()/get_value() etc from different threads
 *      safely (provided that there's no non-const methods going on).
*/
class Variable : public noncopyable {
    // Var uses TLS,thus copying/assignment neet to copy TLS stuff as well,
    // which is heavy. We disable copying/assignment now.
public:
    Variable() {}
    virtual ~Variable();

    // Implement this method to print the variable into ostream
    virtual void describe(std::ostream& os, bool quote_string) const = 0;

    // string from of describe()
    std::string get_description() const;

    ///@note The output will be ploted by js.plot
    // Describle saved series as a json-string into the stream.
    // Returns 0 on success, otherwise (this variable does not save series).
    virtual int describe_series(std::ostream& os) const {
        return 1;
    }

    // Expose this variable globally so that it's counted in following functions:
    // list_exposed()
    // count_exposed()
    // describe_exposed()
    // find_exposed()
    // Returns 0 on success, otherwise -1.
    ///@todo stringpiece
    int expose(const std::string& name, DisplayFilter display_filter = DISPLAY_ON_ALL) {
        return expose_impl(std::string(), name, display_filter);
    }

    // Expose this variable with a prefix
    // Returns 0 on success, otherwise -1.
    int expose_as(const std::string& prefix, const std::string& name,
                  DisplayFilter display_filter = DISPLAY_ON_ALL) {
        return expose_impl(prefix, name, display_filter);
    }

    // Hide this variable so that it's not counted in *_exposed functions.
    // Returns false if this variable is already hidden.
    // CAUTION!! SubClasses must call hide() manually to avoid displaying
    // a variable that is just destructing.
    bool hide();

    // Check if this variable is is_hidden.
    bool is_hidden() const;

    // Get exposed name. If this variable is not exposed, the name is empty.
    const std::string& name() const {
        return _name;
    }

    // ====================================================================

    // Put name of all exposed variables into 'names'.
    // If you want to print all variables, you have to go through 'names'
    // and call describe_exposed() on each name. 
    // This func internal prevents an iteration from taking the lock too long.
    static void list_exposed(std::vector<std::string>* names,
                             DisplayFilter = DISPLAY_ON_ALL); 

    // Get number of exposed variables.
    static size_t count_exposed();

    // Find an exposed variable by 'name' and put its description into 'os'.
    // Returns 0 on found, otherwise return -1.
    static int describe_exposed(const std::string& name,
                                std::ostream& os,
                                bool quote_string = false,
                                DisplayFilter = DISPLAY_ON_ALL);

    // String form. Returns empty string if not found.
    static std::string describe_exposed(const std::string& name,
                                        bool quote_string = false,
                                        DisplayFilter = DISPLAY_ON_ALL);

    ///@note The output will be ploted by js.plot.
    // Describe saved series of variable 'names' as a json-string into 'os'.
    // Returns 0 on success, 1 when the variables does not save series, 
    // -1 otherwise (no variable named 'names').
    static int describe_series_exposed(const std::string& name,
                                       std::ostream& os);

    // Find all exposed variables matching 'white_wildcards' but
    // 'black_wildcards' and send them to 'dumper'.
    // Use default options when 'options' is NULL.
    // Returns number of dumped variables, -1 on error.
    static int dump_exposed(Dumper* dumper, const DumpOptions* options);

protected:
    virtual int expose_impl(const std::string& prefix,
                            const std::string& name,
                            DisplayFilter = DISPLAY_ON_ALL);

private:
    std::string _name;
};

// Make the only use lowercased alphabets / digits / underscores,
// and append the result to 'out'.
void to_underscored_name(std::string* out,const std::string& name);

} // end namespace var

#endif