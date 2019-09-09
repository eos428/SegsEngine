/*************************************************************************/
/*  project_settings.cpp                                                 */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
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

#include "project_settings.h"

#include "core/bind/core_bind.h"
#include "core/core_string_names.h"
#include "core/string_formatter.h"
#include "core/io/file_access_network.h"
#include "core/io/file_access_pack.h"
#include "core/io/marshalls.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "core/os/input_event.h"
#include "core/variant_parser.h"
#include "core/method_bind.h"

#include <zlib.h>

IMPL_GDCLASS(ProjectSettings)

ProjectSettings *ProjectSettings::singleton = nullptr;

ProjectSettings *ProjectSettings::get_singleton() {

    return singleton;
}

String ProjectSettings::get_resource_path() const {

    return resource_path;
};

String ProjectSettings::localize_path(String p_path) const {

    if (resource_path == "")
        return p_path; //not initialized yet

    if (StringUtils::begins_with(p_path,"res://") || StringUtils::begins_with(p_path,"user://") ||
            (PathUtils::is_abs_path(p_path) && !StringUtils::begins_with(p_path,resource_path)))
        return PathUtils::simplify_path(p_path);

    DirAccess *dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);

    String path = PathUtils::simplify_path(PathUtils::from_native_path(p_path));

    if (dir->change_dir(path) == OK) {

        String cwd = dir->get_current_dir();
        cwd = PathUtils::from_native_path(cwd);

        memdelete(dir);

        // Ensure that we end with a '/'.
        // This is important to ensure that we do not wrongly localize the resource path
        // in an absolute path that just happens to contain this string but points to a
        // different folder (e.g. "/my/project" as resource_path would be contained in
        // "/my/project_data", even though the latter is not part of res://.
        // `plus_file("")` is an easy way to ensure we have a trailing '/'.
        const String res_path = PathUtils::plus_file(resource_path,String(""));

        // DirAccess::get_current_dir() is not guaranteed to return a path that with a trailing '/',
        // so we must make sure we have it as well in order to compare with 'res_path'.
        cwd = PathUtils::plus_file(cwd,String(""));

        if (!StringUtils::begins_with(cwd,res_path)) {
            return p_path;
        }

        return StringUtils::replace_first(cwd,res_path, String("res://"));
    } else {

        memdelete(dir);

        int sep = StringUtils::find_last(path,'/');
        if (sep == -1) {
            return "res://" + path;
        }

        String parent = StringUtils::substr(path,0, sep);

        String plocal = localize_path(parent);
        if (plocal == "") {
            return String::null_val;
        }
        return plocal + StringUtils::substr(path,sep);
    }
}

void ProjectSettings::set_initial_value(const StringName &p_name, const Variant &p_value) {

    ERR_FAIL_COND(!props.has(p_name))
    props[p_name].initial = p_value;
}
void ProjectSettings::set_restart_if_changed(const StringName &p_name, bool p_restart) {

    ERR_FAIL_COND(!props.has(p_name))
    props[p_name].restart_if_changed = p_restart;
}

String ProjectSettings::globalize_path(String p_path) const {

    if (StringUtils::begins_with(p_path,"res://")) {

        if (resource_path != "") {

            return StringUtils::replace(p_path,String("res:/"), resource_path);
        }
        return StringUtils::replace(p_path,"res://", "");
    } else if (StringUtils::begins_with(p_path,"user://")) {

        String data_dir = OS::get_singleton()->get_user_data_dir();
        if (data_dir != "") {

            return StringUtils::replace(p_path,String("user:/"), data_dir);
        }
        return StringUtils::replace(p_path,String("user://"), String(""));
    }

    return p_path;
}

bool ProjectSettings::_set(const StringName &p_name, const Variant &p_value) {

    _THREAD_SAFE_METHOD_

    if (p_value.get_type() == Variant::NIL)
        props.erase(p_name);
    else {

        if (p_name == CoreStringNames::get_singleton()->_custom_features) {
            Vector<String> custom_feature_array = StringUtils::split(p_value,',');
            for (int i = 0; i < custom_feature_array.size(); i++) {

                custom_features.insert(custom_feature_array[i]);
            }
            return true;
        }

        if (!disable_feature_overrides) {
            int dot = StringUtils::find(p_name,".");
            if (dot != -1) {
                Vector<String> s = StringUtils::split(p_name,'.');

                bool override_valid = false;
                for (int i = 1; i < s.size(); i++) {
                    String feature =StringUtils::strip_edges( s[i]);
                    if (OS::get_singleton()->has_feature(feature) || custom_features.has(feature)) {
                        override_valid = true;
                        break;
                    }
                }

                if (override_valid) {

                    feature_overrides[s[0]] = p_name;
                }
            }
        }

        if (props.has(p_name)) {
            if (!props[p_name].overridden)
                props[p_name].variant = p_value;

        } else {
            props[p_name] = VariantContainer(p_value, last_order++);
        }
    }

    return true;
}
bool ProjectSettings::_get(const StringName &p_name, Variant &r_ret) const {

    _THREAD_SAFE_METHOD_

    StringName name = p_name;
    if (!disable_feature_overrides && feature_overrides.has(name)) {
        name = feature_overrides[name];
    }
    if (!props.has(name)) {
        WARN_PRINTS("Property not found: " + String(name))
        return false;
    }
    r_ret = props[name].variant;
    return true;
}

struct _VCSort {

    String name;
    Variant::Type type;
    int order;
    int flags;

    bool operator<(const _VCSort &p_vcs) const { return order == p_vcs.order ? name < p_vcs.name : order < p_vcs.order; }
};

void ProjectSettings::_get_property_list(List<PropertyInfo> *p_list) const {

    _THREAD_SAFE_METHOD_
    using namespace StringUtils;
    Set<_VCSort> vclist;

    for (Map<StringName, VariantContainer>::Element *E = props.front(); E; E = E->next()) {

        const VariantContainer *v = &E->get();

        if (v->hide_from_editor)
            continue;

        _VCSort vc;
        vc.name = E->key();
        vc.order = v->order;
        vc.type = v->variant.get_type();
        if (begins_with(vc.name, "input/") || begins_with(vc.name, "import/") || begins_with(vc.name, "export/") ||
                begins_with(vc.name, "/remap") || StringUtils::begins_with(vc.name,"/locale") || StringUtils::begins_with(vc.name,"/autoload"))
            vc.flags = PROPERTY_USAGE_STORAGE;
        else
            vc.flags = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE;

        if (v->restart_if_changed) {
            vc.flags |= PROPERTY_USAGE_RESTART_IF_CHANGED;
        }
        vclist.insert(vc);
    }

    for (Set<_VCSort>::Element *E = vclist.front(); E; E = E->next()) {

        String prop_info_name = E->get().name;
        int dot = StringUtils::find(prop_info_name,".");
        if (dot != -1)
            prop_info_name = StringUtils::substr(prop_info_name,0, dot);

        if (custom_prop_info.has(prop_info_name)) {
            PropertyInfo pi = custom_prop_info[prop_info_name];
            pi.name = E->get().name;
            pi.usage = E->get().flags;
            p_list->push_back(pi);
        } else
            p_list->push_back(PropertyInfo(E->get().type, StringUtils::to_utf8(E->get().name).data(), PROPERTY_HINT_NONE, nullptr, E->get().flags));
    }
}

bool ProjectSettings::_load_resource_pack(const String &p_pack) {

    if (PackedData::get_singleton()->is_disabled())
        return false;

    bool ok = PackedData::get_singleton()->add_pack(p_pack) == OK;

    if (!ok)
        return false;

    //if data.pck is found, all directory access will be from here
    DirAccess::make_default<DirAccessPack>(DirAccess::ACCESS_RESOURCES);
    using_datapack = true;

    return true;
}

void ProjectSettings::_convert_to_last_version(int p_from_version) {

    if (p_from_version <= 3) {
        // Converts the actions from array to dictionary (array of events to dictionary with deadzone + events)
        for (Map<StringName, ProjectSettings::VariantContainer>::Element *E = props.front(); E; E = E->next()) {
            Variant value = E->get().variant;
            if (StringUtils::begins_with(E->key(),"input/") && value.get_type() == Variant::ARRAY) {
                Array array = value;
                Dictionary action;
                action["deadzone"] = Variant(0.5f);
                action["events"] = array;
                E->get().variant = action;
            }
        }
    }
}

/*
 * This method is responsible for loading a project.godot file and/or data file
 * using the following merit order:
 *  - If using NetworkClient, try to lookup project file or fail.
 *  - If --main-pack was passed by the user (`p_main_pack`), load it or fail.
 *  - Search for .pck file matching binary name. There are two possibilities:
 *    o PathUtils::get_basename(exec_path) + '.pck' (e.g. 'win_game.exe' -> 'win_game.pck')
 *    o exec_path + '.pck' (e.g. 'linux_game' -> 'linux_game.pck')
 *    For each tentative, if the file exists, load it or fail.
 *  - On relevant platforms (Android/iOS), lookup project file in OS resource path.
 *    If found, load it or fail.
 *  - Lookup project file in passed `p_path` (--path passed by the user), i.e. we
 *    are running from source code.
 *    If not found and `p_upwards` is true (--upwards passed by the user), look for
 *    project files in parent folders up to the system root (used to run a game
 *    from command line while in a subfolder).
 *    If a project file is found, load it or fail.
 *    If nothing was found, error out.
 */
Error ProjectSettings::_setup(const String &p_path, const String &p_main_pack, bool p_upwards) {

    // If looking for files in a network client, use it directly

    if (FileAccessNetworkClient::get_singleton()) {

        Error err = _load_settings_text_or_binary(String("res://project.godot"), String("res://project.binary"));
        if (err == OK) {
            // Optional, we don't mind if it fails
            _load_settings_text(String("res://override.cfg"));
        }
        return err;
    }

    // Attempt with a user-defined main pack first

    if (p_main_pack != "") {

        bool ok = _load_resource_pack(p_main_pack);
        ERR_FAIL_COND_V(!ok, ERR_CANT_OPEN)

        Error err = _load_settings_text_or_binary(String("res://project.godot"), String("res://project.binary"));
        if (err == OK) {
            // Load override from location of the main pack
            // Optional, we don't mind if it fails
            _load_settings_text(PathUtils::plus_file(PathUtils::get_base_dir(p_main_pack),String("override.cfg")));
        }
        return err;
    }

    String exec_path = OS::get_singleton()->get_executable_path();

    if (exec_path != "") {
        // Attempt with exec_name.pck
        // (This is the usual case when distributing a Godot game.)

        // Based on the OS, it can be the exec path + '.pck' (Linux w/o extension, macOS in .app bundle)
        // or the exec path's basename + '.pck' (Windows).
        // We need to test both possibilities as extensions for Linux binaries are optional
        // (so both 'mygame.bin' and 'mygame' should be able to find 'mygame.pck').

        bool found = false;

        String exec_dir = PathUtils::get_base_dir(exec_path);
        String exec_filename = PathUtils::get_file(exec_path);
        String exec_basename = PathUtils::get_basename(exec_filename);

        // Try to load data pack at the location of the executable
        // As mentioned above, we have two potential names to attempt

        if (_load_resource_pack(PathUtils::plus_file(exec_dir,exec_basename + ".pck")) ||
                _load_resource_pack(PathUtils::plus_file(exec_dir,exec_filename + ".pck"))) {
            found = true;
        } else {
            // If we couldn't find them next to the executable, we attempt
            // the current working directory. Same story, two tests.
            if (_load_resource_pack(exec_basename + ".pck") ||
                    _load_resource_pack(exec_filename + ".pck")) {
                found = true;
            }
        }

        // Attempt with PCK bundled into executable

        if (!found) {
            if (_load_resource_pack(exec_path)) {
                found = true;
            }
        }

        // If we opened our package, try and load our project
        if (found) {
            Error err = _load_settings_text_or_binary(String("res://project.godot"), String("res://project.binary"));
            if (err == OK) {
                // Load override from location of executable
                // Optional, we don't mind if it fails
                _load_settings_text(PathUtils::plus_file(PathUtils::get_base_dir(exec_path),String("override.cfg")));
            }
            return err;
        }
    }

    // Try to use the filesystem for files, according to OS. (only Android -when reading from pck- and iOS use this)

    if (OS::get_singleton()->get_resource_dir() != "") {
        // OS will call ProjectSettings->get_resource_path which will be empty if not overridden!
        // If the OS would rather use a specific location, then it will not be empty.
        resource_path = StringUtils::replace(OS::get_singleton()->get_resource_dir(),"\\", "/");
        if (resource_path != "" && resource_path[resource_path.length() - 1] == '/') {
            resource_path = StringUtils::substr(resource_path,0, resource_path.length() - 1); // chop end
        }

        Error err = _load_settings_text_or_binary(String("res://project.godot"), String("res://project.binary"));
        if (err == OK) {
            // Optional, we don't mind if it fails
            _load_settings_text(String("res://override.cfg"));
        }
        return err;
    }

    // Nothing was found, try to find a project file in provided path (`p_path`)
    // or, if requested (`p_upwards`) in parent directories.

    DirAccess *d = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
    ERR_FAIL_COND_V(!d, ERR_CANT_CREATE);
    d->change_dir(p_path);

    String current_dir = d->get_current_dir();
    String candidate = current_dir;
    bool found = false;
    Error err;

    while (true) {
        err = _load_settings_text_or_binary(PathUtils::plus_file(current_dir,String("project.godot")), PathUtils::plus_file(current_dir,String("project.binary")));
        if (err == OK) {
            // Optional, we don't mind if it fails
            _load_settings_text(PathUtils::plus_file(current_dir,String("override.cfg")));
            candidate = current_dir;
            found = true;
            break;
        }

        if (p_upwards) {
            // Try to load settings ascending through parent directories
            d->change_dir(String(".."));
            if (d->get_current_dir() == current_dir)
                break; // not doing anything useful
            current_dir = d->get_current_dir();
        } else {
            break;
        }
    }

    resource_path = candidate;
    resource_path = PathUtils::from_native_path(resource_path); // windows path to unix path just in case
    memdelete(d);

    if (!found)
        return err;

    if (resource_path.length() && resource_path[resource_path.length() - 1] == '/')
        resource_path = StringUtils::substr(resource_path,0, resource_path.length() - 1); // chop end

    return OK;
}

Error ProjectSettings::setup(const String &p_path, const String &p_main_pack, bool p_upwards) {
    Error err = _setup(p_path, p_main_pack, p_upwards);
    if (err == OK) {
        String custom_settings = GLOBAL_DEF("application/config/project_settings_override", "");
        if (custom_settings != "") {
            _load_settings_text(custom_settings);
        }
    }

    return err;
}

bool ProjectSettings::has_setting(const StringName &p_var) const {

    _THREAD_SAFE_METHOD_

    return props.has(p_var);
}

void ProjectSettings::set_registering_order(bool p_enable) {

    registering_order = p_enable;
}

Error ProjectSettings::_load_settings_binary(const String &p_path) {

    Error err;
    FileAccess *f = FileAccess::open(p_path, FileAccess::READ, &err);
    if (err != OK) {
        return err;
    }

    uint8_t hdr[4];
    f->get_buffer(hdr, 4);
    if (hdr[0] != 'E' || hdr[1] != 'C' || hdr[2] != 'F' || hdr[3] != 'G') {

        memdelete(f);
        ERR_FAIL_V_CMSG(ERR_FILE_CORRUPT, "Corrupted header in binary project.binary (not ECFG).");
    }

    uint32_t count = f->get_32();

    for (uint32_t i = 0; i < count; i++) {

        uint32_t slen = f->get_32();
        CharString cs;
        cs.resize(slen + 1);
        cs[slen] = 0;
        f->get_buffer((uint8_t *)cs.data(), slen);
        String key = StringUtils::from_utf8(cs.data());

        uint32_t vlen = f->get_32();
        Vector<uint8_t> d;
        d.resize(vlen);
        f->get_buffer(d.ptrw(), vlen);
        Variant value;
        err = decode_variant(value, d.ptr(), d.size(), nullptr, true);
        ERR_CONTINUE_MSG(err != OK, "Error decoding property: " + key + ".")
        set(key, value);
    }

    return OK;
}

Error ProjectSettings::_load_settings_text(const String &p_path) {

    Error err;
    FileAccess *f = FileAccess::open(p_path, FileAccess::READ, &err);

    if (!f) {
        // FIXME: Above 'err' error code is ERR_FILE_CANT_OPEN if the file is missing
        // This needs to be streamlined if we want decent error reporting
        return ERR_FILE_NOT_FOUND;
    }

    VariantParser::StreamFile stream;
    stream.f = f;

    String assign;
    Variant value;
    VariantParser::Tag next_tag;

    int lines = 0;
    String error_text;
    String section;
    int config_version = 0;

    while (true) {

        assign = Variant();
        next_tag.fields.clear();
        next_tag.name = String();

        err = VariantParser::parse_tag_assign_eof(&stream, lines, error_text, next_tag, assign, value, nullptr, true);
        if (err == ERR_FILE_EOF) {
            memdelete(f);
            // If we're loading a project.godot from source code, we can operate some
            // ProjectSettings conversions if need be.
            _convert_to_last_version(config_version);
            return OK;
        } else if (err != OK) {
            ERR_PRINTS("Error parsing " + p_path + " at line " + itos(lines) + ": " + error_text + " File might be corrupted.");
            memdelete(f);
            return err;
        }

        if (assign != String()) {
            if (section == String() && assign == "config_version") {
                config_version = value;
                if (config_version > CONFIG_VERSION) {
                    memdelete(f);
                    ERR_FAIL_V_MSG(ERR_FILE_CANT_OPEN, FormatV("Can't open project at '%s', its `config_version` (%d) is from a more recent and incompatible version of the engine. Expected config version: %d.", StringUtils::to_utf8(p_path).data(), config_version, CONFIG_VERSION));
                }
            } else {
                if (section == String()) {
                    set(assign, value);
                } else {
                    set(section + "/" + assign, value);
                }
            }
        } else if (next_tag.name != String()) {
            section = next_tag.name;
        }
    }
}

Error ProjectSettings::_load_settings_text_or_binary(const String &p_text_path, const String &p_bin_path) {

    // Attempt first to load the text-based project.godot file
    Error err_text = _load_settings_text(p_text_path);
    if (err_text == OK) {
        return OK;
    } else if (err_text != ERR_FILE_NOT_FOUND) {
        // If the text-based file exists but can't be loaded, we want to know it
        ERR_PRINTS("Couldn't load file '" + p_text_path + "', error code " + itos(err_text) + ".")
        return err_text;
    }

    // Fallback to binary project.binary file if text-based was not found
    Error err_bin = _load_settings_binary(p_bin_path);
    return err_bin;
}

int ProjectSettings::get_order(const String &p_name) const {

    ERR_FAIL_COND_V(!props.has(p_name), -1)
    return props[p_name].order;
}

void ProjectSettings::set_order(const String &p_name, int p_order) {

    ERR_FAIL_COND(!props.has(p_name))
    props[p_name].order = p_order;
}

void ProjectSettings::set_builtin_order(const StringName &p_name) {
    ERR_FAIL_COND(!props.has(p_name))
    if (props[p_name].order >= NO_BUILTIN_ORDER_BASE) {
        props[p_name].order = last_builtin_order++;
    }
}

void ProjectSettings::clear(const String &p_name) {

    ERR_FAIL_COND(!props.has(p_name))
    props.erase(p_name);
}

Error ProjectSettings::save() {

    return save_custom(PathUtils::plus_file(get_resource_path(),String("project.godot")));
}

Error ProjectSettings::_save_settings_binary(const String &p_file, const Map<String, List<String> > &props, const CustomMap &p_custom, const String &p_custom_features) {

    Error err;
    FileAccess *file = FileAccess::open(p_file, FileAccess::WRITE, &err);
    ERR_FAIL_COND_V_MSG(err != OK, err, "Couldn't save project.binary at " + p_file + ".");

    uint8_t hdr[4] = { 'E', 'C', 'F', 'G' };
    file->store_buffer(hdr, 4);

    int count = 0;

    for (Map<String, List<String> >::Element *E = props.front(); E; E = E->next()) {

        for (List<String>::Element *F = E->get().front(); F; F = F->next()) {

            count++;
        }
    }

    if (p_custom_features != String()) {
        file->store_32(count + 1);
        //store how many properties are saved, add one for custom featuers, which must always go first
        String key = CoreStringNames::get_singleton()->_custom_features;
        file->store_32(key.length());
        file->store_string(key);

        int len;
        err = encode_variant(p_custom_features, nullptr, len, false);
        if (err != OK) {
            memdelete(file);
            ERR_FAIL_V(err);
        }

        Vector<uint8_t> buff;
        buff.resize(len);

        err = encode_variant(p_custom_features, buff.ptrw(), len, false);
        if (err != OK) {
            memdelete(file);
            ERR_FAIL_V(err);
        }
        file->store_32(len);
        file->store_buffer(buff.ptr(), buff.size());

    } else {
        file->store_32(count); //store how many properties are saved
    }

    for (Map<String, List<String> >::Element *E = props.front(); E; E = E->next()) {

        for (List<String>::Element *F = E->get().front(); F; F = F->next()) {

            String key = F->get();
            if (E->key() != "")
                key = E->key() + "/" + key;
            Variant value;
            if (p_custom.has(key))
                value = p_custom[key];
            else
                value = get(key);

            file->store_32(key.length());
            file->store_string(key);

            int len;
            err = encode_variant(value, nullptr, len, true);
            if (err != OK)
                memdelete(file);
            ERR_FAIL_COND_V(err != OK, ERR_INVALID_DATA);

            Vector<uint8_t> buff;
            buff.resize(len);

            err = encode_variant(value, buff.ptrw(), len, true);
            if (err != OK)
                memdelete(file);
            ERR_FAIL_COND_V(err != OK, ERR_INVALID_DATA);
            file->store_32(len);
            file->store_buffer(buff.ptr(), buff.size());
        }
    }

    file->close();
    memdelete(file);

    return OK;
}

Error ProjectSettings::_save_settings_text(const String &p_file, const Map<String, List<String> > &props, const CustomMap &p_custom, const String &p_custom_features) {

    Error err;
    FileAccess *file = FileAccess::open(p_file, FileAccess::WRITE, &err);

    ERR_FAIL_COND_V_MSG(err != OK, err, "Couldn't save project.godot - " + p_file + ".")

    file->store_line(String("; Engine configuration file."));
    file->store_line(String("; It's best edited using the editor UI and not directly,"));
    file->store_line(String("; since the parameters that go here are not all obvious."));
    file->store_line(String(";"));
    file->store_line(String("; Format:"));
    file->store_line(String(";   [section] ; section goes between []"));
    file->store_line(String(";   param=value ; assign values to parameters"));
    file->store_line(String(""));

    file->store_string("config_version=" + itos(CONFIG_VERSION) + "\n");
    if (p_custom_features != String())
        file->store_string("custom_features=\"" + p_custom_features + "\"\n");
    file->store_string(String("\n"));

    for (Map<String, List<String> >::Element *E = props.front(); E; E = E->next()) {

        if (E != props.front())
            file->store_string(String("\n"));

        if (E->key() != "")
            file->store_string("[" + E->key() + "]\n\n");
        for (List<String>::Element *F = E->get().front(); F; F = F->next()) {

            String key = F->get();
            if (E->key() != "")
                key = E->key() + "/" + key;
            Variant value;
            if (p_custom.has(key))
                value = p_custom[key];
            else
                value = get(key);

            String vstr;
            VariantWriter::write_to_string(value, vstr);
            if (StringUtils::contains(F->get(),' '))
                file->store_string(StringUtils::quote(F->get()) + "=" + vstr + "\n");
            else
                file->store_string(F->get() + "=" + vstr + "\n");
        }
    }

    file->close();
    memdelete(file);

    return OK;
}

Error ProjectSettings::_save_custom_bnd(const String &p_file) { // add other params as dictionary and array?

    return save_custom(p_file);
};

Error ProjectSettings::save_custom(const String &p_path, const CustomMap &p_custom, const Vector<String> &p_custom_features, bool p_merge_with_current) {

    ERR_FAIL_COND_V(p_path == "", ERR_INVALID_PARAMETER);

    Set<_VCSort> vclist;

    if (p_merge_with_current) {
        for (Map<StringName, VariantContainer>::Element *G = props.front(); G; G = G->next()) {

            const VariantContainer *v = &G->get();

            if (v->hide_from_editor)
                continue;

            if (p_custom.has(G->key()))
                continue;

            _VCSort vc;
            vc.name = G->key(); //*k;
            vc.order = v->order;
            vc.type = v->variant.get_type();
            vc.flags = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE;
            if (v->variant == v->initial)
                continue;

            vclist.insert(vc);
        }
    }

    for (const Map<String, Variant>::Element *E = p_custom.front(); E; E = E->next()) {

        // Lookup global prop to store in the same order
        Map<StringName, VariantContainer>::Element *global_prop = props.find(E->key());

        _VCSort vc;
        vc.name = E->key();
        vc.order = global_prop ? global_prop->get().order : 0xFFFFFFF;
        vc.type = E->get().get_type();
        vc.flags = PROPERTY_USAGE_STORAGE;
        vclist.insert(vc);
    }

    Map<String, List<String> > props;

    for (Set<_VCSort>::Element *E = vclist.front(); E; E = E->next()) {

        String category = E->get().name;
        String name = E->get().name;

        int div = StringUtils::find(category,"/");

        if (div < 0)
            category = "";
        else {

            category = StringUtils::substr(category,0, div);
            name = StringUtils::substr(name,div + 1);
        }
        props[category].push_back(name);
    }

    String custom_features;

    for (int i = 0; i < p_custom_features.size(); i++) {
        if (i > 0)
            custom_features += ',';

        String f =StringUtils::replace(StringUtils::strip_edges( p_custom_features[i]),"\"", "");
        custom_features += f;
    }

    if (StringUtils::ends_with(p_path,String(".godot")))
        return _save_settings_text(p_path, props, p_custom, custom_features);
    else if (StringUtils::ends_with(p_path,String(".binary")))
        return _save_settings_binary(p_path, props, p_custom, custom_features);
    else {

        ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED, "Unknown config file format: " + p_path + ".")
    }
}

Variant _GLOBAL_DEF(const StringName &p_var, const Variant &p_default, bool p_restart_if_changed) {

    Variant ret;
    if (!ProjectSettings::get_singleton()->has_setting(p_var)) {
        ProjectSettings::get_singleton()->set(p_var, p_default);
    }
    ret = ProjectSettings::get_singleton()->get(p_var);

    ProjectSettings::get_singleton()->set_initial_value(p_var, p_default);
    ProjectSettings::get_singleton()->set_builtin_order(p_var);
    ProjectSettings::get_singleton()->set_restart_if_changed(p_var, p_restart_if_changed);
    return ret;
}
Vector<String> ProjectSettings::get_optimizer_presets() const {

    List<PropertyInfo> pi;
    ProjectSettings::get_singleton()->get_property_list(&pi);
    Vector<String> names;

    for (List<PropertyInfo>::Element *E = pi.front(); E; E = E->next()) {

        if (!StringUtils::begins_with(E->get().name,"optimizer_presets/"))
            continue;
        names.push_back(StringUtils::get_slice(E->get().name,'/', 1));
    }

    names.sort();

    return names;
}

void ProjectSettings::_add_property_info_bind(const Dictionary &p_info) {

    ERR_FAIL_COND(!p_info.has("name"))
    ERR_FAIL_COND(!p_info.has("type"))

    PropertyInfo pinfo;
    pinfo.name = p_info["name"];
    ERR_FAIL_COND(!props.has(pinfo.name))
    pinfo.type = Variant::Type(p_info["type"].operator int());
    ERR_FAIL_INDEX(pinfo.type, Variant::VARIANT_MAX)

    if (p_info.has("hint"))
        pinfo.hint = PropertyHint(p_info["hint"].operator int());
    if (p_info.has("hint_string"))
        pinfo.hint_string = p_info["hint_string"];

    set_custom_property_info(StringName(pinfo.name), pinfo);
}

void ProjectSettings::set_custom_property_info(const StringName &p_prop, const PropertyInfo &p_info) {

    ERR_FAIL_COND(!props.has(p_prop))
    custom_prop_info[p_prop] = p_info;
    custom_prop_info[p_prop].name = p_prop;
}

const Map<StringName, PropertyInfo> &ProjectSettings::get_custom_property_info() const {
    return custom_prop_info;
}

void ProjectSettings::set_disable_feature_overrides(bool p_disable) {

    disable_feature_overrides = p_disable;
}

bool ProjectSettings::is_using_datapack() const {

    return using_datapack;
}

bool ProjectSettings::property_can_revert(const String &p_name) {

    if (!props.has(p_name))
        return false;

    return props[p_name].initial != props[p_name].variant;
}

Variant ProjectSettings::property_get_revert(const String &p_name) {

    if (!props.has(p_name))
        return Variant();

    return props[p_name].initial;
}

void ProjectSettings::set_setting(const String &p_setting, const Variant &p_value) {
    set(p_setting, p_value);
}

Variant ProjectSettings::get_setting(const String &p_setting) const {
    return get(p_setting);
}

bool ProjectSettings::has_custom_feature(const String &p_feature) const {
    return custom_features.has(p_feature);
}

void ProjectSettings::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("has_setting", "name"), &ProjectSettings::has_setting);
    MethodBinder::bind_method(D_METHOD("set_setting", "name", "value"), &ProjectSettings::set_setting);
    MethodBinder::bind_method(D_METHOD("get_setting", "name"), &ProjectSettings::get_setting);
    MethodBinder::bind_method(D_METHOD("set_order", "name", "position"), &ProjectSettings::set_order);
    MethodBinder::bind_method(D_METHOD("get_order", "name"), &ProjectSettings::get_order);
    MethodBinder::bind_method(D_METHOD("set_initial_value", "name", "value"), &ProjectSettings::set_initial_value);
    MethodBinder::bind_method(D_METHOD("add_property_info", "hint"), &ProjectSettings::_add_property_info_bind);
    MethodBinder::bind_method(D_METHOD("clear", "name"), &ProjectSettings::clear);
    MethodBinder::bind_method(D_METHOD("localize_path", "path"), &ProjectSettings::localize_path);
    MethodBinder::bind_method(D_METHOD("globalize_path", "path"), &ProjectSettings::globalize_path);
    MethodBinder::bind_method(D_METHOD("save"), &ProjectSettings::save);
    MethodBinder::bind_method(D_METHOD("load_resource_pack", "pack"), &ProjectSettings::_load_resource_pack);
    MethodBinder::bind_method(D_METHOD("property_can_revert", "name"), &ProjectSettings::property_can_revert);
    MethodBinder::bind_method(D_METHOD("property_get_revert", "name"), &ProjectSettings::property_get_revert);

    MethodBinder::bind_method(D_METHOD("save_custom", "file"), &ProjectSettings::_save_custom_bnd);
}

ProjectSettings::ProjectSettings() {

    singleton = this;
    last_order = NO_BUILTIN_ORDER_BASE;
    last_builtin_order = 0;
    disable_feature_overrides = false;
    registering_order = true;

    Array events;
    Dictionary action;
    Ref<InputEventKey> key;
    Ref<InputEventJoypadButton> joyb;

    GLOBAL_DEF("application/config/name", "");
    GLOBAL_DEF("application/config/description", "");
    custom_prop_info[StaticCString("application/config/description")] = PropertyInfo(Variant::STRING, "application/config/description", PROPERTY_HINT_MULTILINE_TEXT);
    GLOBAL_DEF("application/run/main_scene", "");
    custom_prop_info[StaticCString("application/run/main_scene")] = PropertyInfo(Variant::STRING, "application/run/main_scene", PROPERTY_HINT_FILE, "*.tscn,*.scn,*.res");
    GLOBAL_DEF("application/run/disable_stdout", false);
    GLOBAL_DEF("application/run/disable_stderr", false);
    GLOBAL_DEF("application/config/use_custom_user_dir", false);
    GLOBAL_DEF("application/config/custom_user_dir_name", "");
    GLOBAL_DEF("application/config/project_settings_override", "");
    GLOBAL_DEF("audio/default_bus_layout", "res://default_bus_layout.tres");
    custom_prop_info[StaticCString("audio/default_bus_layout")] = PropertyInfo(Variant::STRING, "audio/default_bus_layout", PROPERTY_HINT_FILE, "*.tres");

    PoolStringArray extensions = PoolStringArray();
    extensions.push_back(String("gd"));
    if (Engine::get_singleton()->has_singleton(String("GodotSharp")))
        extensions.push_back(String("cs"));
    extensions.push_back(String("shader"));

    GLOBAL_DEF("editor/search_in_file_extensions", extensions);
    custom_prop_info[StaticCString("editor/search_in_file_extensions")] = PropertyInfo(Variant::POOL_STRING_ARRAY, "editor/search_in_file_extensions");

    GLOBAL_DEF("editor/script_templates_search_path", "res://script_templates");
    custom_prop_info[StaticCString("editor/script_templates_search_path")] = PropertyInfo(Variant::STRING, "editor/script_templates_search_path", PROPERTY_HINT_DIR);

    action = Dictionary();
    action["deadzone"] = Variant(0.5f);
    events = Array();
    key.instance();
    key->set_scancode(KEY_ENTER);
    events.push_back(key);
    key.instance();
    key->set_scancode(KEY_KP_ENTER);
    events.push_back(key);
    key.instance();
    key->set_scancode(KEY_SPACE);
    events.push_back(key);
    joyb.instance();
    joyb->set_button_index(JOY_BUTTON_0);
    events.push_back(joyb);
    action["events"] = events;
    GLOBAL_DEF("input/ui_accept", action);
    input_presets.push_back(String("input/ui_accept"));

    action = Dictionary();
    action["deadzone"] = Variant(0.5f);
    events = Array();
    key.instance();
    key->set_scancode(KEY_SPACE);
    events.push_back(key);
    joyb.instance();
    joyb->set_button_index(JOY_BUTTON_3);
    events.push_back(joyb);
    action["events"] = events;
    GLOBAL_DEF("input/ui_select", action);
    input_presets.push_back(String("input/ui_select"));

    action = Dictionary();
    action["deadzone"] = Variant(0.5f);
    events = Array();
    key.instance();
    key->set_scancode(KEY_ESCAPE);
    events.push_back(key);
    joyb.instance();
    joyb->set_button_index(JOY_BUTTON_1);
    events.push_back(joyb);
    action["events"] = events;
    GLOBAL_DEF("input/ui_cancel", action);
    input_presets.push_back(String("input/ui_cancel"));

    action = Dictionary();
    action["deadzone"] = Variant(0.5f);
    events = Array();
    key.instance();
    key->set_scancode(KEY_TAB);
    events.push_back(key);
    action["events"] = events;
    GLOBAL_DEF("input/ui_focus_next", action);
    input_presets.push_back(String("input/ui_focus_next"));

    action = Dictionary();
    action["deadzone"] = Variant(0.5f);
    events = Array();
    key.instance();
    key->set_scancode(KEY_TAB);
    key->set_shift(true);
    events.push_back(key);
    action["events"] = events;
    GLOBAL_DEF("input/ui_focus_prev", action);
    input_presets.push_back(String("input/ui_focus_prev"));

    action = Dictionary();
    action["deadzone"] = Variant(0.5f);
    events = Array();
    key.instance();
    key->set_scancode(KEY_LEFT);
    events.push_back(key);
    joyb.instance();
    joyb->set_button_index(JOY_DPAD_LEFT);
    events.push_back(joyb);
    action["events"] = events;
    GLOBAL_DEF("input/ui_left", action);
    input_presets.push_back(String("input/ui_left"));

    action = Dictionary();
    action["deadzone"] = Variant(0.5f);
    events = Array();
    key.instance();
    key->set_scancode(KEY_RIGHT);
    events.push_back(key);
    joyb.instance();
    joyb->set_button_index(JOY_DPAD_RIGHT);
    events.push_back(joyb);
    action["events"] = events;
    GLOBAL_DEF("input/ui_right", action);
    input_presets.push_back(String("input/ui_right"));

    action = Dictionary();
    action["deadzone"] = Variant(0.5f);
    events = Array();
    key.instance();
    key->set_scancode(KEY_UP);
    events.push_back(key);
    joyb.instance();
    joyb->set_button_index(JOY_DPAD_UP);
    events.push_back(joyb);
    action["events"] = events;
    GLOBAL_DEF("input/ui_up", action);
    input_presets.push_back(String("input/ui_up"));

    action = Dictionary();
    action["deadzone"] = Variant(0.5f);
    events = Array();
    key.instance();
    key->set_scancode(KEY_DOWN);
    events.push_back(key);
    joyb.instance();
    joyb->set_button_index(JOY_DPAD_DOWN);
    events.push_back(joyb);
    action["events"] = events;
    GLOBAL_DEF("input/ui_down", action);
    input_presets.push_back(String("input/ui_down"));

    action = Dictionary();
    action["deadzone"] = Variant(0.5f);
    events = Array();
    key.instance();
    key->set_scancode(KEY_PAGEUP);
    events.push_back(key);
    action["events"] = events;
    GLOBAL_DEF("input/ui_page_up", action);
    input_presets.push_back(String("input/ui_page_up"));

    action = Dictionary();
    action["deadzone"] = Variant(0.5f);
    events = Array();
    key.instance();
    key->set_scancode(KEY_PAGEDOWN);
    events.push_back(key);
    action["events"] = events;
    GLOBAL_DEF("input/ui_page_down", action);
    input_presets.push_back(String("input/ui_page_down"));

    action = Dictionary();
    action["deadzone"] = Variant(0.5f);
    events = Array();
    key.instance();
    key->set_scancode(KEY_HOME);
    events.push_back(key);
    action["events"] = events;
    GLOBAL_DEF("input/ui_home", action);
    input_presets.push_back(String("input/ui_home"));

    action = Dictionary();
    action["deadzone"] = Variant(0.5f);
    events = Array();
    key.instance();
    key->set_scancode(KEY_END);
    events.push_back(key);
    action["events"] = events;
    GLOBAL_DEF("input/ui_end", action);
    input_presets.push_back(String("input/ui_end"));

    custom_prop_info[StaticCString("display/window/handheld/orientation")] = PropertyInfo(Variant::STRING, "display/window/handheld/orientation", PROPERTY_HINT_ENUM, "landscape,portrait,reverse_landscape,reverse_portrait,sensor_landscape,sensor_portrait,sensor");
    custom_prop_info[StaticCString("rendering/threads/thread_model")] = PropertyInfo(Variant::INT, "rendering/threads/thread_model", PROPERTY_HINT_ENUM, "Single-Unsafe,Single-Safe,Multi-Threaded");
    custom_prop_info[StaticCString("physics/2d/thread_model")] = PropertyInfo(Variant::INT, "physics/2d/thread_model", PROPERTY_HINT_ENUM, "Single-Unsafe,Single-Safe,Multi-Threaded");
    custom_prop_info[StaticCString("rendering/quality/intended_usage/framebuffer_allocation")] = PropertyInfo(Variant::INT, "rendering/quality/intended_usage/framebuffer_allocation", PROPERTY_HINT_ENUM, "2D,2D Without Sampling,3D,3D Without Effects");

    GLOBAL_DEF("debug/settings/profiler/max_functions", 16384);
    custom_prop_info[StaticCString("debug/settings/profiler/max_functions")] = PropertyInfo(Variant::INT, "debug/settings/profiler/max_functions", PROPERTY_HINT_RANGE, "128,65535,1");

    //assigning here, because using GLOBAL_GET on every block for compressing can be slow
    Compression::zstd_long_distance_matching = GLOBAL_DEF("compression/formats/zstd/long_distance_matching", false);
    custom_prop_info[StaticCString("compression/formats/zstd/long_distance_matching")] = PropertyInfo(Variant::BOOL, "compression/formats/zstd/long_distance_matching");
    Compression::zstd_level = GLOBAL_DEF("compression/formats/zstd/compression_level", 3);
    custom_prop_info[StaticCString("compression/formats/zstd/compression_level")] = PropertyInfo(Variant::INT, "compression/formats/zstd/compression_level", PROPERTY_HINT_RANGE, "1,22,1");
    Compression::zstd_window_log_size = GLOBAL_DEF("compression/formats/zstd/window_log_size", 27);
    custom_prop_info[StaticCString("compression/formats/zstd/window_log_size")] = PropertyInfo(Variant::INT, "compression/formats/zstd/window_log_size", PROPERTY_HINT_RANGE, "10,30,1");

    Compression::zlib_level = GLOBAL_DEF("compression/formats/zlib/compression_level", Z_DEFAULT_COMPRESSION);
    custom_prop_info[StaticCString("compression/formats/zlib/compression_level")] = PropertyInfo(Variant::INT, "compression/formats/zlib/compression_level", PROPERTY_HINT_RANGE, "-1,9,1");

    Compression::gzip_level = GLOBAL_DEF("compression/formats/gzip/compression_level", Z_DEFAULT_COMPRESSION);
    custom_prop_info[StaticCString("compression/formats/gzip/compression_level")] = PropertyInfo(Variant::INT, "compression/formats/gzip/compression_level", PROPERTY_HINT_RANGE, "-1,9,1");

    // Would ideally be defined in an Android-specific file, but then it doesn't appear in the docs
    GLOBAL_DEF("android/modules", "");

    using_datapack = false;
}

ProjectSettings::~ProjectSettings() {

    singleton = nullptr;
}