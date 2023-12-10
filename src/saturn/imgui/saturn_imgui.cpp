#include "saturn_imgui.h"

#include <string>
#include <iostream>
#include <algorithm>
#include <map>
#include <fstream>

#include "saturn/imgui/saturn_imgui_dynos.h"
#include "saturn/imgui/saturn_imgui_cc_editor.h"
#include "saturn/imgui/saturn_imgui_machinima.h"
#include "saturn/imgui/saturn_imgui_settings.h"
#include "saturn/imgui/saturn_imgui_chroma.h"
#include "saturn/libs/imgui/imgui.h"
#include "saturn/libs/imgui/imgui_internal.h"
#include "saturn/libs/imgui/imgui_impl_sdl.h"
#include "saturn/libs/imgui/imgui_impl_opengl3.h"
#include "saturn/libs/imgui/imgui_neo_sequencer.h"
#include "saturn/saturn.h"
#include "saturn/saturn_colors.h"
#include "saturn/saturn_textures.h"
#include "saturn/discord/saturn_discord.h"
#include "pc/controller/controller_keyboard.h"
#include "data/dynos.cpp.h"
#include "icons/IconsForkAwesome.h"
#include "icons/IconsFontAwesome5.h"
#include "saturn/filesystem/saturn_projectfile.h"
#include "saturn/saturn_timeline_groups.h"
#include "saturn/cmd/saturn_cmd.h"
#include "saturn/saturn_json.h"

#include <SDL2/SDL.h>

#ifdef __MINGW32__
# define FOR_WINDOWS 1
#else
# define FOR_WINDOWS 0
#endif

#if FOR_WINDOWS || defined(OSX_BUILD)
# define GLEW_STATIC
# include <GL/glew.h>
#endif

#define GL_GLEXT_PROTOTYPES 1
#ifdef USE_GLES
# include <SDL2/SDL_opengles2.h>
# define RAPI_NAME "OpenGL ES"
#else
# include <SDL2/SDL_opengl.h>
# define RAPI_NAME "OpenGL"
#endif

#if FOR_WINDOWS
#define PLATFORM "Windows"
#define PLATFORM_ICON ICON_FK_WINDOWS
#elif defined(OSX_BUILD)
#define PLATFORM "Mac OS"
#define PLATFORM_ICON ICON_FK_APPLE
#else
#define PLATFORM "Linux"
#define PLATFORM_ICON ICON_FK_LINUX
#endif

extern "C" {
#include "pc/gfx/gfx_pc.h"
#include "pc/configfile.h"
#include "game/mario.h"
#include "game/game_init.h"
#include "game/camera.h"
#include "game/level_update.h"
#include "engine/level_script.h"
#include "game/object_list_processor.h"
}

#define CLI_MAX_INPUT_SIZE 4096

using namespace std;

SDL_Window* window = nullptr;
ImGuiIO io;

Array<PackData *> &iDynosPacks = DynOS_Gfx_GetPacks();

// Variables

int currentMenu = 0;
bool showMenu = true;
bool showStatusBars = true;

bool windowStats = true;
bool windowCcEditor;
bool windowAnimPlayer;
bool windowSettings;
bool windowChromaKey;

bool chromaRequireReload;

int windowStartHeight;

bool has_copy_camera;
bool copy_relative = true;

bool paste_forever;

int copiedKeyframeIndex = -1;

bool k_context_popout_open = false;
Keyframe k_context_popout_keyframe = Keyframe(0, InterpolationCurve::LINEAR);
ImVec2 k_context_popout_pos = ImVec2(0, 0);

bool was_camera_frozen = false;

bool splash_finished = false;

std::string editor_theme = "moon";
std::vector<std::pair<std::string, std::string>> theme_list = {};

std::vector<std::string> macro_array = {};
std::vector<std::string> macro_dir_stack = {};
std::string current_macro_dir = "";
int current_macro_id = -1;
int current_macro_dir_count = 0;
char cli_input[CLI_MAX_INPUT_SIZE];

void saturn_load_macros() {
    std::string dir = "dynos/macros/" + current_macro_dir;
    macro_array.clear();
    current_macro_id = -1;
    current_macro_dir_count = 1;
    if (current_macro_dir != "") macro_array.push_back("../");
    else current_macro_dir_count = 0;
    for (auto& entry : filesystem::directory_iterator(dir)) {
        if (!filesystem::is_directory(entry)) continue;
        macro_array.push_back(entry.path().filename().u8string() + "/");
        current_macro_dir_count++;
    }
    for (auto& entry : filesystem::directory_iterator(dir)) {
        if (filesystem::is_directory(entry)) continue;
        if (entry.path().extension().u8string() == ".stm")
            macro_array.push_back(entry.path().filename().u8string());
    }
}

// Bundled Components

void imgui_bundled_tooltip(const char* text) {
    if (!configEditorShowTips) return;

    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(450.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void imgui_bundled_help_marker(const char* desc) {
    if (!configEditorShowTips) {
        ImGui::TextDisabled("");
        return;
    }

    ImGui::TextDisabled(ICON_FA_QUESTION_CIRCLE);
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void imgui_bundled_space(float size, const char* title = NULL, const char* help_marker = NULL) {
    ImGui::Dummy(ImVec2(0, size/2));
    ImGui::Separator();
    if (title == NULL) {
        ImGui::Dummy(ImVec2(0, size/2));
    } else {
        ImGui::Dummy(ImVec2(0, size/4));
        ImGui::Text(title);
        if (help_marker != NULL) {
            ImGui::SameLine(); imgui_bundled_help_marker(help_marker);
        }
        ImGui::Dummy(ImVec2(0, size/4));
    }
}

void imgui_bundled_window_reset(const char* windowTitle, int width, int height, int x, int y) {
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::Selectable(ICON_FK_UNDO " Reset Window Pos")) {
            ImGui::SetWindowSize(ImGui::FindWindowByName(windowTitle), ImVec2(width, height));
            ImGui::SetWindowPos(ImGui::FindWindowByName(windowTitle), ImVec2(x, y));
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

ImGuiWindowFlags imgui_bundled_window_corner(int corner, int width, int height, float alpha) {
    ImGuiIO& io = ImGui::GetIO();
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;
    if (corner != -1) {
        const float PAD = 10.0f;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 work_pos = viewport->WorkPos;
        ImVec2 work_size = viewport->WorkSize;
        ImVec2 window_pos, window_pos_pivot;
        window_pos.x = (corner & 1) ? (work_pos.x + work_size.x - PAD) : (work_pos.x + PAD);
        window_pos.y = (corner & 2) ? (work_pos.y + work_size.y - PAD) : (work_pos.y + PAD);
        window_pos_pivot.x = (corner & 1) ? 1.0f : 0.0f;
        window_pos_pivot.y = (corner & 2) ? 1.0f : 0.0f;
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        if (width != 0) ImGui::SetNextWindowSize(ImVec2(width, height));
        window_flags |= ImGuiWindowFlags_NoMove;
        ImGui::SetNextWindowBgAlpha(alpha);
    }

    return window_flags;
}

#define JSON_VEC4(json, entry, dest) if (json.isMember(entry)) dest = ImVec4(json[entry][0].asFloat(), json[entry][1].asFloat(), json[entry][2].asFloat(), json[entry][3].asFloat())
#define JSON_BOOL(json, entry, dest) if (json.isMember(entry)) dest = json[entry].asBool()
#define JSON_FLOAT(json, entry, dest) if (json.isMember(entry)) dest = json[entry].asFloat()
#define JSON_VEC2(json, entry, dest) if (json.isMember(entry)) dest = ImVec2(json[entry][0].asFloat(), json[entry][1].asFloat())
#define JSON_DIR(json, entry, dest) if (json.isMember(entry)) dest = to_dir(json[entry].asString())

ImGuiDir to_dir(std::string dir) {
    if (dir == "up") return ImGuiDir_Up;
    if (dir == "left") return ImGuiDir_Left;
    if (dir == "down") return ImGuiDir_Down;
    if (dir == "right") return ImGuiDir_Right;
    return ImGuiDir_None;
}

void imgui_custom_theme(std::string theme_name) {
    std::filesystem::path path = std::filesystem::path("dynos/themes/" + theme_name + ".json");
    if (!std::filesystem::exists(path)) return;
    std::ifstream file = std::ifstream(path);
    Json::Value json;
    json << file;
    ImGuiStyle* style = &ImGui::GetStyle();
    style->ResetStyle();
    ImVec4* colors = style->Colors;
    if (json.isMember("colors")) {
        Json::Value colorsJson = json["colors"];
        JSON_VEC4(colorsJson, "text", colors[ImGuiCol_Text]);
        JSON_VEC4(colorsJson, "text_disabled", colors[ImGuiCol_TextDisabled]);
        JSON_VEC4(colorsJson, "window_bg", colors[ImGuiCol_WindowBg]);
        JSON_VEC4(colorsJson, "child_bg", colors[ImGuiCol_ChildBg]);
        JSON_VEC4(colorsJson, "popup_bg", colors[ImGuiCol_PopupBg]);
        JSON_VEC4(colorsJson, "border", colors[ImGuiCol_Border]);
        JSON_VEC4(colorsJson, "border_shadow", colors[ImGuiCol_BorderShadow]);
        JSON_VEC4(colorsJson, "frame_bg", colors[ImGuiCol_FrameBg]);
        JSON_VEC4(colorsJson, "frame_bg_hovered", colors[ImGuiCol_FrameBgHovered]);
        JSON_VEC4(colorsJson, "frame_bg_active", colors[ImGuiCol_FrameBgActive]);
        JSON_VEC4(colorsJson, "title_bg", colors[ImGuiCol_TitleBg]);
        JSON_VEC4(colorsJson, "title_bg_active", colors[ImGuiCol_TitleBgActive]);
        JSON_VEC4(colorsJson, "title_bg_collapsed", colors[ImGuiCol_TitleBgCollapsed]);
        JSON_VEC4(colorsJson, "menu_bar_bg", colors[ImGuiCol_MenuBarBg]);
        JSON_VEC4(colorsJson, "scrollbar_bg", colors[ImGuiCol_ScrollbarBg]);
        JSON_VEC4(colorsJson, "scrollbar_grab", colors[ImGuiCol_ScrollbarGrab]);
        JSON_VEC4(colorsJson, "scrollbar_grab_hovered", colors[ImGuiCol_ScrollbarGrabHovered]);
        JSON_VEC4(colorsJson, "scrollbar_grab_active", colors[ImGuiCol_ScrollbarGrabActive]);
        JSON_VEC4(colorsJson, "check_mark", colors[ImGuiCol_CheckMark]);
        JSON_VEC4(colorsJson, "slider_grab", colors[ImGuiCol_SliderGrab]);
        JSON_VEC4(colorsJson, "slider_grab_active", colors[ImGuiCol_SliderGrabActive]);
        JSON_VEC4(colorsJson, "button", colors[ImGuiCol_Button]);
        JSON_VEC4(colorsJson, "button_hovered", colors[ImGuiCol_ButtonHovered]);
        JSON_VEC4(colorsJson, "button_active", colors[ImGuiCol_ButtonActive]);
        JSON_VEC4(colorsJson, "header", colors[ImGuiCol_Header]);
        JSON_VEC4(colorsJson, "header_hovered", colors[ImGuiCol_HeaderHovered]);
        JSON_VEC4(colorsJson, "header_active", colors[ImGuiCol_HeaderActive]);
        JSON_VEC4(colorsJson, "separator", colors[ImGuiCol_Separator]);
        JSON_VEC4(colorsJson, "separator_hovered", colors[ImGuiCol_SeparatorHovered]);
        JSON_VEC4(colorsJson, "separator_active", colors[ImGuiCol_SeparatorActive]);
        JSON_VEC4(colorsJson, "resize_grip", colors[ImGuiCol_ResizeGrip]);
        JSON_VEC4(colorsJson, "resize_grip_hovered", colors[ImGuiCol_ResizeGripHovered]);
        JSON_VEC4(colorsJson, "resize_grip_active", colors[ImGuiCol_ResizeGripActive]);
        JSON_VEC4(colorsJson, "tab", colors[ImGuiCol_Tab]);
        JSON_VEC4(colorsJson, "tab_hovered", colors[ImGuiCol_TabHovered]);
        JSON_VEC4(colorsJson, "tab_active", colors[ImGuiCol_TabActive]);
        JSON_VEC4(colorsJson, "tab_unfocused", colors[ImGuiCol_TabUnfocused]);
        JSON_VEC4(colorsJson, "tab_unfocused_active", colors[ImGuiCol_TabUnfocusedActive]);
        JSON_VEC4(colorsJson, "plot_lines", colors[ImGuiCol_PlotLines]);
        JSON_VEC4(colorsJson, "plot_lines_hovered", colors[ImGuiCol_PlotLinesHovered]);
        JSON_VEC4(colorsJson, "plot_histogram", colors[ImGuiCol_PlotHistogram]);
        JSON_VEC4(colorsJson, "plot_histogram_hovered", colors[ImGuiCol_PlotHistogramHovered]);
        JSON_VEC4(colorsJson, "table_header_bg", colors[ImGuiCol_TableHeaderBg]);
        JSON_VEC4(colorsJson, "table_border_strong", colors[ImGuiCol_TableBorderStrong]);
        JSON_VEC4(colorsJson, "table_border_light", colors[ImGuiCol_TableBorderLight]);
        JSON_VEC4(colorsJson, "table_row_bg", colors[ImGuiCol_TableRowBg]);
        JSON_VEC4(colorsJson, "table_row_bg_alt", colors[ImGuiCol_TableRowBgAlt]);
        JSON_VEC4(colorsJson, "text_selected_bg", colors[ImGuiCol_TextSelectedBg]);
        JSON_VEC4(colorsJson, "drag_drop_target", colors[ImGuiCol_DragDropTarget]);
        JSON_VEC4(colorsJson, "nav_highlight", colors[ImGuiCol_NavHighlight]);
        JSON_VEC4(colorsJson, "nav_windowing_highlight", colors[ImGuiCol_NavWindowingHighlight]);
        JSON_VEC4(colorsJson, "nav_windowing_dim_bg", colors[ImGuiCol_NavWindowingDimBg]);
        JSON_VEC4(colorsJson, "modal_window_dim_bg", colors[ImGuiCol_ModalWindowDimBg]);
    }
    if (json.isMember("style")) {
        Json::Value styleJson = json["style"];
        JSON_BOOL(styleJson, "anti_aliased_lines", style->AntiAliasedLines);
        JSON_BOOL(styleJson, "anti_aliased_lines_use_tex", style->AntiAliasedLinesUseTex);
        JSON_BOOL(styleJson, "anti_aliased_fill", style->AntiAliasedFill);
        JSON_FLOAT(styleJson, "alpha", style->Alpha);
        JSON_FLOAT(styleJson, "disabled_alpha", style->DisabledAlpha);
        JSON_FLOAT(styleJson, "window_rounding", style->WindowRounding);
        JSON_FLOAT(styleJson, "window_border_size", style->WindowBorderSize);
        JSON_FLOAT(styleJson, "child_rounding", style->ChildRounding);
        JSON_FLOAT(styleJson, "child_border_size", style->ChildBorderSize);
        JSON_FLOAT(styleJson, "popup_rounding", style->PopupRounding);
        JSON_FLOAT(styleJson, "popup_border_size", style->PopupBorderSize);
        JSON_FLOAT(styleJson, "frame_rounding", style->FrameRounding);
        JSON_FLOAT(styleJson, "frame_border_size", style->FrameBorderSize);
        JSON_FLOAT(styleJson, "indent_spacing", style->IndentSpacing);
        JSON_FLOAT(styleJson, "columns_min_spacing", style->ColumnsMinSpacing);
        JSON_FLOAT(styleJson, "scrollbar_size", style->ScrollbarSize);
        JSON_FLOAT(styleJson, "scrollbar_rounding", style->ScrollbarRounding);
        JSON_FLOAT(styleJson, "grab_min_size", style->GrabMinSize);
        JSON_FLOAT(styleJson, "grab_rounding", style->GrabRounding);
        JSON_FLOAT(styleJson, "log_slider_deadzone", style->LogSliderDeadzone);
        JSON_FLOAT(styleJson, "tab_rounding", style->TabRounding);
        JSON_FLOAT(styleJson, "tab_border_size", style->TabBorderSize);
        JSON_FLOAT(styleJson, "tab_min_width_for_close_button", style->TabMinWidthForCloseButton);
        JSON_FLOAT(styleJson, "mouse_cursor_scale", style->MouseCursorScale);
        JSON_FLOAT(styleJson, "curve_tessellation_tol", style->CurveTessellationTol);
        JSON_FLOAT(styleJson, "circle_tessellation_max_error", style->CircleTessellationMaxError);
        JSON_VEC2(styleJson, "window_padding", style->WindowPadding);
        JSON_VEC2(styleJson, "window_min_size", style->WindowMinSize);
        JSON_VEC2(styleJson, "window_title_align", style->WindowTitleAlign);
        JSON_VEC2(styleJson, "frame_padding", style->FramePadding);
        JSON_VEC2(styleJson, "item_spacing", style->ItemSpacing);
        JSON_VEC2(styleJson, "item_inner_spacing", style->ItemInnerSpacing);
        JSON_VEC2(styleJson, "cell_padding", style->CellPadding);
        JSON_VEC2(styleJson, "touch_extra_padding", style->TouchExtraPadding);
        JSON_VEC2(styleJson, "button_text_align", style->ButtonTextAlign);
        JSON_VEC2(styleJson, "selectable_text_align", style->SelectableTextAlign);
        JSON_VEC2(styleJson, "display_window_padding", style->DisplayWindowPadding);
        JSON_VEC2(styleJson, "display_safe_area_padding", style->DisplaySafeAreaPadding);
        JSON_DIR(styleJson, "color_button_position", style->ColorButtonPosition);
        JSON_DIR(styleJson, "window_menu_button_position", style->WindowMenuButtonPosition);
    }
}

#undef JSON_VEC4
#undef JSON_BOOL
#undef JSON_FLOAT
#undef JSON_VEC2
#undef JSON_DIR

bool initFont = true;

void imgui_update_theme() {
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle* style = &ImGui::GetStyle();

    float SCALE = 1.f;

    if (initFont) {
        initFont = false;

        ImFontConfig defaultConfig;
        defaultConfig.SizePixels = 13.0f * SCALE;
        io.Fonts->AddFontDefault(&defaultConfig);

        ImFontConfig symbolConfig;
        symbolConfig.MergeMode = true;
        symbolConfig.SizePixels = 13.0f * SCALE;
        symbolConfig.GlyphMinAdvanceX = 13.0f * SCALE; // Use if you want to make the icon monospaced
        static const ImWchar icon_ranges[] = { ICON_MIN_FK, ICON_MAX_FK, 0 };
        io.Fonts->AddFontFromFileTTF("fonts/forkawesome-webfont.ttf", symbolConfig.SizePixels, &symbolConfig, icon_ranges);
    }

    // backwards compatibility with older theme settings
    if (configEditorTheme == 0) editor_theme = "legacy";
    else if (configEditorTheme == 1) editor_theme = "moon";
    else if (configEditorTheme == 2) editor_theme = "halflife";
    else if (configEditorTheme == 3) editor_theme = "moviemaker";
    else if (configEditorTheme == 4) editor_theme = "dear";
    else {
        for (const auto& entry : std::filesystem::directory_iterator("dynos/themes")) {
            std::filesystem::path path = entry.path();
            if (path.extension().string() != ".json") continue;
            std::string name = path.filename().string();
            name = name.substr(0, name.length() - 5);
            if (string_hash(name.c_str(), 0, name.length()) == configEditorThemeJson) {
                editor_theme = name;
                break;
            }
        }
    }
    std::cout << "Using theme: " << editor_theme << std::endl;
    configEditorTheme = -1;
    imgui_custom_theme(editor_theme);

    style->ScaleAllSizes(SCALE);
}

// Set up ImGui

void saturn_imgui_init_backend(SDL_Window * sdl_window, SDL_GLContext ctx) {
    window = sdl_window;

    const char* glsl_version = "#version 120";
    ImGuiContext* imgui = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui);
    io = ImGui::GetIO(); (void)io;
    io.WantSetMousePos = false;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    imgui_update_theme();

    ImGui_ImplSDL2_InitForOpenGL(window, ctx);
    ImGui_ImplOpenGL3_Init(glsl_version);

    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, configWindowState?"1":"0");
}

void saturn_load_themes() {
    for (const auto& entry : std::filesystem::directory_iterator("dynos/themes")) {
        std::filesystem::path path = entry.path();
        if (path.extension().string() != ".json") continue;
        std::string id = path.filename().string();
        id = id.substr(0, id.length() - 5);
        std::ifstream stream = std::ifstream(path);
        Json::Value json;
        try {
            json << stream;
        }
        catch (std::runtime_error err) {
            std::cout << "Error parsing theme " << id << std::endl;
            continue;
        }
        if (!json.isMember("name")) continue;
        std::cout << "Found theme " << json["name"].asString() << " (" << id << ")" << std::endl;
        theme_list.push_back({ id, json["name"].asString() });
    }
}

void saturn_imgui_init() {
    saturn_load_themes();
    sdynos_imgui_init();
    smachinima_imgui_init();
    ssettings_imgui_init();
    
    saturn_load_project_list();
    saturn_load_macros();
}

void saturn_imgui_handle_events(SDL_Event * event) {
    ImGui_ImplSDL2_ProcessEvent(event);
    switch (event->type){
        case SDL_KEYDOWN:
            if(event->key.keysym.sym == SDLK_F3) {
                saturn_play_keyframe();
            }
            if(event->key.keysym.sym == SDLK_F4) {
                limit_fps = !limit_fps;
                configWindow.fps_changed = true;
            }

            // this is for YOU, baihsense
            // here's your crash key
            //    - bup
            if(event->key.keysym.sym == SDLK_SCROLLLOCK) {
                imgui_update_theme();
            }
            if(event->key.keysym.sym == SDLK_F9) {
                DynOS_Gfx_GetPacks().Clear();
                DynOS_Opt_Init();
                //model_details = "" + std::to_string(DynOS_Gfx_GetPacks().Count()) + " model pack";
                //if (DynOS_Gfx_GetPacks().Count() != 1) model_details += "s";

                if (gCurrLevelNum > 3 || !mario_exists)
                    DynOS_ReturnToMainMenu();
            }

            if(event->key.keysym.sym == SDLK_F6) {
                k_popout_open = !k_popout_open;
            }
        
        break;
    }
    smachinima_imgui_controls(event);
}

void saturn_keyframe_sort(std::vector<Keyframe>* keyframes) {
    for (int i = 0; i < keyframes->size(); i++) {
        for (int j = i + 1; j < keyframes->size(); j++) {
            if ((*keyframes)[i].position < (*keyframes)[j].position) continue;
            Keyframe temp = (*keyframes)[i];
            (*keyframes)[i] = (*keyframes)[j];
            (*keyframes)[j] = temp;
        }
    }
}

uint32_t startFrame = 0;
uint32_t endFrame = 60;
int endFrameText = 60;

std::vector<std::string> find_timeline_group(std::string timeline, std::string* out = nullptr) {
    for (auto& entry : k_groups) {
        if (std::find(entry.second.begin(), entry.second.end(), timeline) != entry.second.end()) {
            if (out != nullptr) *out = entry.first;
            return entry.second;
        }
    }
    return { timeline };
}

bool saturn_keyframe_group_matches(std::string id, int frame) {
    std::vector<std::string> group = find_timeline_group(id);
    for (std::string timeline : group) {
        if (!saturn_keyframe_matches(timeline, frame)) return false;
    }
    return true;
}

void saturn_keyframe_window() {
#ifdef DISCORDRPC
    discord_state = "In-Game // Keyframing";
#endif

    std::string windowLabel = "Timeline###kf_timeline";
    ImGuiWindowFlags timeline_flags = imgui_bundled_window_corner(1, 0, 0, 0.64f);
    ImGui::Begin(windowLabel.c_str(), &k_popout_open, timeline_flags);
    if (!keyframe_playing) {
        if (ImGui::Button(ICON_FK_PLAY " Play###k_t_play")) {
            startFrame = 0;
            saturn_play_keyframe();
        }
        ImGui::SameLine();
        ImGui::Checkbox("Loop###k_t_loop", &k_loop);
    } else {
        if (ImGui::Button(ICON_FK_STOP " Stop###k_t_stop")) {
            keyframe_playing = false;
        }
        ImGui::SameLine();
        ImGui::Checkbox("Loop###k_t_loop", &k_loop);
    }

    ImGui::Separator();

    ImGui::PushItemWidth(35);
    if (ImGui::InputInt("Frames", &endFrameText, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (endFrameText >= 60) {
            endFrame = (uint32_t)endFrameText;
        } else {
            endFrame = 60;
            endFrameText = 60;
        }
    };
    ImGui::PopItemWidth();
            
    // Scrolling
    int scroll = keyframe_playing ? (k_current_frame - startFrame) == 30 ? 1 : 0 : (ImGui::IsWindowHovered() ? (int)(ImGui::GetMouseScrollY() * -2) : 0);
    if (scroll >= 60) scroll = 59;
    startFrame += scroll;
    if (startFrame + 60 > endFrame) startFrame = endFrame - 60;
    if (startFrame > endFrame) startFrame = 0;
    uint32_t end = min(60, endFrame - startFrame) + startFrame;

    // Sequencer
    std::vector<std::string> skip = {};
    if (ImGui::BeginNeoSequencer("Sequencer###k_sequencer", (uint32_t*)&k_current_frame, &startFrame, &end, ImVec2((end - startFrame) * 6, 0), ImGuiNeoSequencerFlags_HideZoom)) {
        for (auto& entry : k_frame_keys) {
            if (std::find(skip.begin(), skip.end(), entry.first) != skip.end()) continue;
            std::string name = entry.second.first.name;
            for (std::string groupMember : find_timeline_group(entry.first, &name)) {
                skip.push_back(groupMember);
            }
            if (ImGui::BeginNeoTimeline(name.c_str(), entry.second.second)) {
                ImGui::EndNeoTimeLine();
            }
        }
        ImGui::EndNeoSequencer();
    }

    // Keyframes
    if (!keyframe_playing) {
        std::map<std::string, bool> shouldEditKeyframe = {};
        for (auto& entry : k_frame_keys) {
            if (shouldEditKeyframe.find(entry.first) != shouldEditKeyframe.end()) continue;
            std::vector<std::string> group = find_timeline_group(entry.first);
            bool doEdit = !saturn_keyframe_group_matches(entry.first, k_current_frame);
            for (std::string groupEntry : group) {
                shouldEditKeyframe.insert({ groupEntry, doEdit });
            }
        }
        for (auto& entry : k_frame_keys) {
            KeyframeTimeline timeline = entry.second.first;
            std::vector<Keyframe>* keyframes = &entry.second.second;

            if (k_previous_frame == k_current_frame && shouldEditKeyframe[entry.first]) {
                // We create a keyframe here
                int keyframeIndex = 0;
                for (int i = 0; i < keyframes->size(); i++) {
                    if (k_current_frame >= (*keyframes)[i].position) keyframeIndex = i;
                }
                if ((*keyframes)[keyframeIndex].position == k_current_frame) {
                    Keyframe* keyframe = &(*keyframes)[keyframeIndex];
                    if (timeline.type == KFTYPE_BOOL) (*keyframe).value = *(bool*)timeline.dest ? 1 : 0;
                    if (timeline.type == KFTYPE_FLOAT) (*keyframe).value = *(float*)timeline.dest;
                    if (timeline.type == KFTYPE_FLAGS) (*keyframe).value = *(float*)timeline.dest;
                    if (timeline.forceWait) (*keyframe).curve = InterpolationCurve::WAIT;
                }
                else {
                    Keyframe keyframe = Keyframe(k_current_frame, (*keyframes)[keyframeIndex].curve);
                    keyframe.timelineID = entry.first;
                    if (timeline.type == KFTYPE_BOOL) keyframe.value = *(bool*)timeline.dest ? 1 : 0;
                    if (timeline.type == KFTYPE_FLOAT) keyframe.value = *(float*)timeline.dest;
                    if (timeline.type == KFTYPE_FLAGS) keyframe.value = *(float*)timeline.dest;
                    if (timeline.forceWait) keyframe.curve = InterpolationCurve::WAIT;
                    keyframes->push_back(keyframe);
                }
                saturn_keyframe_sort(keyframes);
            }
            else saturn_keyframe_apply(entry.first, k_current_frame);
        }
    }
    ImGui::End();

    // Auto focus (use controls without clicking window first)
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_None) && saturn_disable_sm64_input()) {
        ImGui::SetWindowFocus(windowLabel.c_str());
    }

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_None)) {
        for (auto& entry : k_frame_keys) {
            if (entry.second.first.name.find(", Main") != string::npos || entry.second.first.name.find(", Shade") != string::npos) {
                // Apply color keyframes
                //apply_cc_from_editor();
            }
        }
    }

    if (camera.frozen) {
        if (keyframe_playing || k_current_frame != k_previous_frame) {
            should_update_cam_from_keyframes = false;
            vec3f_copy(gCamera->pos, freezecamPos);
            vec3f_set_dist_and_angle(gCamera->pos, gCamera->focus, 100, freezecamPitch, freezecamYaw);
            gLakituState.roll = freezecamRoll;
        }
        else {
            float dist;
            s16 yaw;
            s16 pitch;
            vec3f_copy(freezecamPos, gCamera->pos);
            vec3f_get_dist_and_angle(gCamera->pos, gCamera->focus, &dist, &pitch, &yaw);
            freezecamYaw = (float)yaw;
            freezecamPitch = (float)pitch;
            freezecamRoll = (float)gLakituState.roll;
        }
        vec3f_copy(gLakituState.pos, gCamera->pos);
        vec3f_copy(gLakituState.focus, gCamera->focus);
        vec3f_copy(gLakituState.goalPos, gCamera->pos);
        vec3f_copy(gLakituState.goalFocus, gCamera->focus);
        gCamera->yaw = calculate_yaw(gCamera->focus, gCamera->pos);
        gLakituState.yaw = gCamera->yaw;
    }

    k_previous_frame = k_current_frame;
}

char saturnProjectFilename[257] = "Project";
int current_project_id;

void saturn_imgui_update() {
    if (!splash_finished) return;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    ImGui::NewFrame();
    
    windowStartHeight = (showStatusBars) ? 48 : 30;

    camera.savestate_mult = 1.f;

    if (showMenu) {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Menu")) {
                windowCcEditor = false;

                if (ImGui::MenuItem(ICON_FK_WINDOW_MAXIMIZE " Show UI",      translate_bind_to_name(configKeyShowMenu[0]), showMenu)) showMenu = !showMenu;
                if (ImGui::MenuItem(ICON_FK_WINDOW_MINIMIZE " Show Status Bars",  NULL, showStatusBars)) showStatusBars = !showStatusBars;
                ImGui::Separator();

                if (ImGui::BeginMenu("Open Project")) {
                    ImGui::BeginChild("###menu_model_ccs", ImVec2(165, 75), true);
                    for (int n = 0; n < project_array.size(); n++) {
                        const bool is_selected = (current_project_id == n);
                        std::string spj_name = project_array[n].substr(0, project_array[n].size() - 4);

                        if (ImGui::Selectable(spj_name.c_str(), is_selected)) {
                            current_project_id = n;
                            if (spj_name.length() <= 256);
                                strcpy(saturnProjectFilename, spj_name.c_str());
                            //saturn_load_project((char*)(spj_name + ".spj").c_str());
                        }

                        if (ImGui::BeginPopupContextItem()) {
                            ImGui::Text("%s.spj", spj_name.c_str());
                            imgui_bundled_tooltip(("/dynos/projects/" + spj_name + ".spj").c_str());
                            if (spj_name != "autosave") {
                                if (ImGui::SmallButton(ICON_FK_TRASH_O " Delete File")) {
                                    saturn_delete_file(project_dir + spj_name + ".spj");
                                    saturn_load_project_list();
                                    ImGui::CloseCurrentPopup();
                                } ImGui::SameLine(); imgui_bundled_help_marker("WARNING: This action is irreversible!");
                            }
                            ImGui::Separator();
                            ImGui::TextDisabled("%i project(s)", project_array.size());
                            if (ImGui::Button(ICON_FK_UNDO " Refresh")) {
                                saturn_load_project_list();
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::EndPopup();
                        }
                    }
                    ImGui::EndChild();
                    ImGui::PushItemWidth(125);
                    ImGui::InputText(".spj###project_file_input", saturnProjectFilename, 256);
                    ImGui::PopItemWidth();
                    if (ImGui::Button(ICON_FA_FILE " Open###project_file_open")) {
                        saturn_load_project((char*)(std::string(saturnProjectFilename) + ".spj").c_str());
                    }
                    ImGui::SameLine(70);
                    if (ImGui::Button(ICON_FA_SAVE " Save###project_file_save")) {
                        saturn_save_project((char*)(std::string(saturnProjectFilename) + ".spj").c_str());
                        saturn_load_project_list();
                    }
                    ImGui::SameLine();
                    imgui_bundled_help_marker("NOTE: Project files are currently EXPERIMENTAL and prone to crashing!");
                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem(ICON_FA_UNDO " Load Autosaved")) {
                    saturn_load_project("autosave.spj");
                }

                ImGui::Separator();

                if (ImGui::BeginMenu(ICON_FK_CODE " Macros")) {
                    ImGui::BeginChild("###menu_macros", ImVec2(165, 75), true);
                    for (int i = 0; i < macro_array.size(); i++) {
                        std::string macro = macro_array[i];
                        bool selected = false;
                        if (filesystem::is_directory("dynos/macros/" + current_macro_dir + macro)) {
                            if (ImGui::Selectable((std::string(ICON_FK_FOLDER " ") + macro).c_str(), &selected)) {
                                if (macro == "../") {
                                    current_macro_dir = macro_dir_stack[macro_dir_stack.size() - 1];
                                    macro_dir_stack.pop_back();
                                }
                                else {
                                    macro_dir_stack.push_back(current_macro_dir);
                                    current_macro_dir += macro;
                                }
                                saturn_load_macros();
                            }
                        }
                        else {
                            selected = i == current_macro_id;
                            if (ImGui::Selectable(macro.c_str(), &selected)) {
                                current_macro_id = i;
                            }
                        }
                    }
                    ImGui::EndChild();
                    if (ImGui::Button(ICON_FK_UNDO " Refresh")) {
                        saturn_load_macros();
                    } ImGui::SameLine();
                    if (current_macro_id == -1) ImGui::BeginDisabled();
                    if (ImGui::Button("Run")) {
                        saturn_cmd_clear_registers();
                        saturn_cmd_eval_file("dynos/macros/" + current_macro_dir + macro_array[current_macro_id]);
                    }
                    if (current_macro_id == -1) ImGui::EndDisabled();
                    ImGui::SameLine();
                    imgui_bundled_help_marker("EXPERIMENTAL - Allows you to run commands and do stuff in bulk.\nCurrently has no debugging or documentation.");
                    ImGui::Checkbox("Command Line", &configEnableCli);
                    ImGui::EndMenu();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Stats",        NULL, windowStats == true)) windowStats = !windowStats;
                if (ImGui::MenuItem(ICON_FK_LINE_CHART " Timeline Editor", "F6", k_popout_open == true)) {
                    k_popout_open = !k_popout_open;
                    windowSettings = false;
                }
                if (ImGui::MenuItem(ICON_FK_COG " Settings",     NULL, windowSettings == true)) {
                    windowSettings = !windowSettings;
                    k_popout_open = false;
                }
                //if (windowStats) imgui_bundled_window_reset("Stats", 250, 125, 10, windowStartHeight);

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Camera")) {
                windowCcEditor = false;

                ImGui::Checkbox("Freeze", &camera.frozen);
                if (camera.frozen) {
                    saturn_keyframe_camera_popout("Camera", "k_c_camera");
                    ImGui::SameLine(200); ImGui::TextDisabled(translate_bind_to_name(configKeyFreeze[0]));

                    if (ImGui::BeginMenu("Options###camera_options")) {
                        camera.savestate_mult = 0.f;
                        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
                        ImGui::BeginChild("###model_metadata", ImVec2(200, 90), true, ImGuiWindowFlags_NoScrollbar);
                        ImGui::TextDisabled("pos %.f, %.f, %.f", gCamera->pos[0], gCamera->pos[1], gCamera->pos[2]);
                        ImGui::TextDisabled("foc %.f, %.f, %.f", gCamera->focus[0], gCamera->focus[1], gCamera->focus[2]);
                        if (ImGui::Button(ICON_FK_FILES_O " Copy###copy_camera")) {
                            saturn_copy_camera(copy_relative);
                            if (copy_relative) saturn_paste_camera();
                            has_copy_camera = 1;
                        } ImGui::SameLine();
                        if (!has_copy_camera) ImGui::BeginDisabled();
                        if (ImGui::Button(ICON_FK_CLIPBOARD " Paste###paste_camera")) {
                            if (has_copy_camera) saturn_paste_camera();
                        }
                        /*ImGui::Checkbox("Loop###camera_paste_forever", &paste_forever);
                        if (paste_forever) {
                            saturn_paste_camera();
                        }*/
                        if (!has_copy_camera) ImGui::EndDisabled();
                        ImGui::Checkbox("Relative to Mario###camera_copy_relative", &copy_relative);

                        ImGui::EndChild();
                        ImGui::PopStyleVar();

                        ImGui::Text(ICON_FK_VIDEO_CAMERA " Speed");
                        ImGui::PushItemWidth(150);
                        ImGui::SliderFloat("Move", &camVelSpeed, 0.0f, 2.0f);
                        if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) { camVelSpeed = 1.f; }
                        ImGui::SliderFloat("Rotate", &camVelRSpeed, 0.0f, 2.0f);
                        if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) { camVelRSpeed = 1.f; }
                        ImGui::PopItemWidth();
                        ImGui::Text(ICON_FK_KEYBOARD_O " Control Mode");
                        const char* mCameraSettings[] = { "Keyboard", "Keyboard/Gamepad (Old)", "Mouse (Experimental)" };
                        ImGui::PushItemWidth(200);
                        ImGui::Combo("###camera_mode", (int*)&configMCameraMode, mCameraSettings, IM_ARRAYSIZE(mCameraSettings));
                        ImGui::PopItemWidth();
                        if (configMCameraMode == 2) {
                            imgui_bundled_tooltip("Move Camera -> LShift + Mouse Buttons");
                        } else if (configMCameraMode == 1) {
                            imgui_bundled_tooltip("Pan Camera -> R + C-Buttons\nRaise/Lower Camera -> L + C-Buttons\nRotate Camera -> L + Crouch + C-Buttons");
                        } else if (configMCameraMode == 0) {
                            imgui_bundled_tooltip("Move Camera -> Y/G/H/J\nRaise/Lower Camera -> T/U\nRotate Camera -> R + Y/G/H/J");
                        }
                        ImGui::EndMenu();
                    }
                }
                ImGui::Separator();
                ImGui::PushItemWidth(100);
                ImGui::SliderFloat("FOV", &camera_fov, 0.0f, 100.0f);
                if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) { camera_fov = 50.f; }
                imgui_bundled_tooltip("Controls the FOV of the in-game camera; Default is 50, or 40 in Project64.");
                saturn_keyframe_float_popout(&camera_fov, "FOV", "k_fov");
                ImGui::SameLine(200); ImGui::TextDisabled("N/M");
                ImGui::PopItemWidth();
                ImGui::Checkbox("Smooth###fov_smooth", &camera.fov_smooth);

                ImGui::Separator();
                ImGui::PushItemWidth(100);
                ImGui::SliderFloat("Follow", &camera.focus, 0.0f, 1.0f);
                if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) { camera.focus = 1.f; }
                saturn_keyframe_float_popout(&camera.focus, "Follow", "k_focus");
                ImGui::PopItemWidth();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Appearance")) {
                sdynos_imgui_menu();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Game")) {
                imgui_machinima_quick_options();
                ImGui::EndMenu();
            }

            if (ImGui::MenuItem(ICON_FK_EYEDROPPER " CHROMA KEY", NULL, autoChroma)) {
                schroma_imgui_init();
                autoChroma = !autoChroma;
                windowCcEditor = false;
                windowAnimPlayer = false;

                for (int i = 0; i < 960; i++) {
                    gObjectPool[i].header.gfx.node.flags &= ~GRAPH_RENDER_INVISIBLE;
                    if (autoChroma && !autoChromaObjects) gObjectPool[i].header.gfx.node.flags |= GRAPH_RENDER_INVISIBLE;
                }
            }
            ImGui::EndMainMenuBar();
        }

        ImGuiViewportP* viewport = (ImGuiViewportP*)(void*)ImGui::GetMainViewport();
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
        float height = ImGui::GetFrameHeight();

        if (showStatusBars) {
            if (ImGui::BeginViewportSideBar("##SecondaryMenuBar", viewport, ImGuiDir_Up, height, window_flags)) {
                if (ImGui::BeginMenuBar()) {
                    if (configFps60) ImGui::TextDisabled("%.1f FPS (%.3f ms/frame)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
                    else ImGui::TextDisabled("%.1f FPS (%.3f ms/frame)", ImGui::GetIO().Framerate / 2, 1000.0f / (ImGui::GetIO().Framerate / 2));
                    ImGui::EndMenuBar();
                }
                ImGui::End();
            }

            if (ImGui::BeginViewportSideBar("##MainStatusBar", viewport, ImGuiDir_Down, height, window_flags)) {
                if (ImGui::BeginMenuBar()) {
                    ImGui::Text(PLATFORM_ICON " ");
#ifdef GIT_BRANCH
#ifdef GIT_HASH
                    ImGui::SameLine(20);
                    ImGui::TextDisabled(ICON_FK_GITHUB " " GIT_BRANCH " " GIT_HASH);
#endif
#endif
                    ImGui::SameLine(ImGui::GetWindowWidth() - 135);
                    ImGui::Text("Autosaving in %ds", autosaveDelay / 30);
                    ImGui::EndMenuBar();
                }
                ImGui::End();
            }
        }

        if (configEnableCli)
        if (ImGui::BeginViewportSideBar("###CliMenuBar", viewport, ImGuiDir_Down, height, window_flags)) {
            if (ImGui::BeginMenuBar()) {
                ImGui::Dummy({ 0, 0 });
                ImGui::SameLine(-7);
                ImGui::PushItemWidth(ImGui::GetWindowWidth());
                if (ImGui::InputText("###cli_input", cli_input, CLI_MAX_INPUT_SIZE, ImGuiInputTextFlags_EnterReturnsTrue)) {
                    is_cli = true;
                    saturn_cmd_eval(std::string(cli_input));
                    cli_input[0] = 0;
                    ImGui::SetKeyboardFocusHere(-1);
                }
                ImGui::PopItemWidth();
                ImGui::EndMenuBar();
            }
            ImGui::End();
        }

        if (windowStats) {
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
            ImGuiWindowFlags stats_flags = imgui_bundled_window_corner(2, 0, 0, 0.64f);
            ImGui::Begin("Stats", &windowStats, stats_flags);

#ifdef DISCORDRPC
            if (has_discord_init && gCurUser.username != "") {
                if (gCurUser.username == NULL || gCurUser.username == "") {
                    ImGui::Text(ICON_FK_DISCORD " Loading...");
                } else {
                    std::string disc = gCurUser.discriminator;
                    if (disc == "0") ImGui::Text(ICON_FK_DISCORD " @%s", gCurUser.username);
                    else ImGui::Text(ICON_FK_DISCORD " %s#%s", gCurUser.username, gCurUser.discriminator);
                }
                ImGui::Separator();
            }
#endif
            ImGui::Text(ICON_FK_FOLDER_OPEN " %i model pack(s)", model_list.size());
            ImGui::Text(ICON_FK_FILE_TEXT " %i color code(s)", color_code_list.size());
            ImGui::TextDisabled(ICON_FK_PICTURE_O " %i textures loaded", preloaded_textures_count);

            ImGui::End();
            ImGui::PopStyleColor();
        }
        if (windowCcEditor && support_color_codes && current_model.ColorCodeSupport) {
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
            ImGuiWindowFlags cce_flags = imgui_bundled_window_corner(0, 0, 0, 1.f);
            ImGui::Begin("Color Code Editor", &windowCcEditor, cce_flags);
            OpenCCEditor();
            ImGui::End();
            ImGui::PopStyleColor();

#ifdef DISCORDRPC
            discord_state = "In-Game // Editing a CC";
#endif
        }
        if (windowAnimPlayer && mario_exists) {
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
            ImGuiWindowFlags anim_flags = imgui_bundled_window_corner(0, 0, 0, 1.f);
            ImGui::Begin("Animation Mixtape", &windowAnimPlayer, anim_flags);
            imgui_machinima_animation_player();
            ImGui::End();
            ImGui::PopStyleColor();
        }
        if (windowSettings) {
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
            ImGuiWindowFlags settings_flags = imgui_bundled_window_corner(1, 0, 0, 0.64f);
            ImGui::Begin("Settings", &windowSettings, settings_flags);
            ssettings_imgui_update();
            ImGui::End();
            ImGui::PopStyleColor();
        }
        if (autoChroma && mario_exists) {
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
            ImGuiWindowFlags chroma_flags = imgui_bundled_window_corner(0, 0, 0, 1.f);
            if (chromaRequireReload) chroma_flags |= ImGuiWindowFlags_UnsavedDocument;
            ImGui::Begin("Chroma Key Settings", &autoChroma, chroma_flags);
            schroma_imgui_update();
            ImGui::End();
            ImGui::PopStyleColor();
        }
        if (k_popout_open) {
            saturn_keyframe_window();
        }
        if (k_context_popout_open) {
            ImGui::SetNextWindowBgAlpha(0.64f);
            ImGui::SetNextWindowPos(k_context_popout_pos);
            if (ImGui::Begin("###kf_menu", &k_context_popout_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove)) {
                vector<Keyframe>* keyframes = &k_frame_keys[k_context_popout_keyframe.timelineID].second;
                int curve = -1;
                int index = -1;
                for (int i = 0; i < keyframes->size(); i++) {
                    if ((*keyframes)[i].position == k_context_popout_keyframe.position) index = i;
                }
                if (k_context_popout_keyframe.position == 0) ImGui::BeginDisabled();
                bool doDelete = false;
                bool doCopy = false;
                if (ImGui::MenuItem("Delete")) {
                    doDelete = true;
                }
                if (ImGui::MenuItem("Move to Pointer")) {
                    doDelete = true;
                    doCopy = true;
                }
                if (k_context_popout_keyframe.position == 0) ImGui::EndDisabled();
                if (ImGui::MenuItem("Copy to Pointer")) {
                    doCopy = true;
                }
                ImGui::Separator();
                bool forceWait = k_frame_keys[k_context_popout_keyframe.timelineID].first.forceWait;
                if (forceWait) ImGui::BeginDisabled();
                for (int i = 0; i < IM_ARRAYSIZE(curveNames); i++) {
                    if (ImGui::MenuItem(curveNames[i].c_str(), NULL, k_context_popout_keyframe.curve == InterpolationCurve(i))) {
                        curve = i;
                    }
                }
                if (forceWait) ImGui::EndDisabled();
                ImVec2 pos = ImGui::GetWindowPos();
                ImVec2 size = ImGui::GetWindowSize();
                ImVec2 mouse = ImGui::GetMousePos();
                if ((mouse.x < pos.x || mouse.y < pos.y || mouse.x >= pos.x + size.x || mouse.y >= pos.y + size.y) && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle) || ImGui::IsMouseClicked(ImGuiMouseButton_Right))) k_context_popout_open = false;
                ImGui::End();
                std::vector<std::string> affectedTimelines = find_timeline_group(k_context_popout_keyframe.timelineID);
                for (std::string timeline : affectedTimelines) {
                    keyframes = &k_frame_keys[timeline].second;
                    if (curve != -1) {
                        (*keyframes)[index].curve = InterpolationCurve(curve);
                        k_context_popout_open = false;
                        k_previous_frame = -1;
                    }
                    if (doCopy) {
                        int hoverKeyframeIndex = -1;
                        for (int i = 0; i < keyframes->size(); i++) {
                            if ((*keyframes)[i].position == k_current_frame) hoverKeyframeIndex = i;
                        }
                        Keyframe copy = Keyframe(k_current_frame, (*keyframes)[index].curve);
                        copy.value = (*keyframes)[index].value;
                        copy.timelineID = (*keyframes)[index].timelineID;
                        if (hoverKeyframeIndex == -1) keyframes->push_back(copy);
                        else (*keyframes)[hoverKeyframeIndex] = copy;
                        k_context_popout_open = false;
                        k_previous_frame = -1;
                    }
                    if (doDelete) {
                        keyframes->erase(keyframes->begin() + index);
                        k_context_popout_open = false;
                        k_previous_frame = -1;
                    }
                    saturn_keyframe_sort(keyframes);
                }
            }
        }

        //ImGui::ShowDemoWindow();
    }

    is_cc_editing = windowCcEditor & support_color_codes & current_model.ColorCodeSupport;

    ImGui::Render();
    GLint last_program;
    glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
    glUseProgram(0);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glUseProgram(last_program);

    if (was_camera_frozen && !camera.frozen && k_frame_keys.find("k_c_camera_pos0") != k_frame_keys.end()) {
        k_frame_keys.erase("k_c_camera_pos0");
        k_frame_keys.erase("k_c_camera_pos1");
        k_frame_keys.erase("k_c_camera_pos2");
        k_frame_keys.erase("k_c_camera_yaw");
        k_frame_keys.erase("k_c_camera_pitch");
        k_frame_keys.erase("k_c_camera_roll");
    }
    was_camera_frozen = camera.frozen;

    //if (!is_gameshark_open) apply_cc_from_editor();
}

/*
    ======== SATURN ========
    Advanced Keyframe Engine
    ========================
*/

void saturn_keyframe_context_popout(Keyframe keyframe) {
    if (keyframe_playing || k_context_popout_open) return;
    k_context_popout_open = true;
    k_context_popout_pos = ImGui::GetMousePos();
    k_context_popout_keyframe = keyframe;
}

void saturn_create_timeline(void* dest, KeyframeType type, std::string name, int precision, bool forceWait, float value, InterpolationCurve curve, int position, std::string timelineID) {
    KeyframeTimeline timeline = KeyframeTimeline(dest, type, name, precision, forceWait);
    Keyframe keyframe = Keyframe(value, curve, position, timelineID);
    //k_frame_keys.insert({ timelineID, std::make_pair(timeline, std::vector<Keyframe>{keyframe})});
    k_current_frame = 0;
    startFrame = 0;
} // (TODO) -> Further refactor this code

void saturn_keyframe_float_popout(float* edit_value, string value_name, string id) {
    bool contains = saturn_timeline_exists(id.c_str());

    string buttonLabel = ICON_FK_LINK "###kb_" + id;

    ImGui::SameLine();
    if (ImGui::Button(buttonLabel.c_str())) {
        k_popout_open = true;
        if (contains) k_frame_keys.erase(id);
        else { // Add the timeline
            KeyframeTimeline timeline = KeyframeTimeline(edit_value, KFTYPE_FLOAT, value_name, -2, false);
            Keyframe keyframe = Keyframe(*edit_value, InterpolationCurve::LINEAR, 0, id);
            k_frame_keys.insert({ id, std::make_pair(timeline, std::vector<Keyframe>{ keyframe }) });
            k_current_frame = 0;
            startFrame = 0;
        }
    }
    imgui_bundled_tooltip(contains ? "Remove" : "Animate");
}
void saturn_keyframe_bool_popout(bool* edit_value, string value_name, string id) {
    bool contains = saturn_timeline_exists(id.c_str());

    string buttonLabel = ICON_FK_LINK "###kb_" + id;

    ImGui::SameLine();
    if (ImGui::Button(buttonLabel.c_str())) {
        k_popout_open = true;
        if (contains) k_frame_keys.erase(id);
        else { // Add the timeline
            KeyframeTimeline timeline = KeyframeTimeline(edit_value, KFTYPE_BOOL, value_name, -2, true);
            Keyframe keyframe = Keyframe((*edit_value ? 1 : 0), InterpolationCurve::WAIT, 0, id);
            k_frame_keys.insert({ id, std::make_pair(timeline, std::vector<Keyframe>{ keyframe }) });
            k_current_frame = 0;
            startFrame = 0;
        }
    }
    imgui_bundled_tooltip(contains ? "Remove" : "Animate");
}
void saturn_keyframe_camera_popout(string value_name, string id) {
    bool contains = saturn_timeline_exists(id.c_str());

    string buttonLabel = ICON_FK_LINK "###kb_" + id;

    ImGui::SameLine();
    if (ImGui::Button(buttonLabel.c_str())) {
        float dist;
        s16 pitch, yaw;
        vec3f_copy(freezecamPos, gCamera->pos);
        vec3f_get_dist_and_angle(gCamera->pos, gCamera->focus, &dist, &pitch, &yaw);
        freezecamYaw = (float)yaw;
        freezecamPitch = (float)pitch;
        // ((id, name), (precision, value_ptr))
        std::pair<std::pair<std::string, std::string>, std::pair<int, float*>> values[] = {
            std::make_pair(std::make_pair("pos0", "Pos X"), std::make_pair(0, &freezecamPos[0])),
            std::make_pair(std::make_pair("pos1", "Pos Y"), std::make_pair(0, &freezecamPos[1])),
            std::make_pair(std::make_pair("pos2", "Pos Z"), std::make_pair(0, &freezecamPos[2])),
            std::make_pair(std::make_pair("yaw", "Yaw"), std::make_pair(2, &freezecamYaw)),
            std::make_pair(std::make_pair("pitch", "Pitch"), std::make_pair(2, &freezecamPitch)),
            std::make_pair(std::make_pair("roll", "Roll"), std::make_pair(2, &freezecamRoll)),
        };
        k_popout_open = true;
        if (contains) {
            for (int i = 0; i < IM_ARRAYSIZE(values); i++) {
                k_frame_keys.erase(id + "_" + values[i].first.first);
            }
        }
        else { // Add the timeline
            for (int i = 0; i < IM_ARRAYSIZE(values); i++) {
                KeyframeTimeline timeline = KeyframeTimeline(values[i].second.second, KFTYPE_FLOAT, (value_name + " " + values[i].first.second), values[i].second.first, false);
                Keyframe keyframe = Keyframe(*values[i].second.second, InterpolationCurve::LINEAR, 0, (id + "_" + values[i].first.first));
                k_frame_keys.insert({ id + "_" + values[i].first.first, std::make_pair(timeline, std::vector<Keyframe>{ keyframe }) });
                k_current_frame = 0;
                startFrame = 0;
            }
        }
    }
    imgui_bundled_tooltip(contains ? "Remove" : "Animate");
}

void saturn_keyframe_color_popout(string value_name, string id, float* r, float* g, float* b) {
    bool contains = saturn_timeline_exists(id.c_str());

    string buttonLabel = ICON_FK_LINK "###kb_" + id;

    ImGui::SameLine();
    if (ImGui::Button(buttonLabel.c_str())) {
        // ((id, name), (precision, value_ptr))
        std::pair<std::pair<std::string, std::string>, std::pair<int, float*>> values[] = {
            std::make_pair(std::make_pair("r", "R"), std::make_pair(-3, r)),
            std::make_pair(std::make_pair("g", "G"), std::make_pair(-3, g)),
            std::make_pair(std::make_pair("b", "B"), std::make_pair(-3, b)),
        };
        k_popout_open = true;
        if (contains) {
            for (int i = 0; i < IM_ARRAYSIZE(values); i++) {
                k_frame_keys.erase(id + "_" + values[i].first.first);
            }
        }
        else { // Add the timeline
            for (int i = 0; i < IM_ARRAYSIZE(values); i++) {
                KeyframeTimeline timeline = KeyframeTimeline(values[i].second.second, (value_name + " " + values[i].first.second), values[i].second.first, false);
                Keyframe keyframe = Keyframe(*values[i].second.second, InterpolationCurve::LINEAR, 0, (id + "_" + values[i].first.first));
                k_frame_keys.insert({ id + "_" + values[i].first.first, std::make_pair(timeline, std::vector<Keyframe>{ keyframe }) });
                k_current_frame = 0;
                startFrame = 0;
            }
        }
    }
    imgui_bundled_tooltip(contains ? "Remove" : "Animate");
}
void saturn_keyframe_rotation_popout(string value_name, string id, float* yaw, float* pitch) {
    bool contains = saturn_timeline_exists(id.c_str());

    string buttonLabel = ICON_FK_LINK "###kb_" + id;

    if (ImGui::Button(buttonLabel.c_str())) {
        // ((id, name), (precision, value_ptr))
        std::pair<std::pair<std::string, std::string>, std::pair<int, float*>> values[] = {
            std::make_pair(std::make_pair("yaw", "Yaw"), std::make_pair(0, yaw)),
            std::make_pair(std::make_pair("pitch", "Pitch"), std::make_pair(0, pitch)),
        };
        k_popout_open = true;
        if (contains) {
            for (int i = 0; i < IM_ARRAYSIZE(values); i++) {
                k_frame_keys.erase(id + "_" + values[i].first.first);
            }
        }
        else { // Add the timeline
            for (int i = 0; i < IM_ARRAYSIZE(values); i++) {
                KeyframeTimeline timeline = KeyframeTimeline(values[i].second.second, (value_name + " " + values[i].first.second), values[i].second.first, false);
                Keyframe keyframe = Keyframe(*values[i].second.second, InterpolationCurve::LINEAR, 0, (id + "_" + value[i].first.first));
                k_frame_keys.insert({ id + "_" + values[i].first.first, std::make_pair(timeline, std::vector<Keyframe>{ keyframe }) });
                k_current_frame = 0;
                startFrame = 0;
            }
        }
    }
    imgui_bundled_tooltip(contains ? "Remove" : "Animate");
}

void saturn_keyframe_anim_popout(string value_name, string id) {
    bool contains = saturn_timeline_exists(id.c_str());

    string buttonLabel = ICON_FK_LINK "###kb_" + id;

    if (ImGui::Button(buttonLabel.c_str())) {
        k_popout_open = true;
        if (contains) k_frame_keys.erase(id);
        else { // Add the timeline
            KeyframeTimeline timeline = KeyframeTimeline(&k_current_anim, KFTYPE_FLAGS, value_name, 0, true);
            Keyframe keyframe = Keyframe(*(float*)&k_current_anim, InterpolationCurve::WAIT, 0, id);
            k_frame_keys.insert({ id, std::make_pair(timeline, std::vector<Keyframe>{ keyframe }) });
            k_current_frame = 0;
            startFrame = 0;
        }
    }
    imgui_bundled_tooltip(contains ? "Remove" : "Animate");

    if (contains && k_current_frame == 0) {
        ImGui::SameLine();
        if (ImGui::Button(ICON_FK_UNDO "###kb_mario_anim_reset")) {
            k_current_anim = -1;
            place_keyframe_anim = true;
        }
        imgui_bundled_tooltip("Replace with empty keyframe");
    }
}

bool saturn_disable_sm64_input() {
    return ImGui::GetIO().WantTextInput;
}

