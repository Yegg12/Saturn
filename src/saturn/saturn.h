#ifndef Saturn
#define Saturn

#include <stdio.h>
#include <PR/ultratypes.h>
#include <stdbool.h>
#include <mario_animation_ids.h>
#include <SDL2/SDL.h>
#include "types.h"

#ifdef __cplusplus
#include <string>
#include <vector>
#include <map>
#endif

extern bool mario_exists;

extern struct camera_t {
    bool frozen;
    float speed;
    float focus;
    float savestate_mult;
    bool fov_smooth;
    bool is_moving;

    bool view_enabled;
    bool view_moving;
    bool view_zooming;
    bool view_rotating;
    int view_move[2];
} camera;

extern int current_eye_state;

extern bool enable_head_rotations;
extern bool enable_shadows;
extern bool enable_dust_particles;
extern bool enable_torso_rotation;
extern float run_speed;
extern bool can_fall_asleep;
extern int saturnModelState;
extern bool linkMarioScale;
extern bool is_spinning;
extern float spin_mult;

extern bool is_custom_anim;
extern bool using_chainer;
extern int chainer_index;
extern bool is_anim_playing;
extern enum MarioAnimID selected_animation;
extern bool is_anim_looped;
extern bool is_anim_hang;
extern float anim_speed;
extern int current_anim_frame;
extern int current_anim_id;
extern int current_anim_length;
extern bool is_anim_paused;

extern float this_face_angle;

extern bool limit_fps;
extern bool has_discord_init;

extern u16 gChromaKeyColor;
extern u16 gChromaKeyBackground;

extern int mcam_timer;

extern SDL_Scancode saturn_key_to_scancode(unsigned int key[]);

extern bool autoChroma;
extern bool autoChromaLevel;
extern bool autoChromaObjects;

extern bool should_update_cam_from_keyframes;

extern u8 activatedToads;

#ifdef __cplusplus
enum InterpolationCurve {
    LINEAR,
    SINE,
    QUADRATIC,
    CUBIC,
    WAIT
};
enum KeyframeType {
    KFTYPE_FLOAT,
    KFTYPE_FLAGS,
    KFTYPE_BOOL
};

inline std::string curveNames[] = {
    "Linear",
    "Sine",
    "Quadratic",
    "Cubic",
    "Wait"
};

class Keyframe {
    public:
        float value;
        InterpolationCurve curve;
        int position;
        std::string timelineID;

        Keyframe(float value, InterpolationCurve curve, int position, std::string timelineID)
            : value(value), curve(curve), position(position), timelineID(timelineID)
        {}
};

class KeyframeTimeline {
    public:
        void* dest = nullptr;
        KeyframeType type;
        std::string name;
        int precision;
        bool forceWait;

        KeyframeTimeline(void* dest, KeyframeType type, std::string name, int precision, bool forceWait)
            : dest(dest), type(type), name(name), precision(precision), forceWait(forceWait)
        {}

        KeyframeTimeline(void* dest, std::string name, int precision, bool forceWait)
            : dest(dest), name(name), precision(precision), forceWait(forceWait)
        {}
};

extern bool k_popout_open;
extern float* active_key_float_value;
extern bool* active_key_bool_value;
extern s32 active_data_type;
extern int k_current_frame;
extern int k_previous_frame;
extern int k_curr_curve_type;
extern bool keyframe_playing;
extern int k_current_anim;
extern int k_prev_anim;

extern bool place_keyframe_anim;

extern std::map<std::string, std::pair<KeyframeTimeline, std::vector<Keyframe>>> k_frame_keys;

extern int k_last_passed_index;
extern int k_distance_between;
extern int k_current_distance;
extern float k_static_increase_value;
extern int k_last_placed_frame;
extern bool k_loop;
extern bool k_animating_camera;

extern std::string model_details;
extern std::string cc_details;
extern bool is_cc_editing;

extern int autosaveDelay;

extern Vec3f stored_mario_pos;
extern Vec3s stored_mario_angle;
extern void saturn_copy_camera(bool);
extern void saturn_paste_camera(void);
extern bool saturn_keyframe_apply(std::string, int);
extern bool saturn_keyframe_matches(std::string, int);

extern double saturn_expression_encode();
extern void saturn_expression_decode(double);

extern "C" {
#endif
    void saturn_update(void);
    void saturn_play_animation(MarioAnimID);
    void saturn_play_keyframe();
    void saturn_print(const char*);
    const char* saturn_get_stage_name(int);
    void saturn_do_load();
    void saturn_on_splash_finish();
    s32 saturn_should_show_splash();
    void saturn_onkeydown(int);
#ifdef __cplusplus
}
#endif

#endif
