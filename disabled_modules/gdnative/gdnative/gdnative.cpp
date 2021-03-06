/*************************************************************************/
/*  gdnative.cpp                                                         */
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

#include "gdnative/gdnative.h"

#include "core/class_db.h"
#include "core/engine.h"
#include "core/error_macros.h"
#include "core/global_constants.h"
#include "core/object_db.h"
#include "core/os/os.h"
#include "core/variant.h"
#include "core/print_string.h"
#include "core/string_formatter.h"
#include "core/method_bind_interface.h"

#include "modules/gdnative/gdnative.h"

#ifdef __cplusplus
extern "C" {
#endif

void GDAPI godot_object_destroy(godot_object *p_o) {
    memdelete((Object *)p_o);
}

// Singleton API

godot_object GDAPI *godot_global_get_singleton(char *p_name) {
    return (godot_object *)Engine::get_singleton()->get_named_singleton(String(p_name));
} // result shouldn't be freed

// MethodBind API

godot_method_bind GDAPI *godot_method_bind_get_method(const char *p_classname, const char *p_methodname) {

    MethodBind *mb = ClassDB::get_method(StringName(p_classname), StringName(p_methodname));
    // MethodBind *mb = ClassDB::get_method("Node", "get_name");
    return (godot_method_bind *)mb;
}

void GDAPI godot_method_bind_ptrcall(godot_method_bind *p_method_bind, godot_object *p_instance, const void **p_args, void *p_ret) {

    MethodBind *mb = (MethodBind *)p_method_bind;
    Object *o = (Object *)p_instance;
    mb->ptrcall(o, p_args, p_ret);
}

godot_variant GDAPI godot_method_bind_call(godot_method_bind *p_method_bind, godot_object *p_instance, const godot_variant **p_args, const int p_arg_count, godot_variant_call_error *p_call_error) {
    MethodBind *mb = (MethodBind *)p_method_bind;
    Object *o = (Object *)p_instance;
    const Variant **args = (const Variant **)p_args;

    godot_variant ret;
    godot_variant_new_nil(&ret);

    Variant *ret_val = (Variant *)&ret;

    Callable::CallError r_error;
    *ret_val = mb->call(o, args, p_arg_count, r_error);

    if (p_call_error) {
        p_call_error->error = (godot_variant_call_error_error)r_error.error;
        p_call_error->argument = r_error.argument;
        p_call_error->expected = (godot_variant_type)r_error.expected;
    }

    return ret;
}

godot_class_constructor GDAPI godot_get_class_constructor(const char *p_classname) {
    ClassDB::ClassInfo *class_info = ClassDB::classes.getptr(StringName(p_classname));
    if (class_info)
        return (godot_class_constructor)class_info->creation_func;
    return nullptr;
}

godot_dictionary GDAPI godot_get_global_constants() {
    godot_dictionary constants;
    godot_dictionary_new(&constants);
    Dictionary *p_constants = (Dictionary *)&constants;
    const int constants_count = GlobalConstants::get_global_constant_count();
    for (int i = 0; i < constants_count; ++i) {
        const char *name = GlobalConstants::get_global_constant_name(i);
        int value = GlobalConstants::get_global_constant_value(i);
        (*p_constants)[name] = value;
    }
    return constants;
}

// System functions
void GDAPI godot_register_native_call_type(const char *p_call_type, native_call_cb p_callback) {
    GDNativeCallRegistry::get_singleton()->register_native_call_type(StringName(p_call_type), p_callback);
}

void GDAPI *godot_alloc(int p_bytes) {
    return memalloc(p_bytes);
}

void GDAPI *godot_realloc(void *p_ptr, int p_bytes) {
    return memrealloc(p_ptr, p_bytes);
}

void GDAPI godot_free(void *p_ptr) {
    memfree(p_ptr);
}

void GDAPI godot_print_error(const char *p_description, const char *p_function, const char *p_file, int p_line) {
    _err_print_error(p_function, p_file, p_line, p_description, ERR_HANDLER_ERROR);
}

void GDAPI godot_print_warning(const char *p_description, const char *p_function, const char *p_file, int p_line) {
    _err_print_error(p_function, p_file, p_line, p_description, ERR_HANDLER_WARNING);
}

void GDAPI godot_print(const godot_string *p_message) {
    print_line(*(String *)p_message);
}

void _gdnative_report_version_mismatch(const godot_object *p_library, const char *p_ext, godot_gdnative_api_version p_want, godot_gdnative_api_version p_have) {
    String message("Error loading GDNative file ");
    GDNativeLibrary *library = (GDNativeLibrary *)p_library;

    message += String(library->get_current_library_path()) + ": Extension \"" + p_ext + "\" can't be loaded.\n";

    message += FormatVE("Got version %d.%d but needs %d.%d!",p_have.major,p_have.minor,p_want.major,p_want.minor);

    _err_print_error("gdnative_init", library->get_current_library_path().c_str(), 0, message);
}

void _gdnative_report_loading_error(const godot_object *p_library, const char *p_what) {
    String message("Error loading GDNative file ");
    GDNativeLibrary *library = (GDNativeLibrary *)p_library;

    message += library->get_current_library_path() + ": " + p_what;

    _err_print_error("gdnative_init", library->get_current_library_path().c_str(), 0, message);
}

bool GDAPI godot_is_instance_valid(const godot_object *p_object) {
    return gObjectDB().instance_validate((Object *)p_object);
}

#ifdef __cplusplus
}
#endif
