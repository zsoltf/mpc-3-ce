/* Prefix of t_gatom, pinned Pd f009fd8, g_text.c:582-591.
 * Read-only metadata; all mutations dispatch the actual class method. */
#ifndef MPCLEARN_PD_CONTROL_LAYOUT_H
#define MPCLEARN_PD_CONTROL_LAYOUT_H
typedef struct {
    t_text a_text;
    int a_flavor;
    t_glist* a_glist;
    t_float a_toggle,a_draghi,a_draglo;
    t_symbol* a_label;
} PdGatomPrefix;
/* Exact private layout in pinned g_array.c:115-129. No retained pointers. */
typedef struct {
    t_gobj x_gobj;
    t_scalar* x_scalar;
    t_glist* x_glist;
    t_symbol *x_name,*x_realname;
    unsigned x_usedindsp:1,x_saveit:1,x_savesize:1,x_listviewing:1,x_hidename:1,x_edit:1;
} PdGarrayLayout;
#endif
