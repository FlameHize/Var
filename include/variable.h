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

// Date: 2024/05/13

#ifndef VAR_VARIABLE_H
#define VAR_VARIABLE_H

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
class Variable {
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
    virtual int describle_series(std::ostream& os) const {
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
        ///@todo
    }

    // Expose this variable with a prefix
    // Returns 0 on success, otherwise -1.
    int expose_as(const std::string& prefix, const std::string& name,
                  DisplayFilter display_filter = DISPLAY_ON_ALL) {
        ///@todo
    }

    // Get exposed name. If this variable is not exposed, the name is empty.
    const std::string& name() const {
        ///@todo
    }

    // Hide this variable so that it's not counted in *_exposed functions.
    // Returns false if this variable is already hidden.
    // CAUTION!! SubClasses must call hide() manually to avoid displaying
    // a variable that is just destructing.
    bool hide();

    // Find an exposed variable by 'name' and put its description into 'os'.
    // Returns 0 on found, otherwise return -1.
    static int describe_exposed(const std::string& name,
                                std::ostream& os,
                                bool quote_string = false,
                                DisplayFilter = DISPLAY_ON_ALL);

    // String form. Returns empty string if not found.
    std::string describe_exposed(const std::string& name,
                                 bool quote_string = false,
                                 DisplayFilter = DISPLAY_ON_ALL);

    ///@note The output will be ploted by js.plot.
    // Describe saved series of variable 'names' as a json-string into 'os'.
    // Returns 0 on success, 1 when the variables does not save series, 
    // -1 otherwise (no variable named 'names').
    static int describe_series_exposed(const std::string& name,
                                       std::ostream& os);

    // Put name of all exposed variables into 'names'.
    // If you want to print all variables, you have to go through 'names'
    // and call describe_exposed() on each name. 
    // This func internal prevents an iteration from taking the lock too long.
    static void list_exposed(std::vector<std::string>* names,
                             DisplayFilter = DISPLAY_ON_ALL); 

    // Get number of exposed variables.
    static size_t count_exposed();

    // Find all exposed variables matching 'white_wildcards' but
    // 'black_wildcards' and send them to 'dumper'.
    // Use default options when 'options' is NULL.
    // Returns number of dumped variables, -1 on error.
    ///@todo
    static int dump_exposed();

protected:
    virtual int exposed_impl(const std::string& prefix,
                             const std::string& name,
                             DisplayFilter = DISPLAY_ON_ALL);

private:
    std::string _name;

    ///@todo DISALLOW_COPY_AND_ASSIGN
};

} // end namespace var

#endif