/*************************************************************************/
/*  doc_data.h                                                           */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#pragma once

#include "core/io/xml_parser.h"
#include "core/hash_map.h"
#include "core/string.h"

class DocData {
public:
    struct ArgumentDoc {

        String name;
        String type;
        String enumeration;
        String default_value;
    };

    struct MethodDoc {

        String name;
        String return_type;
        String return_enum;
        String qualifiers;
        String description;
        Vector<ArgumentDoc> arguments;
        bool operator<(const MethodDoc &p_md) const {
            return name < p_md.name;
        }
    };

    struct ConstantDoc {

        String name;
        String value;
        String enumeration;
        String description;
    };

    struct PropertyDoc {

        String name;
        StringName type;
        String enumeration;
        String description;
        String setter, getter;
        String default_value;
        bool overridden = false;
        bool operator<(const PropertyDoc &p_prop) const {
            return name < p_prop.name;
        }
    };

    struct ClassDoc {

        StringName name;
        StringName inherits;
        String category;
        String brief_description;
        String description;
        Vector<String> tutorials;
        Vector<MethodDoc> methods;
        Vector<MethodDoc> defined_signals;
        Vector<ConstantDoc> constants;
        Vector<PropertyDoc> properties;
        Vector<PropertyDoc> theme_properties;
    };

    String version;

    HashMap<StringName, ClassDoc> class_list;
    Error _load(Ref<XMLParser> parser);

public:
    void merge_from(const DocData &p_data);
    void remove_from(const DocData &p_data);
    void generate(bool p_basic_types = false);
    Error load_classes(StringView p_dir);
    static Error erase_classes(StringView p_dir);
    Error save_classes(StringView p_default_path, const HashMap<StringName, String> &p_class_path);

    Error load_compressed(const uint8_t *p_data, int p_compressed_size, int p_uncompressed_size);
};

