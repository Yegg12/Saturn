// 0x0E000F30
const GeoLayout castle_geo_000F30[] = {
   GEO_NODE_START(),
   GEO_OPEN_NODE(),
      GEO_LEVEL_DISPLAY_LIST(LAYER_OPAQUE, inside_castle_seg7_dl_07028FD0),
      GEO_LEVEL_DISPLAY_LIST(LAYER_ALPHA, inside_castle_seg7_dl_07029578),
      GEO_LEVEL_DISPLAY_LIST(LAYER_OPAQUE, inside_castle_seg7_dl_0702A650),
      GEO_LEVEL_DISPLAY_LIST(LAYER_TRANSPARENT_DECAL, inside_castle_seg7_dl_0702AA10),
      GEO_LEVEL_DISPLAY_LIST(LAYER_ALPHA, inside_castle_seg7_dl_0702AB20),
      GEO_LEVEL_DISPLAY_LIST(LAYER_OPAQUE, inside_castle_seg7_dl_0702E408),
      GEO_LEVEL_DISPLAY_LIST(LAYER_OPAQUE, inside_castle_seg7_dl_0702FD30),
      GEO_LEVEL_DISPLAY_LIST(LAYER_OPAQUE, inside_castle_seg7_dl_07023DB0),
      GEO_LEVEL_DISPLAY_LIST(LAYER_OPAQUE, inside_castle_seg7_dl_07031588),
      GEO_LEVEL_DISPLAY_LIST(LAYER_ALPHA, inside_castle_seg7_dl_07031720),
      GEO_LEVEL_DISPLAY_LIST(LAYER_ALPHA, inside_castle_seg7_dl_07031830),
      GEO_LEVEL_DISPLAY_LIST(LAYER_OPAQUE, inside_castle_seg7_dl_07032FC0),
      GEO_LEVEL_DISPLAY_LIST(LAYER_ALPHA, inside_castle_seg7_dl_07033158),
      GEO_LEVEL_DISPLAY_LIST(LAYER_OPAQUE, inside_castle_seg7_dl_07034D88),
      GEO_LEVEL_DISPLAY_LIST(LAYER_ALPHA, inside_castle_seg7_dl_07035178),
      GEO_LEVEL_DISPLAY_LIST(LAYER_ALPHA, inside_castle_seg7_dl_07035288),
      GEO_LEVEL_DISPLAY_LIST(LAYER_OPAQUE, inside_castle_seg7_dl_07036D88),
      GEO_LEVEL_DISPLAY_LIST(LAYER_OPAQUE, inside_castle_seg7_dl_07037988),
      GEO_LEVEL_DISPLAY_LIST(LAYER_OPAQUE, inside_castle_seg7_dl_07037BF8),
      GEO_LEVEL_DISPLAY_LIST(LAYER_TRANSPARENT, inside_castle_seg7_dl_07037DE8),
      GEO_LEVEL_DISPLAY_LIST(LAYER_TRANSPARENT, dl_castle_aquarium_light),
      GEO_LEVEL_DISPLAY_LIST(LAYER_ALPHA, inside_castle_seg7_dl_07038350),
      GEO_LEVEL_DISPLAY_LIST(LAYER_OPAQUE, inside_castle_seg7_dl_0703A6C8),
      GEO_LEVEL_DISPLAY_LIST(LAYER_ALPHA, inside_castle_seg7_dl_0703A808),
      GEO_LEVEL_DISPLAY_LIST(LAYER_OPAQUE, inside_castle_seg7_dl_070234C0),
      GEO_LEVEL_DISPLAY_LIST(LAYER_OPAQUE, inside_castle_seg7_dl_07023520),
      GEO_LEVEL_DISPLAY_LIST(LAYER_OPAQUE, inside_castle_seg7_dl_0703BA08),
      GEO_ASM(  0, geo_painting_update),
      GEO_ASM(PAINTING_ID(0, 1), geo_painting_draw),
      GEO_ASM(PAINTING_ID(1, 1), geo_painting_draw),
      GEO_ASM(PAINTING_ID(2, 1), geo_painting_draw),
      GEO_ASM(PAINTING_ID(3, 1), geo_painting_draw),
      GEO_ASM(0, geo_exec_inside_castle_light),
   GEO_CLOSE_NODE(),
   GEO_RETURN(),
};

// 0x0E001400
const GeoLayout castle_geo_001400[] = {
   GEO_NODE_SCREEN_AREA(10, SCREEN_WIDTH/2, SCREEN_HEIGHT/2, SCREEN_WIDTH/2, SCREEN_HEIGHT/2),
   GEO_OPEN_NODE(),
      GEO_ZBUFFER(0),
      GEO_OPEN_NODE(),
         GEO_NODE_ORTHO(100),
         GEO_OPEN_NODE(),
            GEO_BACKGROUND_COLOR(0x0001),
         GEO_CLOSE_NODE(),
      GEO_CLOSE_NODE(),
      GEO_ZBUFFER(1),
      GEO_OPEN_NODE(),
         GEO_CAMERA_FRUSTUM_WITH_FUNC(64, 50, 7000, geo_camera_fov),
         GEO_OPEN_NODE(),
            GEO_CAMERA(13, 0, 2000, 6000, 0, 0, 0, geo_camera_main),
            GEO_OPEN_NODE(),
               GEO_BRANCH(1, castle_geo_000F30),
               GEO_RENDER_OBJ(),
               GEO_ASM(0, geo_envfx_main),
            GEO_CLOSE_NODE(),
         GEO_CLOSE_NODE(),
      GEO_CLOSE_NODE(),
   GEO_CLOSE_NODE(),
   GEO_END(),
};
