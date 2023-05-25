/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "yyjson", "index.html", [
    [ "Introduction", "index.html", [
      [ "Features", "index.html#autotoc_md1", null ],
      [ "Limitations", "index.html#autotoc_md2", null ],
      [ "Performance", "index.html#autotoc_md3", null ],
      [ "Sample Code", "index.html#autotoc_md7", null ],
      [ "Documentation", "index.html#autotoc_md12", null ],
      [ "TODO", "index.html#autotoc_md13", null ],
      [ "License", "index.html#autotoc_md14", null ]
    ] ],
    [ "Building and testing", "md_doc__build_and_test.html", [
      [ "Source code", "md_doc__build_and_test.html#autotoc_md15", null ],
      [ "Package manager", "md_doc__build_and_test.html#autotoc_md16", [
        [ "Use vcpkg", "md_doc__build_and_test.html#autotoc_md17", null ]
      ] ],
      [ "CMake", "md_doc__build_and_test.html#autotoc_md18", [
        [ "Use CMake to build a library", "md_doc__build_and_test.html#autotoc_md19", null ],
        [ "Use CMake as a dependency", "md_doc__build_and_test.html#autotoc_md20", null ],
        [ "Use CMake to generate project", "md_doc__build_and_test.html#autotoc_md21", null ],
        [ "Use CMake to generate documentation", "md_doc__build_and_test.html#autotoc_md22", null ],
        [ "Testing With CMake and CTest", "md_doc__build_and_test.html#autotoc_md23", null ]
      ] ],
      [ "Compile-time Options", "md_doc__build_and_test.html#autotoc_md24", null ]
    ] ],
    [ "API", "md_doc__a_p_i.html", [
      [ "API Design", "md_doc__a_p_i.html#autotoc_md25", [
        [ "API prefix", "md_doc__a_p_i.html#autotoc_md26", null ],
        [ "API for immutable/mutable data", "md_doc__a_p_i.html#autotoc_md27", null ],
        [ "API for string", "md_doc__a_p_i.html#autotoc_md28", null ]
      ] ],
      [ "Reading JSON", "md_doc__a_p_i.html#autotoc_md30", [
        [ "Read JSON from string", "md_doc__a_p_i.html#autotoc_md31", null ],
        [ "Read JSON from file", "md_doc__a_p_i.html#autotoc_md32", null ],
        [ "Read JSON from file pointer", "md_doc__a_p_i.html#autotoc_md33", null ],
        [ "Read JSON with options", "md_doc__a_p_i.html#autotoc_md34", null ],
        [ "Reader flag", "md_doc__a_p_i.html#autotoc_md35", null ]
      ] ],
      [ "Writing JSON", "md_doc__a_p_i.html#autotoc_md37", [
        [ "Write JSON to string", "md_doc__a_p_i.html#autotoc_md38", null ],
        [ "Write JSON to file", "md_doc__a_p_i.html#autotoc_md39", null ],
        [ "Write JSON to file pointer", "md_doc__a_p_i.html#autotoc_md40", null ],
        [ "Write JSON with options", "md_doc__a_p_i.html#autotoc_md41", null ],
        [ "Writer flag", "md_doc__a_p_i.html#autotoc_md42", null ]
      ] ],
      [ "Accessing JSON Document", "md_doc__a_p_i.html#autotoc_md44", [
        [ "JSON Document", "md_doc__a_p_i.html#autotoc_md45", null ],
        [ "JSON Value", "md_doc__a_p_i.html#autotoc_md46", null ],
        [ "JSON Array", "md_doc__a_p_i.html#autotoc_md47", null ],
        [ "JSON Array Iterator", "md_doc__a_p_i.html#autotoc_md48", null ],
        [ "JSON Object", "md_doc__a_p_i.html#autotoc_md49", null ],
        [ "JSON Object Iterator", "md_doc__a_p_i.html#autotoc_md50", null ]
      ] ],
      [ "Creating JSON Document", "md_doc__a_p_i.html#autotoc_md52", [
        [ "Mutable Document", "md_doc__a_p_i.html#autotoc_md53", null ],
        [ "JSON Value Creation", "md_doc__a_p_i.html#autotoc_md54", null ],
        [ "JSON Array Creation", "md_doc__a_p_i.html#autotoc_md55", null ],
        [ "JSON Array Modification", "md_doc__a_p_i.html#autotoc_md56", null ],
        [ "JSON Object Creation", "md_doc__a_p_i.html#autotoc_md57", null ],
        [ "JSON Object Modification", "md_doc__a_p_i.html#autotoc_md58", null ]
      ] ],
      [ "JSON Pointer and Patch", "md_doc__a_p_i.html#autotoc_md60", [
        [ "JSON Pointer", "md_doc__a_p_i.html#autotoc_md61", null ],
        [ "JSON Patch", "md_doc__a_p_i.html#autotoc_md62", null ],
        [ "JSON Merge Patch", "md_doc__a_p_i.html#autotoc_md63", null ]
      ] ],
      [ "Number Processing", "md_doc__a_p_i.html#autotoc_md65", [
        [ "Number reader", "md_doc__a_p_i.html#autotoc_md66", null ],
        [ "Number writer", "md_doc__a_p_i.html#autotoc_md67", null ]
      ] ],
      [ "Text Processing", "md_doc__a_p_i.html#autotoc_md68", [
        [ "Character Encoding", "md_doc__a_p_i.html#autotoc_md69", null ],
        [ "NUL Character", "md_doc__a_p_i.html#autotoc_md70", null ]
      ] ],
      [ "Memory Allocator", "md_doc__a_p_i.html#autotoc_md71", [
        [ "Single allocator for multiple JSON", "md_doc__a_p_i.html#autotoc_md72", null ],
        [ "Stack memory allocator", "md_doc__a_p_i.html#autotoc_md73", null ],
        [ "Use a third-party allocator library", "md_doc__a_p_i.html#autotoc_md74", null ]
      ] ],
      [ "Stack Memory Usage", "md_doc__a_p_i.html#autotoc_md75", null ],
      [ "Null Check", "md_doc__a_p_i.html#autotoc_md76", null ],
      [ "Thread Safety", "md_doc__a_p_i.html#autotoc_md77", null ],
      [ "Locale Independence", "md_doc__a_p_i.html#autotoc_md78", null ]
    ] ],
    [ "Data Structures", "md_doc__data_structure.html", [
      [ "Immutable Value", "md_doc__data_structure.html#autotoc_md80", null ],
      [ "Immutable Document", "md_doc__data_structure.html#autotoc_md81", null ],
      [ "Mutable Value", "md_doc__data_structure.html#autotoc_md83", null ],
      [ "Mutable Document", "md_doc__data_structure.html#autotoc_md84", null ],
      [ "Memory Management", "md_doc__data_structure.html#autotoc_md86", null ]
    ] ],
    [ "Changelog", "md__c_h_a_n_g_e_l_o_g.html", [
      [ "0.7.0 (2023-05-25)", "md__c_h_a_n_g_e_l_o_g.html#autotoc_md88", null ],
      [ "0.6.0 (2022-12-12)", "md__c_h_a_n_g_e_l_o_g.html#autotoc_md92", null ],
      [ "0.5.1 (2022-06-17)", "md__c_h_a_n_g_e_l_o_g.html#autotoc_md95", null ],
      [ "0.5.0 (2022-05-25)", "md__c_h_a_n_g_e_l_o_g.html#autotoc_md97", null ],
      [ "0.4.0 (2021-12-12)", "md__c_h_a_n_g_e_l_o_g.html#autotoc_md101", null ],
      [ "0.3.0 (2021-05-25)", "md__c_h_a_n_g_e_l_o_g.html#autotoc_md105", null ],
      [ "0.2.0 (2020-12-12)", "md__c_h_a_n_g_e_l_o_g.html#autotoc_md109", null ],
      [ "0.1.0 (2020-10-26)", "md__c_h_a_n_g_e_l_o_g.html#autotoc_md114", null ]
    ] ],
    [ "Deprecated List", "deprecated.html", null ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "Globals", "globals.html", [
        [ "All", "globals.html", "globals_dup" ],
        [ "Functions", "globals_func.html", "globals_func" ],
        [ "Variables", "globals_vars.html", null ],
        [ "Typedefs", "globals_type.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ],
      [ "Data Structures", "annotated.html", [
        [ "Data Structures", "annotated.html", "annotated_dup" ],
        [ "Data Structure Index", "classes.html", null ],
        [ "Data Fields", "functions.html", [
          [ "All", "functions.html", null ],
          [ "Variables", "functions_vars.html", null ]
        ] ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"annotated.html",
"yyjson_8h.html#a4d30446a286f54e2f95847f3c6669493",
"yyjson_8h.html#add7037998fb39b3e2d1b3caf59f9d66a"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';