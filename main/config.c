
#define DEF_DBG_MODULE	DBG_MODULE_NVS

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include "esp_partition.h"


static esp_partition_t* find_partition(esp_partition_type_t type, esp_partition_subtype_t subtype, const char* name)
{
//    INFO("Find partition with type %s, subtype %s, label %s...", get_type_str(type), get_subtype_str(subtype),
//                    name == NULL ? "NULL (unspecified)" : name);

    const esp_partition_t* part  = esp_partition_find_first(type, subtype, name);

    if (!part) {
        ERROR("partition not found\n");
        return NULL;
    }

    INFO("found partition '%s' at offset 0x%x with size 0x%x\n", part->label, part->address, part->size);

    return part;
}


static bool dbgFindPart(uint8_t argc, char** argv)
{
    uint8_t type;
    uint8_t subType;
    esp_partition_t* part;

    if (argc < 3) {
        return false;
    }

    type = strtol(argv[1], NULL, 16);
    subType = strtol(argv[2], NULL, 16);

    part = find_partition(type, subType, NULL);
    if (!part) {
        PRINT("partition not found\n");
        return true;
    }

    PRINT("addr:0x%x size:0x%x spi:0x%x\n", part->address, part->size, part->flash_chip);

    return true;
}

static bool dbgStatus(uint8_t argc, char** argv)
{
    esp_partition_t* part;
    part = find_partition(0x40, 0x01, NULL);
    if (!part) {
        PRINT("config partition not found\n");
        return true;
    }

    PRINT("addr:0x%x size:0x%x spi:0x%x\n", part->address, part->size, part->flash_chip);

    //PRINT_BUF("config:", PRINT_BUF_STYLE_ASC_SIZE_NL, part->address, part->size);

    return true;
}

static bool dbgRead(uint8_t argc, char** argv)
{
    uint32_t*    addr;

    if (argc < 2) {
        return false;
    }

    addr = strtol(argv[1], NULL, 16);

    PRINT_BUF(NULL, PRINT_BUF_STYLE_HEX_SIZE_NL, addr, 4);

    return true;
}

// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("config", NULL)
		DEBUG_MENU_CMD("status",    NULL,		NULL, dbgStatus)
		DEBUG_MENU_CMD("findPart",  NULL,		NULL, dbgFindPart)
		DEBUG_MENU_CMD("rd",	    NULL,		NULL, dbgRead)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*


void CONFIG_init(void)
{
    DBG_TREE_add("/",		g_menu);
}