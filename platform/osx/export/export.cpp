/*************************************************************************/
/*  export.cpp                                                           */
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

#include "export.h"

#include "core/io/marshalls.h"
#include "core/io/resource_saver.h"
#include "core/io/zip_io.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "core/version.h"
#include "editor/editor_export.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "platform/osx/logo.gen.h"
#include <cstring>
#include <sys/stat.h>

class EditorExportPlatformOSX final : public EditorExportPlatform {

    GDCLASS(EditorExportPlatformOSX,EditorExportPlatform)


    int version_code;

    Ref<ImageTexture> logo;

    void _fix_plist(const Ref<EditorExportPreset> &p_preset, Vector<uint8_t> &plist, const String &p_binary);
    void _make_icon(const Ref<Image> &p_icon, Vector<uint8_t> &p_data);

    Error _code_sign(const Ref<EditorExportPreset> &p_preset, const String &p_path);
    Error _create_dmg(const String &p_dmg_path, const String &p_pkg_name, const String &p_app_path_name);

#ifdef OSX_ENABLED
    bool use_codesign() const { return true; }
    bool use_dmg() const { return true; }
#else
    bool use_codesign() const { return false; }
    bool use_dmg() const { return false; }
#endif

protected:
    void get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) override;
    void get_export_options(List<ExportOption> *r_options) override;

public:
    String get_name() const override { return "Mac OSX"; }
    String get_os_name() const override { return "OSX"; }
    Ref<Texture> get_logo() const override { return logo; }

    List<String> get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const  override {
        List<String> list;
        if (use_dmg()) {
            list.push_back("dmg");
        }
        list.push_back("zip");
        return list;
    }
    Error export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, int p_flags = 0) override;

    bool can_export(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates) const override;

    void get_platform_features(List<String> *r_features) override {

        r_features->push_back("pc");
        r_features->push_back("s3tc");
        r_features->push_back("OSX");
    }

    void resolve_platform_feature_priorities(const Ref<EditorExportPreset> &p_preset, Set<String> &p_features) override {
    }

    EditorExportPlatformOSX();
    ~EditorExportPlatformOSX() override;
};

IMPL_GDCLASS(EditorExportPlatformOSX)

void EditorExportPlatformOSX::get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) {
    if (p_preset->get("texture_format/s3tc")) {
        r_features->push_back("s3tc");
    }
    if (p_preset->get("texture_format/etc")) {
        r_features->push_back("etc");
    }
    if (p_preset->get("texture_format/etc2")) {
        r_features->push_back("etc2");
    }

    r_features->push_back("64");
}

void EditorExportPlatformOSX::get_export_options(List<ExportOption> *r_options) {

    r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "custom_package/debug", PROPERTY_HINT_GLOBAL_FILE, "*.zip"), ""));
    r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "custom_package/release", PROPERTY_HINT_GLOBAL_FILE, "*.zip"), ""));

    r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/name", PROPERTY_HINT_PLACEHOLDER_TEXT, "Game Name"), ""));
    r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/info"), "Made with Godot Engine"));
    r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/icon", PROPERTY_HINT_FILE, "*.png,*.icns"), ""));
    r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/identifier", PROPERTY_HINT_PLACEHOLDER_TEXT, "com.example.game"), ""));
    r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/signature"), ""));
    r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/short_version"), "1.0"));
    r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/version"), "1.0"));
    r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/copyright"), ""));
    r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "display/high_res"), false));

#ifdef OSX_ENABLED
    r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "codesign/identity"), ""));
    r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "codesign/entitlements"), ""));
#endif

    r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "texture_format/s3tc"), true));
    r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "texture_format/etc"), false));
    r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "texture_format/etc2"), false));
}

void _rgba8_to_packbits_encode(int p_ch, int p_size, PoolVector<uint8_t> &p_source, Vector<uint8_t> &p_dest) {

    int src_len = p_size * p_size;

    Vector<uint8_t> result;
    result.resize(src_len * 1.25); //temp vector for rle encoded data, make it 25% larger for worst case scenario
    int res_size = 0;

    uint8_t buf[128];
    int buf_size = 0;

    int i = 0;
    while (i < src_len) {
        uint8_t cur = p_source.read()[i * 4 + p_ch];

        if (i < src_len - 2) {

            if ((p_source.read()[(i + 1) * 4 + p_ch] == cur) && (p_source.read()[(i + 2) * 4 + p_ch] == cur)) {
                if (buf_size > 0) {
                    result.write[res_size++] = (uint8_t)(buf_size - 1);
                    memcpy(&result.write[res_size], &buf, buf_size);
                    res_size += buf_size;
                    buf_size = 0;
                }

                uint8_t lim = i + 130 >= src_len ? src_len - i - 1 : 130;
                bool hit_lim = true;

                for (int j = 3; j <= lim; j++) {
                    if (p_source.read()[(i + j) * 4 + p_ch] != cur) {
                        hit_lim = false;
                        i = i + j - 1;
                        result.write[res_size++] = (uint8_t)(j - 3 + 0x80);
                        result.write[res_size++] = cur;
                        break;
                    }
                }
                if (hit_lim) {
                    result.write[res_size++] = (uint8_t)(lim - 3 + 0x80);
                    result.write[res_size++] = cur;
                    i = i + lim;
                }
            } else {
                buf[buf_size++] = cur;
                if (buf_size == 128) {
                    result.write[res_size++] = (uint8_t)(buf_size - 1);
                    memcpy(&result.write[res_size], &buf, buf_size);
                    res_size += buf_size;
                    buf_size = 0;
                }
            }
        } else {
            buf[buf_size++] = cur;
            result.write[res_size++] = (uint8_t)(buf_size - 1);
            memcpy(&result.write[res_size], &buf, buf_size);
            res_size += buf_size;
            buf_size = 0;
        }

        i++;
    }

    int ofs = p_dest.size();
    p_dest.resize(p_dest.size() + res_size);
    memcpy(&p_dest.write[ofs], result.ptr(), res_size);
}

void EditorExportPlatformOSX::_make_icon(const Ref<Image> &p_icon, Vector<uint8_t> &p_data) {

    Ref<ImageTexture> it = memnew(ImageTexture);

    Vector<uint8_t> data;

    data.resize(8);
    data.write[0] = 'i';
    data.write[1] = 'c';
    data.write[2] = 'n';
    data.write[3] = 's';

    struct MacOSIconInfo {
        const char *name;
        const char *mask_name;
        bool is_png;
        int size;
    };

    static const MacOSIconInfo icon_infos[] = {
        { "ic10", "", true, 1024 }, //1024x1024 32-bit PNG and 512x512@2x 32-bit "retina" PNG
        { "ic09", "", true, 512 }, //512×512 32-bit PNG
        { "ic14", "", true, 512 }, //256x256@2x 32-bit "retina" PNG
        { "ic08", "", true, 256 }, //256×256 32-bit PNG
        { "ic13", "", true, 256 }, //128x128@2x 32-bit "retina" PNG
        { "ic07", "", true, 128 }, //128x128 32-bit PNG
        { "ic12", "", true, 64 }, //32x32@2x 32-bit "retina" PNG
        { "ic11", "", true, 32 }, //16x16@2x 32-bit "retina" PNG
        { "il32", "l8mk", false, 32 }, //32x32 24-bit RLE + 8-bit uncompressed mask
        { "is32", "s8mk", false, 16 } //16x16 24-bit RLE + 8-bit uncompressed mask
    };

    for (unsigned int i = 0; i < (sizeof(icon_infos) / sizeof(icon_infos[0])); ++i) {
        Ref<Image> copy = p_icon; // does this make sense? doesn't this just increase the reference count instead of making a copy? Do we even need a copy?
        copy->convert(Image::FORMAT_RGBA8);
        copy->resize(icon_infos[i].size, icon_infos[i].size);

        if (icon_infos[i].is_png) {
            // Encode PNG icon.
            it->create_from_image(copy);
			String path = PathUtils::plus_file(EditorSettings::get_singleton()->get_cache_dir(),"icon.png");
            ResourceSaver::save(path, it);

            FileAccess *f = FileAccess::open(path, FileAccess::READ);
            if (!f) {
                // Clean up generated file.
                DirAccess::remove_file_or_error(path);
                ERR_FAIL();
            }

            int ofs = data.size();
            uint32_t len = f->get_len();
            data.resize(data.size() + len + 8);
            f->get_buffer(&data.write[ofs + 8], len);
            memdelete(f);
            len += 8;
            len = BSWAP32(len);
            memcpy(&data.write[ofs], icon_infos[i].name, 4);
            encode_uint32(len, &data.write[ofs + 4]);

            // Clean up generated file.
            DirAccess::remove_file_or_error(path);

        } else {
            PoolVector<uint8_t> src_data = copy->get_data();

            //encode 24bit RGB RLE icon
            {
                int ofs = data.size();
                data.resize(data.size() + 8);

                _rgba8_to_packbits_encode(0, icon_infos[i].size, src_data, data); // encode R
                _rgba8_to_packbits_encode(1, icon_infos[i].size, src_data, data); // encode G
                _rgba8_to_packbits_encode(2, icon_infos[i].size, src_data, data); // encode B

                int len = data.size() - ofs;
                len = BSWAP32(len);
                memcpy(&data.write[ofs], icon_infos[i].name, 4);
                encode_uint32(len, &data.write[ofs + 4]);
            }

            //encode 8bit mask uncompressed icon
            {
                int ofs = data.size();
                int len = copy->get_width() * copy->get_height();
                data.resize(data.size() + len + 8);

                for (int j = 0; j < len; j++) {
                    data.write[ofs + 8 + j] = src_data.read()[j * 4 + 3];
                }
                len += 8;
                len = BSWAP32(len);
                memcpy(&data.write[ofs], icon_infos[i].mask_name, 4);
                encode_uint32(len, &data.write[ofs + 4]);
            }
        }
    }

    uint32_t total_len = data.size();
    total_len = BSWAP32(total_len);
    encode_uint32(total_len, &data.write[4]);

    p_data = data;
}

void EditorExportPlatformOSX::_fix_plist(const Ref<EditorExportPreset> &p_preset, Vector<uint8_t> &plist, const String &p_binary) {

    String strnew;
	String str = StringUtils::from_utf8((const char *)plist.ptr(), plist.size());
    Vector<String> lines = StringUtils::split(str,"\n");
	const std::pair<const char*,String> replacements[] = {
		{"$binary", p_binary},
		{"$name", p_binary},
		{"$info", p_preset->get("application/info")},
		{"$identifier", p_preset->get("application/identifier")},
		{"$short_version", p_preset->get("application/short_version")},
		{"$version", p_preset->get("application/version")},
		{"$signature", p_preset->get("application/signature")},
		{"$copyright", p_preset->get("application/copyright")},
		{"$highres", p_preset->get("display/high_res") ? "<true/>" : "<false/>"},
    };
    for (int i = 0; i < lines.size(); i++) {
		String line(lines[i]);
        for(const std::pair<const char *,String > &en : replacements)
        {
		   line = StringUtils::replace(line,en.first, en.second);
        }
        strnew += line + "\n";
    }

	CharString cs = StringUtils::to_utf8(strnew);
    plist.resize(cs.size() - 1);
    for (int i = 0; i < cs.size() - 1; i++) {
        plist.write[i] = cs[i];
    }
}

/**
    If we're running the OSX version of the Godot editor we'll:
    - export our application bundle to a temporary folder
    - attempt to code sign it
    - and then wrap it up in a DMG
**/

Error EditorExportPlatformOSX::_code_sign(const Ref<EditorExportPreset> &p_preset, const String &p_path) {
    List<String> args;

    if (p_preset->get("codesign/entitlements") != "") {
        /* this should point to our entitlements.plist file that sandboxes our application, I don't know if this should also be placed in our app bundle */
        args.push_back("-entitlements");
        args.push_back(p_preset->get("codesign/entitlements"));
    }
    args.push_back("-s");
    args.push_back(p_preset->get("codesign/identity"));
    args.push_back("-v"); /* provide some more feedback */
    args.push_back(p_path);

    String str;
    Error err = OS::get_singleton()->execute("codesign", args, true, nullptr, &str, nullptr, true);
    ERR_FAIL_COND_V(err != OK, err);

    print_line("codesign: " + str);
	if (StringUtils::contains(str,"no identity found")) {
        EditorNode::add_io_error("codesign: no identity found");
        return FAILED;
    }

    return OK;
}

Error EditorExportPlatformOSX::_create_dmg(const String &p_dmg_path, const String &p_pkg_name, const String &p_app_path_name) {
    List<String> args;

    OS::get_singleton()->move_to_trash(p_dmg_path);

    args.push_back("create");
    args.push_back(p_dmg_path);
    args.push_back("-volname");
    args.push_back(p_pkg_name);
    args.push_back("-fs");
    args.push_back("HFS+");
    args.push_back("-srcfolder");
    args.push_back(p_app_path_name);

    String str;
    Error err = OS::get_singleton()->execute("hdiutil", args, true, nullptr, &str, nullptr, true);
	ERR_FAIL_COND_V(err != OK, err)

    print_line("hdiutil returned: " + str);
	if (StringUtils::contains(str,"create failed")) {
		if (StringUtils::contains(str,"File exists")) {
            EditorNode::add_io_error("hdiutil: create failed - file exists");
        } else {
            EditorNode::add_io_error("hdiutil: create failed");
        }
        return FAILED;
    }

    return OK;
}

Error EditorExportPlatformOSX::export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, int p_flags) {
    ExportNotifier notifier(*this, p_preset, p_debug, p_path, p_flags);

    String src_pkg_name;

    EditorProgress ep("export", "Exporting for OSX", 3, true);

    if (p_debug)
        src_pkg_name = p_preset->get("custom_package/debug");
    else
        src_pkg_name = p_preset->get("custom_package/release");

    if (src_pkg_name.empty()) {
        String err;
        src_pkg_name = find_export_template("osx.zip", &err);
        if (src_pkg_name.empty()) {
            EditorNode::add_io_error(err);
            return ERR_FILE_NOT_FOUND;
        }
    }

	if (!DirAccess::exists(PathUtils::get_base_dir(p_path))) {
        return ERR_FILE_BAD_PATH;
    }

    FileAccess *src_f = nullptr;
    zlib_filefunc_def io = zipio_create_io_from_file(&src_f);

    if (ep.step("Creating app", 0)) {
        return ERR_SKIP;
    }

	unzFile src_pkg_zip = unzOpen2(qPrintable(src_pkg_name.m_str), &io);
    if (!src_pkg_zip) {

        EditorNode::add_io_error("Could not find template app to export:\n" + src_pkg_name);
        return ERR_FILE_NOT_FOUND;
    }

    int ret = unzGoToFirstFile(src_pkg_zip);

    String binary_to_use = "godot_osx_" + String(p_debug ? "debug" : "release") + ".64";

    String pkg_name;
    if (p_preset->get("application/name") != "")
        pkg_name = p_preset->get("application/name"); // app_name
    else if (!String(ProjectSettings::get_singleton()->get("application/config/name")).empty())
        pkg_name = String(ProjectSettings::get_singleton()->get("application/config/name"));
    else
        pkg_name = "Unnamed";

    Error err = OK;
    String tmp_app_path_name = "";
    zlib_filefunc_def io2 = io;
    FileAccess *dst_f = nullptr;
    io2.opaque = &dst_f;
    zipFile dst_pkg_zip = nullptr;

	String export_format = use_dmg() && StringUtils::ends_with(p_path,"dmg") ? "dmg" : "zip";
    if (export_format == "dmg") {
        // We're on OSX so we can export to DMG, but first we create our application bundle
		tmp_app_path_name = PathUtils::plus_file(EditorSettings::get_singleton()->get_cache_dir(),pkg_name + ".app");
        print_line("Exporting to " + tmp_app_path_name);
        DirAccess *tmp_app_path = DirAccess::create_for_path(tmp_app_path_name);
        if (!tmp_app_path) {
            err = ERR_CANT_CREATE;
        }

        // Create our folder structure or rely on unzip?
        if (err == OK) {
            print_line("Creating " + tmp_app_path_name + "/Contents/MacOS");
            err = tmp_app_path->make_dir_recursive(tmp_app_path_name + "/Contents/MacOS");
        }

        if (err == OK) {
            print_line("Creating " + tmp_app_path_name + "/Contents/Frameworks");
            err = tmp_app_path->make_dir_recursive(tmp_app_path_name + "/Contents/Frameworks");
        }

        if (err == OK) {
            print_line("Creating " + tmp_app_path_name + "/Contents/Resources");
            err = tmp_app_path->make_dir_recursive(tmp_app_path_name + "/Contents/Resources");
        }
    } else {
        // Open our destination zip file
		dst_pkg_zip = zipOpen2(qPrintable(p_path.m_str), APPEND_STATUS_CREATE, nullptr, &io2);
        if (!dst_pkg_zip) {
            err = ERR_CANT_CREATE;
        }
    }

    // Now process our template
    bool found_binary = false;
    int total_size = 0;

    while (ret == UNZ_OK && err == OK) {
        bool is_execute = false;

        //get filename
        unz_file_info info;
        char fname[16384];
        ret = unzGetCurrentFileInfo(src_pkg_zip, &info, fname, 16384, nullptr, 0, nullptr, 0);

        String file = fname;

        Vector<uint8_t> data;
        data.resize(info.uncompressed_size);

        //read
        unzOpenCurrentFile(src_pkg_zip);
        unzReadCurrentFile(src_pkg_zip, data.ptrw(), data.size());
        unzCloseCurrentFile(src_pkg_zip);

        //write

		file =StringUtils::replace_first(file,"osx_template.app/", "");

        if (file == "Contents/Info.plist") {
            _fix_plist(p_preset, data, pkg_name);
        }

		if (StringUtils::begins_with(file,"Contents/MacOS/godot_")) {
            if (file != "Contents/MacOS/" + binary_to_use) {
                ret = unzGoToNextFile(src_pkg_zip);
                continue; //ignore!
            }
            found_binary = true;
            is_execute = true;
            file = "Contents/MacOS/" + pkg_name;
        }

        if (file == "Contents/Resources/icon.icns") {
            //see if there is an icon
            String iconpath;
            if (p_preset->get("application/icon") != "")
                iconpath = p_preset->get("application/icon");
            else
                iconpath = ProjectSettings::get_singleton()->get("application/config/icon");

            if (!iconpath.empty()) {
				if (PathUtils::get_extension(iconpath) == "icns") {
                    FileAccess *icon = FileAccess::open(iconpath, FileAccess::READ);
                    if (icon) {
                        data.resize(icon->get_len());
                        icon->get_buffer(&data.write[0], icon->get_len());
                        icon->close();
                        memdelete(icon);
                    }
                } else {
                    Ref<Image> icon;
                    icon.instance();
                    icon->load(iconpath);
                    if (!icon->empty()) {
                        _make_icon(icon, data);
                    }
                }
            }
        }

        if (!data.empty()) {
            print_line("ADDING: " + file + " size: " + itos(data.size()));
            total_size += data.size();

            if (export_format == "dmg") {
                // write it into our application bundle
				file = PathUtils::plus_file(tmp_app_path_name,file);

                // write the file, need to add chmod
                FileAccess *f = FileAccess::open(file, FileAccess::WRITE);
                if (f) {
                    f->store_buffer(data.ptr(), data.size());
                    f->close();
                    if (is_execute) {
                        // Chmod with 0755 if the file is executable
                        FileAccess::set_unix_permissions(file, 0755);
                    }
                    memdelete(f);
                } else {
                    err = ERR_CANT_CREATE;
                }
            } else {
                // add it to our zip file
                file = pkg_name + ".app/" + file;

                zip_fileinfo fi;
                fi.tmz_date.tm_hour = info.tmu_date.tm_hour;
                fi.tmz_date.tm_min = info.tmu_date.tm_min;
                fi.tmz_date.tm_sec = info.tmu_date.tm_sec;
                fi.tmz_date.tm_mon = info.tmu_date.tm_mon;
                fi.tmz_date.tm_mday = info.tmu_date.tm_mday;
                fi.tmz_date.tm_year = info.tmu_date.tm_year;
                fi.dosDate = info.dosDate;
                fi.internal_fa = info.internal_fa;
                fi.external_fa = info.external_fa;

                zipOpenNewFileInZip(dst_pkg_zip,
						qPrintable(file.m_str),
                        &fi,
                        nullptr,
                        0,
                        nullptr,
                        0,
                        nullptr,
                        Z_DEFLATED,
                        Z_DEFAULT_COMPRESSION);

                zipWriteInFileInZip(dst_pkg_zip, data.ptr(), data.size());
                zipCloseFileInZip(dst_pkg_zip);
            }
        }

        ret = unzGoToNextFile(src_pkg_zip);
    }

    // we're done with our source zip
    unzClose(src_pkg_zip);

    if (!found_binary) {
        ERR_PRINTS("Requested template binary '" + binary_to_use + "' not found. It might be missing from your template archive.");
        err = ERR_FILE_NOT_FOUND;
    }

    if (err == OK) {
        if (ep.step("Making PKG", 1)) {
            return ERR_SKIP;
        }

        if (export_format == "dmg") {
            String pack_path = tmp_app_path_name + "/Contents/Resources/" + pkg_name + ".pck";
            Vector<SharedObject> shared_objects;
            err = save_pack(p_preset, pack_path, &shared_objects);

            // see if we can code sign our new package
            String identity = p_preset->get("codesign/identity");

            if (err == OK) {
                DirAccess *da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
                for (int i = 0; i < shared_objects.size(); i++) {
					err = da->copy(shared_objects[i].path, tmp_app_path_name + "/Contents/Frameworks/" + PathUtils::get_file(shared_objects[i].path));
                    if (err == OK && !identity.empty()) {
						err = _code_sign(p_preset, tmp_app_path_name + "/Contents/Frameworks/" + PathUtils::get_file(shared_objects[i].path));
                    }
                }
                memdelete(da);
            }

            if (err == OK && !identity.empty()) {
                if (ep.step("Code signing bundle", 2)) {
                    return ERR_SKIP;
                }

                // the order in which we code sign is important, this is a bit of a shame or we could do this in our loop that extracts the files from our ZIP

                // start with our application
                err = _code_sign(p_preset, tmp_app_path_name + "/Contents/MacOS/" + pkg_name);

                ///@TODO we should check the contents of /Contents/Frameworks for frameworks to sign
            }

            if (err == OK && !identity.empty()) {
                // we should probably loop through all resources and sign them?
                err = _code_sign(p_preset, tmp_app_path_name + "/Contents/Resources/icon.icns");
            }

            if (err == OK && !identity.empty()) {
                err = _code_sign(p_preset, pack_path);
            }

            if (err == OK && !identity.empty()) {
                err = _code_sign(p_preset, tmp_app_path_name + "/Contents/Info.plist");
            }

            // and finally create a DMG
            if (err == OK) {
                if (ep.step("Making DMG", 3)) {
                    return ERR_SKIP;
                }
                err = _create_dmg(p_path, pkg_name, tmp_app_path_name);
            }

            // Clean up temporary .app dir
            OS::get_singleton()->move_to_trash(tmp_app_path_name);

        } else { // pck

			String pack_path = PathUtils::plus_file(EditorSettings::get_singleton()->get_cache_dir(),pkg_name + ".pck");

            Vector<SharedObject> shared_objects;
            err = save_pack(p_preset, pack_path, &shared_objects);

            if (err == OK) {
                zipOpenNewFileInZip(dst_pkg_zip,
						qPrintable((pkg_name + ".app/Contents/Resources/" + pkg_name + ".pck").m_str),
                        nullptr,
                        nullptr,
                        0,
                        nullptr,
                        0,
                        nullptr,
                        Z_DEFLATED,
                        Z_DEFAULT_COMPRESSION);

                FileAccess *pf = FileAccess::open(pack_path, FileAccess::READ);
                if (pf) {
                    const int BSIZE = 16384;
                    uint8_t buf[BSIZE];

                    while (true) {

                        int r = pf->get_buffer(buf, BSIZE);
                        if (r <= 0)
                            break;
                        zipWriteInFileInZip(dst_pkg_zip, buf, r);
                    }

                    zipCloseFileInZip(dst_pkg_zip);
                    memdelete(pf);
                } else {
                    err = ERR_CANT_OPEN;
                }
            }

            if (err == OK) {
                //add shared objects
                for (int i = 0; i < shared_objects.size(); i++) {
                    Vector<uint8_t> file = FileAccess::get_file_as_array(shared_objects[i].path);
                    ERR_CONTINUE(file.empty());

                    zipOpenNewFileInZip(dst_pkg_zip,
							qPrintable((PathUtils::plus_file(String(pkg_name + ".app/Contents/Frameworks/"),PathUtils::get_file(shared_objects[i].path))).m_str),
                            nullptr,
                            nullptr,
                            0,
                            nullptr,
                            0,
                            nullptr,
                            Z_DEFLATED,
                            Z_DEFAULT_COMPRESSION);

                    zipWriteInFileInZip(dst_pkg_zip, file.ptr(), file.size());
                    zipCloseFileInZip(dst_pkg_zip);
                }
            }

            // Clean up generated file.
            DirAccess::remove_file_or_error(pack_path);
        }
    }

    if (dst_pkg_zip) {
        zipClose(dst_pkg_zip, nullptr);
    }

    return err;
}

bool EditorExportPlatformOSX::can_export(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates) const {

    bool valid = false;
    String err;

    if (exists_export_template("osx.zip", &err)) {
        valid = true;
    }

    if (p_preset->get("custom_package/debug") != "") {
        if (FileAccess::exists(p_preset->get("custom_package/debug"))) {
            valid = true;
        } else {
            err += TTR("Custom debug template not found.") + "\n";
        }
    }

    if (p_preset->get("custom_package/release") != "") {
        if (FileAccess::exists(p_preset->get("custom_package/release"))) {
            valid = true;
        } else {
            err += TTR("Custom release template not found.") + "\n";
        }
    }

    if (!err.empty())
        r_error = err;

    r_missing_templates = !valid;
    return valid;
}

EditorExportPlatformOSX::EditorExportPlatformOSX() {

    Ref<Image> img = memnew(Image(_osx_logo));
    logo.instance();
    logo->create_from_image(img);
}

EditorExportPlatformOSX::~EditorExportPlatformOSX() {
}

void register_osx_exporter() {

    Ref<EditorExportPlatformOSX> platform;
    platform.instance();

    EditorExport::get_singleton()->add_export_platform(platform);
}