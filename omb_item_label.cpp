#include <windows.h>
#include <stdio.h>
#include <string>
#include <stdint.h>
#include <vector>
#include <psapi.h>
#include <unordered_map>

#include "mewjector.h" 

#define modName "OMB-ItemLabel"

static MewjectorAPI mj;

struct MSVC_String {
    union {
        char sso_buffer[16];
        char* heap_ptr;
    } data;
    size_t size;
    size_t capacity;
};

std::unordered_map<std::string, int32_t> ItemFrameCache;
void** game_GlobalItemDB = nullptr;


typedef void* (*fnDictionaryCheck)(void* dictionaryObject, void* key);
fnDictionaryCheck game_DictionaryCheck = nullptr;

typedef void** (*fnFindAnimNode)(void* mapObj, void* outIterator, MSVC_String* searchString);
fnFindAnimNode game_FindAnimNode = nullptr;

typedef uint64_t (*fnWeaponVisual)(void* catData);
fnWeaponVisual original_WeaponVisual = nullptr;

typedef MSVC_String* (*fnItemIconSetter)(void* Dictionary, MSVC_String* arg2, void** arg3);
fnItemIconSetter original_ItemIconSetter = nullptr;

typedef void* (*fnCatPartsHelper)(void* catParts);
fnCatPartsHelper original_Helper1 = nullptr;

// UTIL

int32_t GetFrameFromLabel(void* movieClip, MSVC_String* labelName) {
    if (!movieClip || !game_FindAnimNode) return -1; 

    void* animData = *(void**)((char*)movieClip + 0xD0);
    if (!animData) return -1;

    void* mapObj = (char*)animData + 0x20;
    void* iteratorResult = nullptr;

    void** resultNode = game_FindAnimNode(mapObj, &iteratorResult, labelName);

    if (resultNode != nullptr && *resultNode != nullptr) {
        void* mapEnd = *(void**)((char*)animData + 0x28);
        if (*resultNode != mapEnd) {
            int32_t frameNumber = *(int32_t*)((char*)(*resultNode) + 0x30);
            return frameNumber + 1;
        }
    }
    return -1;
}

void* GetDictNode(void* dictionaryObject, const char* keyName) {
    if (!dictionaryObject || !game_DictionaryCheck) return nullptr;

    MSVC_String key = {0};
    size_t len = strlen(keyName);
    key.size = len;
    
    if (len > 15) return nullptr; 

    strcpy_s(key.data.sso_buffer, sizeof(key.data.sso_buffer), keyName);
    key.capacity = 15;

    return game_DictionaryCheck(dictionaryObject, &key);
}

// i honestly have NO IDEA how this works, i copied it
uintptr_t FindPattern(const char* moduleName, const char* pattern) {
    HMODULE hModule = GetModuleHandle(moduleName);
    if (!hModule) return 0;

    MODULEINFO moduleInfo;
    GetModuleInformation(GetCurrentProcess(), hModule, &moduleInfo, sizeof(MODULEINFO));

    uint8_t* base = reinterpret_cast<uint8_t*>(moduleInfo.lpBaseOfDll);
    DWORD size = moduleInfo.SizeOfImage;

    std::vector<int> patternBytes;
    const char* start = pattern;
    const char* end = pattern + strlen(pattern);
    for (const char* current = start; current < end; ++current) {
        if (*current == '?') {
            patternBytes.push_back(-1); 
        } else if (isxdigit(*current)) {
            patternBytes.push_back(strtol(current, nullptr, 16));
            while (isxdigit(*current)) current++; 
        }
    }

    for (DWORD i = 0; i < size - patternBytes.size(); ++i) {
        bool found = true;
        for (size_t j = 0; j < patternBytes.size(); ++j) {
            if (patternBytes[j] != -1 && base[i + j] != patternBytes[j]) {
                found = false;
                break;
            }
        }
        if (found) return reinterpret_cast<uintptr_t>(&base[i]);
    }
    return 0; 
}

void setPartFrame(void* catParts, uint32_t offset, int32_t frame){
    *(uint32_t*)((char*)catParts + offset) = frame;
}

// HOOKS

void* hkCatPartsHelper1(void* catParts) {
    void* catData = *(void**)((char*) catParts + 0x8A8);
    if (!catData) return original_Helper1(catParts);

    // mj.Log(modName, "Cat exists");

    // HAT NECK FACE WEAPON TRINKET (Remove weapon when adding the weapon_source thingy)
    uint32_t slotOffsets[] = { 0x9B0, 0xA70, 0xA10, 0xAD0, 0xB30 };
    uint32_t partsOffsets[] = { 0xA18, 0xA30, 0xA48, 0xA60, 0xA78 };
    uint32_t FramesOffsets[] = { 0x660, 0x6B4, 0x708, 0x75c, 0x7B0 };

    for (int i = 0; i < 5; i++) {
        char* slotStart = (char*)catData + slotOffsets[i];
        
        MSVC_String* itemName = (MSVC_String*)(slotStart + 0x8);

        if (itemName->size > 0) {
            // mj.Log(modName, "item is... size?");
            char* text1 = (itemName->capacity > 15) ? itemName->data.heap_ptr : itemName->data.sso_buffer;
            
            std::string itemString(text1);
            auto cacheIt = ItemFrameCache.find(itemString);

            // CACHE HIT: Apply frame instantly
            if (cacheIt != ItemFrameCache.end()) {
                // mj.Log(modName, "found and changed");
                setPartFrame(catParts, FramesOffsets[i], cacheIt->second);
                continue;
            } 
            
            // CACHE MISS: Do the heavy lifting
            if (game_GlobalItemDB && *game_GlobalItemDB) {
                // mj.Log(modName, "Database exists");
                void* itemsDatabaseNode = (char*)(*game_GlobalItemDB) + 0x5B8;

                
                void* itemDict = game_DictionaryCheck(itemsDatabaseNode, itemName);
                if (!itemDict) continue;

                void* clipNode = GetDictNode(itemDict, "alt_clip");
                if (!clipNode) continue;

                int32_t typeFlag = *(uint32_t*)((char*)clipNode + 0xa8);

                if ((((typeFlag - 1) & 0xfffffffa) == 0) && (typeFlag != 6)) {

                    // mj.Log(modName, "honestly i kinad forgot where this is");
                    
                    void* masterList = (char*)catParts + partsOffsets[i];
                    uint32_t count = *(uint32_t*)((char*)masterList + 0xC);
                    
                    if (count > 0) {
                        char* arrayBase = *(char**)((char*)masterList + 0x10);
                        if (arrayBase && (uintptr_t)arrayBase > 0x1000) {
                            
                            void* realMovieClip = *(void**)arrayBase;
                            int32_t realFrame = GetFrameFromLabel(realMovieClip, itemName);
                            
                            if (realFrame != -1) {
                                // mj.Log(modName, "CACHED new frame %d for item: %s", realFrame, text1);
                                ItemFrameCache[itemString] = realFrame;
                                setPartFrame(catParts, FramesOffsets[i], realFrame);
                            }
                        }
                    }
                }
            }
        }
    }
    return original_Helper1(catParts);
}

MSVC_String* hkItemIconSetter(void* Dictionary, MSVC_String* arg2, void** arg3) {
    bool shouldOverwrite = false;
    char customIconName[16] = {0};

    void* rootDict = (char*)Dictionary + 0x5b8;
    void* itemNode = game_DictionaryCheck(rootDict, arg3);

    if (itemNode != nullptr) {
        MSVC_String keyStr = {0};
        strcpy(keyStr.data.sso_buffer, "alt_clip");
        keyStr.size = 8;
        keyStr.capacity = 15;

        void* altClipNode = game_DictionaryCheck(itemNode, &keyStr);

        if (altClipNode != nullptr) {
            int32_t typeFlag = *(uint32_t*)((char*)altClipNode + 0xa8);
            
            if ((((typeFlag - 1) & 0xfffffffa) == 0) && (typeFlag != 6)) {
                
                MSVC_String* valStr = (MSVC_String*)((char*)altClipNode + 0x68);
                char* text = (valStr->capacity > 15) ? valStr->data.heap_ptr : valStr->data.sso_buffer;
                
                // Idk if i can actually just leave a heap allocated string
                strncpy(customIconName, text, 15);
                customIconName[15] = '\0'; 
                shouldOverwrite = true;
            }
        }
    }

    MSVC_String* result = original_ItemIconSetter(Dictionary, arg2, arg3);

    if (shouldOverwrite) {
        char* outText = (result->capacity > 15) ? result->data.heap_ptr : result->data.sso_buffer;
        
        strcpy(outText, customIconName);
        result->size = strlen(customIconName);
        
        // mj.Log(modName, "Icon string overwritten with: %s", customIconName);
    }

    return result;
}

uint64_t hkWeaponVisual(void* catData) {
    return original_WeaponVisual(catData);
}

// Setting up shit

#define DICT_CHECK_KEY "48 83 EC 30 48 8B F1 48 8B DA 48 8D 0D ? ? ? ? 48 3B CA"
#define DICT_CHECK_OFFSET 0xB

#define FIND_ANIM_NODE_KEY "48 83 EC 40 4D 8B F0 48 8B F2 48 8B E9 49 8B 50 10"
#define FIND_ANIM_NODE_OFFSET 0x1A

#define ICON_SETTER_KEY "C7 45 0F 6E 6F 6E 65 C6 45 13 00 8B 90 A8 00 00 00"
#define ICON_SETTER_OFFSET 0x7D

#define HELPER_KEY "41 B8 75 72 00 00 41 BF 65 00 00 00 41 BC 74 74 00 00 49 BB FF FF FF FF FF FF FF 7F"
#define HELPER_OFFSET 0x3D

#define ITEM_LOOP_KEY "48 BF A3 8B 2E BA E8 A2 8B 2E 48 8B 0D ? ? ? ? 48 81 C1 78 08 00 00"
#define TEST_WEAPON_ADRESS 0x1400c34b0

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        
        if (!MJ_Resolve(&mj)) return TRUE; 
        mj.Log("OMB-ItemLabel", "--- MOD BOOT ---");

        uintptr_t moduleBase = (uintptr_t)GetModuleHandle(NULL);

        // DictionaryCheck
        uintptr_t dictPattern = FindPattern(NULL, DICT_CHECK_KEY);
        if (dictPattern) {
            game_DictionaryCheck = reinterpret_cast<fnDictionaryCheck>(dictPattern - DICT_CHECK_OFFSET);
            mj.Log("OMB-ItemLabel", "SUCCESS: DictionaryCheck resolved.");
        }

        // FindAnimNode
        uintptr_t animFinderPattern = FindPattern(NULL, FIND_ANIM_NODE_KEY);
        if (animFinderPattern) {
            game_FindAnimNode = (fnFindAnimNode)(animFinderPattern - FIND_ANIM_NODE_OFFSET);
            mj.Log("OMB-ItemLabel", "SUCCESS: game_FindAnimNode resolved.");
        }

        // Global Database
        uintptr_t itemLoopPattern = FindPattern(NULL, ITEM_LOOP_KEY);
        if (itemLoopPattern) {
            uintptr_t movInstruction = itemLoopPattern + 10; 
            int32_t relativeOffset = *(int32_t*)(movInstruction + 3); 
            game_GlobalItemDB = (void**)(movInstruction + 7 + relativeOffset);
            mj.Log("OMB-ItemLabel", "SUCCESS: GlobalItemDB resolved.");
        }

        void* tramp = nullptr;

        // Icon Setter Hook
        uintptr_t MidDeal = FindPattern(NULL, ICON_SETTER_KEY);
        if (MidDeal) {
            uintptr_t rvaDeal = (MidDeal - ICON_SETTER_OFFSET) - moduleBase;
            if (mj.InstallHook(rvaDeal, 0, (void*)hkItemIconSetter, &tramp, 50, "OMB-ItemLabel")) {
                original_ItemIconSetter = (fnItemIconSetter)tramp;
                mj.Log("OMB-ItemLabel", "SUCCESS: Icon Setter Hooked.");
            }
        }

        // Cat Parts Helper Hook
        uintptr_t HelperDeal = FindPattern(NULL, HELPER_KEY);
        if (HelperDeal) {
            
            uintptr_t rvaHelper = (HelperDeal - HELPER_OFFSET) - moduleBase;
            
            if (mj.InstallHook(rvaHelper, 0, (void*)hkCatPartsHelper1, &tramp, 50, "OMB-ItemLabel")) {
                original_Helper1 = (fnCatPartsHelper)tramp;
                mj.Log("OMB-ItemLabel", "SUCCESS: Cat Parts Helper Hooked.");
            }
        } else {
            mj.Log("OMB-ItemLabel", "ERROR: Could not find CatPartsHelper pattern!");
        }

        /* Passive Weapon Hook (When added i guess)
        uintptr_t WholeDeal = TEST_WEAPON_ADRESS - moduleBase;
        if (mj.InstallHook(WholeDeal, 0, (void*)hkWeaponVisual, &tramp, 50, "OMB-ItemLabel")) {
            original_WeaponVisual = (fnWeaponVisual)tramp;
            mj.Log("OMB-ItemLabel", "SUCCESS: Weapon Hooked.");
        }
        */
    }
    return true;
}