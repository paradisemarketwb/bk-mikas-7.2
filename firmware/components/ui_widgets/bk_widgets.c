#include "bk_widgets.h"
lv_obj_t *bk_text(lv_obj_t *parent, const char *text, int x, int y, int width) {
 lv_obj_t *label=lv_label_create(parent);
 lv_label_set_text(label,text);
 lv_obj_set_width(label,width);
 lv_obj_set_pos(label,x,y);
 lv_obj_set_style_text_font(label,&bk_font_ru_20,0);
 lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);
 return label;
}
lv_obj_t *bk_gauge(lv_obj_t *parent, lv_color_t accent) {
 lv_obj_t *arc=lv_arc_create(parent);
 lv_obj_set_size(arc,414,414); lv_obj_set_pos(arc,33,33);
 lv_arc_set_rotation(arc,135); lv_arc_set_bg_angles(arc,0,270);
 lv_arc_set_range(arc,0,1000); lv_arc_set_value(arc,0);
 lv_obj_set_style_arc_width(arc,12,LV_PART_MAIN);
 lv_obj_set_style_arc_width(arc,12,LV_PART_INDICATOR);
 lv_obj_set_style_arc_color(arc,accent,LV_PART_INDICATOR);
 lv_obj_remove_style(arc,NULL,LV_PART_KNOB);
 lv_obj_remove_flag(arc,LV_OBJ_FLAG_CLICKABLE);
 return arc;
}
