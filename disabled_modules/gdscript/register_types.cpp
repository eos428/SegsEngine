/*************************************************************************/
/*  register_types.cpp                                                   */
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

#include "register_types.h"

#include "core/class_db.h"
#include "core/io/file_access_encrypted.h"
#include "core/io/resource_loader.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "gdscript.h"
#include "gdscript_tokenizer.h"
#include <QResource>

#include "core/resource/resource_manager.h"

GDScriptLanguage *script_language_gd = nullptr;
Ref<ResourceFormatLoaderGDScript> resource_loader_gd;
Ref<ResourceFormatSaverGDScript> resource_saver_gd;

#ifdef TOOLS_ENABLED

#include "editor/editor_export.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/gdscript_highlighter.h"

#ifndef GDSCRIPT_NO_LSP
#include "core/engine.h"
#include "language_server/gdscript_language_server.h"
#endif // !GDSCRIPT_NO_LSP

class EditorExportGDScript : public EditorExportPlugin {

    GDCLASS(EditorExportGDScript,EditorExportPlugin)

public:
    void _export_file(StringView p_path, StringView p_type, const Set<String> &p_features) override {

        int script_mode = EditorExportPreset::MODE_SCRIPT_COMPILED;
        String script_key;

        const Ref<EditorExportPreset> &preset = get_export_preset();

        if (preset) {
            script_mode = preset->get_script_export_mode();
            script_key = StringUtils::to_lower(preset->get_script_encryption_key());
        }

        if (!StringUtils::ends_with(p_path,".gd") || script_mode == EditorExportPreset::MODE_SCRIPT_TEXT)
            return;

        Vector<uint8_t> file_contents = FileAccess::get_file_as_array(p_path);
        if (file_contents.empty())
            return;

        Vector<uint8_t> file = GDScriptTokenizerBuffer::parse_code_string({(const char *)file_contents.data(), file_contents.size()});
        String base_path(PathUtils::get_basename(p_path));
        if (!file.empty()) {

            if (script_mode == EditorExportPreset::MODE_SCRIPT_ENCRYPTED) {

                String tmp_path = PathUtils::plus_file(EditorSettings::get_singleton()->get_cache_dir(),"script.gde");
                FileAccess *fa = FileAccess::open(tmp_path, FileAccess::WRITE);

                FixedVector<uint8_t,32,true> key;
                key.resize(32);
                for (uint32_t i = 0; i < 32; i++) {
                    int v = 0;
                    if (i * 2 < script_key.length()) {
                        CharType ct = script_key[i * 2];
                        if (ct >= '0' && ct <= '9')
                            ct = ct.digitValue();
                        else if (ct >= 'a' && ct <= 'f')
                            ct = 10 + ct.toLatin1() - 'a';
                        v |= ct.toLatin1() << 4;
                    }

                    if (i * 2 + 1 < script_key.length()) {
                        CharType ct = script_key[i * 2 + 1];
                        if (ct >= '0' && ct <= '9')
                            ct = ct.toLatin1() - '0';
                        else if (ct >= 'a' && ct <= 'f')
                            ct = 10 + ct.toLatin1() - 'a';
                        v |= ct.toLatin1();
                    }
                    key[i] = v;
                }
                FileAccessEncrypted *fae = memnew(FileAccessEncrypted);
                Error err = fae->open_and_parse(fa, key, FileAccessEncrypted::MODE_WRITE_AES256);

                if (err == OK) {
                    fae->store_buffer(file.data(), file.size());
                }

                memdelete(fae);

                file = FileAccess::get_file_as_array(tmp_path);
                add_file(base_path + ".gde", file, true);

                // Clean up temporary file.
                DirAccess::remove_file_or_error(tmp_path);

            } else {

                add_file(base_path + ".gdc", file, true);
            }
        }
    }
};

IMPL_GDCLASS(EditorExportGDScript)

static void _editor_init() {

    Ref<EditorExportGDScript> gd_export(make_ref_counted<EditorExportGDScript>());
    EditorExport::get_singleton()->add_export_plugin(gd_export);
#ifndef GDSCRIPT_NO_LSP
    register_lsp_types();
    GDScriptLanguageServer *lsp_plugin = memnew(GDScriptLanguageServer);
    EditorNode::get_singleton()->add_editor_plugin(lsp_plugin);
    Engine::get_singleton()->add_singleton(Engine::Singleton("GDScriptLanguageProtocol", GDScriptLanguageProtocol::get_singleton()));
#endif // !GDSCRIPT_NO_LSP
}

#endif // TOOLS_ENABLED

void register_gdscript_types() {

    GDScriptNativeClass::initialize_class();
#ifdef TOOLS_ENABLED
    Q_INIT_RESOURCE(gdscript);
    EditorExportGDScript::initialize_class();
#endif
    ClassDB::register_class<GDScript>();
    ClassDB::register_virtual_class<GDScriptFunctionState>();

    script_language_gd = memnew(GDScriptLanguage);
    ScriptServer::register_language(script_language_gd);

    resource_loader_gd = make_ref_counted<ResourceFormatLoaderGDScript>();
    gResourceManager().add_resource_format_loader(resource_loader_gd);

    resource_saver_gd = make_ref_counted<ResourceFormatSaverGDScript>();
    gResourceManager().add_resource_format_saver(resource_saver_gd);

#ifdef TOOLS_ENABLED
    ScriptEditor::register_create_syntax_highlighter_function(GDScriptSyntaxHighlighter::create);
    EditorNode::add_init_callback(_editor_init);
#endif // TOOLS_ENABLED
}

void unregister_gdscript_types() {

    ScriptServer::unregister_language(script_language_gd);

    if (script_language_gd)
        memdelete(script_language_gd);

    gResourceManager().remove_resource_format_loader(resource_loader_gd);
    resource_loader_gd.unref();

    gResourceManager().remove_resource_format_saver(resource_saver_gd);
    resource_saver_gd.unref();
}
