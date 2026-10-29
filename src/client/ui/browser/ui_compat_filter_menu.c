#include "../module/ui_functions.h"

#include <string.h>

/* NOT_FROM_ORIGINAL_SOURCE_STORAGE_FILE: the retail filter popup exposes six
 * Yes/No rows for the server properties that are naturally tri-state. The
 * recovered client converts those parsed rows to selectors in the persistent
 * menu pool, so "All" can be selected without editing the proprietary
 * ui_mp/filter.menu asset (which would also change its pak checksum). */
static const char *const uiCompatFilterCvars[] = {
    "ui_browserShowEmpty",
    "ui_browserShowFull",
    "ui_browserShowPassword",
    "ui_browserShowNoPassword",
    "ui_browserShowPure",
    "ui_browserShowDedicated"
};

enum {
    UI_COMPAT_FILTER_VALUE_ALL = -1,
    UI_COMPAT_FILTER_VALUE_NO = 0,
    UI_COMPAT_FILTER_VALUE_YES = 1,
    UI_COMPAT_FILTER_VALUE_COUNT = 3
};

/* NOT_FROM_ORIGINAL_SOURCE: installs the All / No / Yes list into one parsed
 * Yes/No row. The arrays live in the parsed-menu memory pool. */
static void ui_compat_convert_filter_item(itemDef_t *item)
{
    multiDef_t *const multi = UI_Alloc(sizeof(*multi));

    memset(multi, 0, sizeof(*multi));
    multi->strDef = 0;
    multi->count = UI_COMPAT_FILTER_VALUE_COUNT;
    multi->cvarList[0] = String_Alloc("@MENU_ALL");
    multi->cvarValue[0] = (float)UI_COMPAT_FILTER_VALUE_ALL;
    multi->cvarList[1] = String_Alloc("@MENU_NO");
    multi->cvarValue[1] = (float)UI_COMPAT_FILTER_VALUE_NO;
    multi->cvarList[2] = String_Alloc("@MENU_YES");
    multi->cvarValue[2] = (float)UI_COMPAT_FILTER_VALUE_YES;

    item->type = ITEM_TYPE_MULTI;
    item->typeValidated = ITEM_TYPE_MULTI;
    item->typeData = multi;
}

/* NOT_FROM_ORIGINAL_SOURCE: gives every tri-state server filter an explicit
 * "All" choice while leaving the stock menu asset untouched. */
void ui_compat_extend_filter_menu(void)
{
    menuDef_t *const menu = Menus_FindByName("filter_popmenu");
    size_t cvarIndex;

    if (menu == NULL)
        return;

    for (cvarIndex = 0;
         cvarIndex < sizeof(uiCompatFilterCvars) /
                         sizeof(uiCompatFilterCvars[0]); ++cvarIndex) {
        const char *const cvar = uiCompatFilterCvars[cvarIndex];
        int32_t itemIndex;

        for (itemIndex = 0; itemIndex < menu->itemCount; ++itemIndex) {
            itemDef_t *const item = menu->items[itemIndex];

            if (item == NULL || item->cvar == NULL ||
                item->type != ITEM_TYPE_YESNO ||
                Q_stricmp(item->cvar, cvar) != 0) {
                continue;
            }
            ui_compat_convert_filter_item(item);
            break;
        }
    }
}
