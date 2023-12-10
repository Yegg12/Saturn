#include "saturn/saturn_textures.h"

#include <string>
#include <iostream>
#include <vector>
#include <algorithm>
#include <SDL2/SDL.h>

#include "saturn/saturn.h"
#include "saturn/saturn_colors.h"
#include "saturn/imgui/saturn_imgui.h"
#include "saturn/imgui/saturn_imgui_chroma.h"

#include "saturn/libs/imgui/imgui.h"
#include "saturn/libs/imgui/imgui_internal.h"
#include "saturn/libs/imgui/imgui_impl_sdl.h"
#include "saturn/libs/imgui/imgui_impl_opengl3.h"

#include "pc/configfile.h"

extern "C" {
#include "game/mario.h"
#include "game/camera.h"
#include "game/level_update.h"
#include "sm64.h"
#include "pc/gfx/gfx_pc.h"
}

using namespace std;
#include <dirent.h>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <assert.h>
#include <stdlib.h>
namespace fs = std::filesystem;
#include "pc/fs/fs.h"

#include "saturn/saturn_json.h"

bool custom_eyes_enabled;
bool show_vmario_emblem;

Texpression VanillaEyes;
/* Loads textures from dynos/eyes/ into a global Texpression */
void LoadEyesFolder() {
    // Check if the dynos/eyes/ folder exists
    if (fs::is_directory("dynos/eyes")) {

        VanillaEyes.Name = "eyes";
        VanillaEyes.FolderPath = "dynos/eyes";

        // Load Textures
        for (const auto & entry : fs::directory_iterator("dynos/eyes")) {
            // Only allow PNG files
            if (entry.path().extension() == ".png") {
                TexturePath texture;
                texture.FileName = entry.path().filename().u8string();
                texture.FilePath = entry.path().generic_u8string();
                VanillaEyes.Textures.push_back(texture);
            }
        }
    }
}


/* Loads textures into an expression */
std::vector<TexturePath> LoadExpressionTextures(Texpression expression) {
    std::vector<TexturePath> textures;

    // Check if the expression's folder exists
    if (fs::is_directory(fs::path(expression.FolderPath))) {
        for (const auto & entry : fs::directory_iterator(expression.FolderPath)) {
            if (!fs::is_directory(entry.path())) {
                // Only allow PNG files
                if (entry.path().extension() == ".png") {
                    TexturePath texture;
                    texture.FileName = entry.path().filename().u8string();
                    texture.FilePath = entry.path().generic_u8string();
                    textures.push_back(texture);
                }
            }
        }
    }
    return textures;
}

/* Loads an expression list into a specified model */
std::vector<Texpression> LoadExpressions(std::string modelFolderPath) {
    std::vector<Texpression> expressions_list;

    // Check if the model's /expressions folder exists
    if (fs::is_directory(fs::path(modelFolderPath + "/expressions"))) {
        for (const auto & entry : fs::directory_iterator(modelFolderPath + "/expressions")) {
            // Load individual expression folders
            if (fs::is_directory(entry.path())) {

                Texpression expression;
                expression.Name = entry.path().filename().u8string();
                if (expression.Name == "eye")
                    expression.Name = "eyes";

                expression.FolderPath = entry.path().u8string();
                // Load all PNG files
                expression.Textures = LoadExpressionTextures(expression);

                // Eyes will always appear first
                if (expression.Name == "eyes") {
                    expressions_list.insert(expressions_list.begin(), expression);
                } else {
                    expressions_list.push_back(expression);
                }
            }
        }
    }

    return expressions_list;
}

/*
    Handles texture replacement. Called from gfx_pc.c
*/
const void* saturn_bind_texture(const void* input) {
    const char* inputTexture = static_cast<const char*>(input);
    const char* outputTexture;

    if (input == (const void*)0x7365727574786574) return input;
    
    std::string texName = inputTexture;

    // Custom model expressions and eye textures
    if (current_model.Active && texName.find("saturn_") != std::string::npos) {
        for (int i = 0; i < current_model.Expressions.size(); i++) {
            Texpression expression = current_model.Expressions[i];
            // Checks if the incoming texture has the expression's "key"
            // This could be "saturn_eye", "saturn_mouth", etc.

            if (expression.PathHasReplaceKey(texName)) {
                std::cout << texName << " -> " << expression.Textures[expression.CurrentIndex].GetRelativePath() << std::endl;
                outputTexture = expression.Textures[expression.CurrentIndex].GetRelativePath().c_str();
                const void* output = static_cast<const void*>(outputTexture);
                return output;
            }
        }
    }

    if (custom_eyes_enabled && current_model.UsingVanillaEyes() && VanillaEyes.Textures.size() > 0) {
        if (texName.find("saturn_eye") != string::npos ||
            texName == "actors/mario/mario_eyes_left_unused.rgba16.png" ||
            texName == "actors/mario/mario_eyes_right_unused.rgba16.png" ||
            texName == "actors/mario/mario_eyes_up_unused.rgba16.png" ||
            texName == "actors/mario/mario_eyes_down_unused.rgba16.png") {
                outputTexture = VanillaEyes.Textures[VanillaEyes.CurrentIndex].GetRelativePath().c_str();
                const void* output = static_cast<const void*>(outputTexture);
                return output;
        }
    }

    // Non-model custom blink cycle
    /*if (force_blink && eye_array.size() > 0 && is_replacing_eyes) {
        if (texName == "actors/mario/mario_eyes_center.rgba16.png" && blink_eye_1 != "") {
            outputTexture = blink_eye_1.c_str();
            const void* output = static_cast<const void*>(outputTexture);
            return output;
        } else if (texName == "actors/mario/mario_eyes_half_closed.rgba16.png" && blink_eye_2 != "") {
            outputTexture = blink_eye_2.c_str();
            const void* output = static_cast<const void*>(outputTexture);
            return output;
        } else if (texName == "actors/mario/mario_eyes_closed.rgba16.png" && blink_eye_3 != "") {
            outputTexture = blink_eye_3.c_str();
            const void* output = static_cast<const void*>(outputTexture);
            return output;
        }
    }*/

    // Non-model cap logo/emblem

    if (show_vmario_emblem) {
        if (texName == "actors/mario/no_m.rgba16.png")
            return "actors/mario/mario_logo.rgba16.png";
    }

    // AUTO-CHROMA

    // Overwrite skybox
    // This runs for both Auto-chroma and the Chroma Key Stage
    if (autoChroma || gCurrLevelNum == LEVEL_SA) {
        if (use_color_background) {
            // Use white, recolorable textures for our color background
            if (texName.find("textures/skyboxes/") != string::npos)
                return "textures/saturn/white.rgba16.png";
        } else {
            // Swapping skyboxes IDs
            if (texName.find("textures/skyboxes/water") != string::npos) {
                switch(gChromaKeyBackground) {
                    case 0: return static_cast<const void*>(texName.replace(22, 5, "water").c_str());
                    case 1: return static_cast<const void*>(texName.replace(22, 5, "bitfs").c_str());
                    case 2: return static_cast<const void*>(texName.replace(22, 5, "wdw").c_str());
                    case 3: return static_cast<const void*>(texName.replace(22, 5, "cloud_floor").c_str());
                    case 4: return static_cast<const void*>(texName.replace(22, 5, "ccm").c_str());
                    case 5: return static_cast<const void*>(texName.replace(22, 5, "ssl").c_str());
                    case 6: return static_cast<const void*>(texName.replace(22, 5, "bbh").c_str());
                    case 7: return static_cast<const void*>(texName.replace(22, 5, "bidw").c_str());
                    case 8:
                        // "Above Clouds" recycles textures for its bottom layer
                        // See /us_pc/bin/clouds_skybox.c @ line 138
                        if (texName == "textures/skyboxes/water.44.rgba16.png" ||
                            texName == "textures/skyboxes/water.45.rgba16.png") {
                                return "textures/skyboxes/clouds.40.rgba16.png";
                            }
                        else {
                            return static_cast<const void*>(texName.replace(22, 5, "clouds").c_str());
                        }
                    case 9: return static_cast<const void*>(texName.replace(22, 5, "bits").c_str());
                }
            }
        }

        if (autoChroma) {
            // Toggle object visibility
            if (!autoChromaObjects) {
                if (texName.find("castle_grounds_textures.0BC00.ia16") != string::npos ||
                    texName.find("butterfly_wing.rgba16") != string::npos) {
                        return "textures/saturn/mario_logo.rgba16.png";
                }
            }
            // Toggle level visibility
            if (!autoChromaLevel) {
                if (texName.find("saturn") == string::npos &&
                    texName.find("dynos") == string::npos &&
                    texName.find("mario_") == string::npos &&
                    texName.find("mario/") == string::npos &&
                    texName.find("skybox") == string::npos &&
                    texName.find("shadow_quarter_circle.ia8") == string::npos &&
                    texName.find("shadow_quarter_square.ia8") == string::npos) {

                        if (texName.find("segment2.11C58.rgba16") != string::npos ||
                            texName.find("segment2.12C58.rgba16") != string::npos ||
                            texName.find("segment2.13C58.rgba16") != string::npos) {
                                return "textures/saturn/mario_logo.rgba16.png";
                        }

                }
            }
            // To-do: Hide paintings as well (low priority)
        }
    }

    return input;
}

void saturn_copy_file(string from, string to) {
    fs::path from_path(from);
    string final = "" + fs::current_path().generic_string() + "/" + to + from_path.filename().generic_string();

    fs::path final_path(final);
    // Convert TXT files to GS
    if (final_path.extension() == ".txt") {
        final = "" + fs::current_path().generic_string() + "/" + to + from_path.stem().generic_string() + ".gs";
    }

    std::cout << from << " - " << final << std::endl;

    // Skip existing files
    if (fs::exists(final))
        return;

    fs::copy(from, final, fs::copy_options::skip_existing);
}

void saturn_delete_file(string file) {
    remove(file.c_str());
}

std::size_t number_of_files_in_directory(std::filesystem::path path) {
    return (std::size_t)std::distance(std::filesystem::directory_iterator{path}, std::filesystem::directory_iterator{});
}