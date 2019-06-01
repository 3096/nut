#include <string.h>
#include <stdio.h>
#include <stdlib.h> 
#include <dirent.h>
#include <fstream>
#include <vector>

#include <switch.h>

#include "../ipswitch/source/console.h"

const char * EXPORT_DIR = "save/";
const char * INJECT_DIR = "inject/";
const char * SAVE_DEV = "save";

Result getSaveList(std::vector<FsSaveDataInfo> & saveInfoList) {
    Result rc=0;
    FsSaveDataIterator iterator;
    size_t total_entries=0;
    FsSaveDataInfo info;

    rc = fsOpenSaveDataIterator(&iterator, FsSaveDataSpaceId_NandUser);//See libnx fs.h.
    if (R_FAILED(rc)) {
        printf("fsOpenSaveDataIterator() failed: 0x%x\n", rc);
        return rc;
    }

    rc = fsSaveDataIteratorRead(&iterator, &info, 1, &total_entries);//See libnx fs.h.
    if (R_FAILED(rc))
        return rc;
    if (total_entries==0)
        return MAKERESULT(Module_Libnx, LibnxError_NotFound);

    for(; R_SUCCEEDED(rc) && total_entries > 0; 
        rc = fsSaveDataIteratorRead(&iterator, &info, 1, &total_entries)) {
        if (info.SaveDataType == FsSaveDataType_SaveData) {
            saveInfoList.push_back(info);
        }
    }

    fsSaveDataIteratorClose(&iterator);

    return 0;
}

Result mountSaveBySaveDataInfo(const FsSaveDataInfo & info, const char * dev) {
    Result rc=0;
    int ret=0;

    u64 titleID = info.titleID;
    u128 userID = info.userID;
    
    FsFileSystem tmpfs;

    printf("\n\nUsing titleID=0x%016lx userID: 0x%lx 0x%lx\n", titleID, (u64)(userID>>64), (u64)userID);

    rc = fsMount_SaveData(&tmpfs, titleID, userID);//See also libnx fs.h.
    if (R_FAILED(rc)) {
        printf("fsMount_SaveData() failed: 0x%x\n", rc);
        return rc;
    }

    ret = fsdevMountDevice(dev, tmpfs);
    if (ret==-1) {
        printf("fsdevMountDevice() failed.\n");
        rc = ret;
        return rc;
    }

    return rc;
}

int isDirectory(const char *path) {
   struct stat statbuf;
   if (stat(path, &statbuf) != 0)
       return 0;
   return S_ISDIR(statbuf.st_mode);
}

int cpFile(const char * filenameI, const char * filenameO) {
    remove( filenameO );
    
    std::ifstream src(filenameI, std::ios::binary);
    std::ofstream dst(filenameO, std::ios::binary);

    dst << src.rdbuf();

    return 0;
}


int copyAllSave(const char * dev, const char * path, bool isInject, 
    const char * exportDir) {
    DIR* dir;
    struct dirent* ent;
    char dirPath[0x100];
    if(isInject) {
        strcpy(dirPath, INJECT_DIR);
        strcat(dirPath, path);
    } else {                    
        strcpy(dirPath, dev);
        strcat(dirPath, path);
    }

    dir = opendir(dirPath);
    if(dir==NULL)
    {
        printf("Failed to open dir: %s\n", dirPath);
        return -1;
    }
    else
    {
        printf("Contents from %s:\n", dirPath);
        while ((ent = readdir(dir)))
        {

            char filename[0x100];
            strcpy(filename, path);
            strcat(filename, "/");
            strcat(filename, ent->d_name);

            char filenameI[0x100];
            char filenameO[0x100];
            if(isInject) {
                strcpy(filenameI, INJECT_DIR);
                strcat(filenameI, filename);

                strcpy(filenameO, dev);
                strcat(filenameO, filename);
            } else {
                strcpy(filenameI, dev);
                strcat(filenameI, filename);

                if (exportDir != NULL) {
                    strcpy(filenameO, exportDir);
                } else {
                    strcpy(filenameO, EXPORT_DIR);
                }
                strcat(filenameO, filename);
            }

            if(isDirectory(filenameI)) {
                mkdir(filenameO, 0700);
                int res = copyAllSave(dev, filename, isInject, exportDir);
                if(res != 0)
                    return res;
            } else {
                printf("Copying %s... ", filenameI);
                cpFile(filenameI, filenameO);
                if(isInject) {
                    if (R_SUCCEEDED(fsdevCommitDevice(SAVE_DEV))) { // Thx yellows8
                        printf("committed.\n");
                    } else {
                        printf("fsdevCommitDevice() failed...\n");
                        return -2;
                    }
                } else {
                    printf("\n");
                }
            }
        }
        closedir(dir);
        printf("Finished %s.\n", dirPath);
        return 0;
    }
}

int dumpAll() {
    return copyAllSave("save:/", ".", false, NULL);
}

int dumpAllTo(char * dir) {
    return copyAllSave("save:/", ".", false, dir);
}

void dumpToTitleUserDir(FsSaveDataInfo info) {
    char exportDir[0x100];
    sprintf(exportDir, "%s%016lx/", EXPORT_DIR, info.titleID);
    mkdir(exportDir, 0700);
    sprintf(exportDir, "%s%016lx/%lx%lx/", 
        EXPORT_DIR, info.titleID, (u64)(info.userID>>64), (u64)info.userID);
    mkdir(exportDir, 0700);
    dumpAllTo(exportDir);
}

int inject() {
    return copyAllSave("save:/", ".", true, NULL);
}

Result getTitleName(u64 titleID, char* name, size_t targetBufferLen) {
    Result rc=0;

    NsApplicationControlData *buf=NULL;
    size_t outsize=0;

    NacpLanguageEntry *langentry = NULL;

    buf = (NsApplicationControlData*)malloc(sizeof(NsApplicationControlData));
    if (buf==NULL) {
        rc = MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
        printf("Failed to alloc mem.\n");
    }
    else {
        memset(buf, 0, sizeof(NsApplicationControlData));
    }

    if (R_SUCCEEDED(rc)) {
        rc = nsInitialize();
        if (R_FAILED(rc)) {
            printf("nsInitialize() failed: 0x%x\n", rc);
        }
    }

    if (R_SUCCEEDED(rc)) {
        rc = nsGetApplicationControlData(1, titleID, buf, sizeof(NsApplicationControlData), &outsize);
        if (R_FAILED(rc)) {
            printf("nsGetApplicationControlData() failed: 0x%x\n", rc);
        }

        if (outsize < sizeof(buf->nacp)) {
            rc = -1;
            printf("Outsize is too small: 0x%lx.\n", outsize);
        }

        if (R_SUCCEEDED(rc)) {
            rc = nacpGetLanguageEntry(&buf->nacp, &langentry);

            if (R_FAILED(rc) || langentry==NULL) printf("Failed to load LanguageEntry.\n");
        }

        if (R_SUCCEEDED(rc)) {
            memset(name, 0, sizeof(*name));
            size_t copyLen = sizeof(langentry->name) > targetBufferLen ? targetBufferLen : sizeof(langentry->name);
            strncpy(name, langentry->name, copyLen);
        }

        nsExit();
    }

    return rc;
}

Result getUserNameById(u128 userID, char* username, size_t targetBufferLen) {
    Result rc=0;

    AccountProfile profile;
    AccountUserData userdata;
    AccountProfileBase profilebase;

    memset(&userdata, 0, sizeof(userdata));
    memset(&profilebase, 0, sizeof(profilebase));

    rc = accountInitialize();
    if (R_FAILED(rc)) {
        printf("accountInitialize() failed: 0x%x\n", rc);
    }

    if (R_SUCCEEDED(rc)) {
        rc = accountGetProfile(&profile, userID);

        if (R_FAILED(rc)) {
            printf("accountGetProfile() failed: 0x%x\n", rc);
        }
        

        if (R_SUCCEEDED(rc)) {
            rc = accountProfileGet(&profile, &userdata, &profilebase);//userdata is otional, see libnx acc.h.

            if (R_FAILED(rc)) {
                printf("accountProfileGet() failed: 0x%x\n", rc);
            }

            if (R_SUCCEEDED(rc)) {
                memset(username, 0, sizeof(*username));
                size_t copyLen = sizeof(profilebase.username) > targetBufferLen
                                     ? targetBufferLen
                                     : sizeof(profilebase.username);
                strncpy(username, profilebase.username, copyLen);
            }
            accountProfileClose(&profile);
        }
        accountExit();
    }

    return rc;
}

int selectSaveFromList(int & selection, int change,
    std::vector<FsSaveDataInfo> & saveInfoList, FsSaveDataInfo & info, bool printName) {

    selection += change;
    if (selection < 0) {
        selection = abs(selection) % saveInfoList.size();
        selection = saveInfoList.size() - selection;
    } else if (selection > 0 
        && static_cast<unsigned int>(selection) >= saveInfoList.size()) {
        selection %= saveInfoList.size();
    }

    info = saveInfoList.at(selection);
    printf("\r                                                                               ");
    consoleUpdate(NULL);
    if (printName){
        char name[0x201];
        getTitleName(info.titleID, name, sizeof(name));
        char username[0x21];
        getUserNameById(info.userID, username, sizeof(username));
        printf("\rSelected: %s \t User: %s", name, username);
    } else {
        printf("\rSelected titleID: 0x%016lx userID: 0x%lx%lx", 
            info.titleID, (u64)(info.userID>>64), (u64)info.userID);
    }

    return selection;
}

bool userConfirm(const char * msg) {
    printf("\n%s\nPress A to confirm, any other button to cancel\n", msg);

    u64 kDownPrevious = hidKeysDown(CONTROLLER_P1_AUTO); 
    while(appletMainLoop())
    {
        hidScanInput();
        u64 kDown = hidKeysDown(CONTROLLER_P1_AUTO);

        if(kDown > kDownPrevious) {
            if (kDown & KEY_A)
                return true;
            else {
                printf("Canceled\n");
                return false;
            }
        }

        kDownPrevious = kDown;
    }
    return false;
}

int main(int argc, char **argv)
{

    Result rc=0;

    consoleInit(NULL);

    std::vector<FsSaveDataInfo> saveInfoList;

    mkdir(EXPORT_DIR, 0700);
    mkdir(INJECT_DIR, 0700);    

    if (R_FAILED(getSaveList(saveInfoList))) {
        printf("Failed to get save list 0x%x\n", rc);
    }

    printf("Y'allAreNUTs v0.1.1\n"
        "Press UP and DOWN to select a save\n"
        "Press LEFT and RIGHT to skip 5 saves\n"
        "Press A to dump save to 'save/'\n"
        "Press Y to dump save to 'save/{titleID}/{userID}/'\n"
        "Press ZR to dump all of your saves\n"
        "Press X to inject contents from 'inject/'\n"
        "Press R to toggle title ID (for manual look up) if it's garbled text\n"
        "Press PLUS to quit\n\n");

    // Main loop
    int selection = 0;
    FsSaveDataInfo info;
    bool printName = true;
    selectSaveFromList(selection, 0, saveInfoList, info, printName);
    while(appletMainLoop())
    {
        //Scan all the inputs. This should be done once for each frame
        hidScanInput();

        //hidKeysDown returns information about which buttons have been just pressed (and they weren't in the previous frame)
        u64 kDown = hidKeysDown(CONTROLLER_P1_AUTO);

        if (kDown & KEY_UP) {
            selectSaveFromList(selection, -1, saveInfoList, info, printName);
        }

        if (kDown & KEY_DOWN) {
            selectSaveFromList(selection, 1, saveInfoList, info, printName);
        }

        if (kDown & KEY_LEFT) {
            selectSaveFromList(selection, -5, saveInfoList, info, printName);
        }

        if (kDown & KEY_RIGHT) {
            selectSaveFromList(selection, 5, saveInfoList, info, printName);
        }

        if (kDown & KEY_R) {
            printName = !printName;
            selectSaveFromList(selection, 0, saveInfoList, info, printName);
        }
        
        if (kDown & KEY_A) {
            mountSaveBySaveDataInfo(info, SAVE_DEV);
            dumpAll();
            fsdevUnmountDevice(SAVE_DEV);
            printf("Dump over.\n\n");
        }

        if (kDown & KEY_Y) {
            mountSaveBySaveDataInfo(info, SAVE_DEV);
            dumpToTitleUserDir(info);
            fsdevUnmountDevice(SAVE_DEV);
            printf("Dump over.\n\n");
        }

        if (kDown & KEY_ZR) {
            if (userConfirm("Dump all saves? This may take a while.")) {
                for(u32 i = 0; i < saveInfoList.size(); i++) {
                    info = saveInfoList.at(i);
                    mountSaveBySaveDataInfo(info, SAVE_DEV);
                    dumpToTitleUserDir(info);
                    fsdevUnmountDevice(SAVE_DEV);
                }
                printf("Dump over.\n\n");
            }
        }

        if (kDown & KEY_X) {
            if (userConfirm("Inject data from 'inject/'?")) {
                mountSaveBySaveDataInfo(info, SAVE_DEV);
                if( inject() == 0 ) {
                    printf("Inject over.\n\n");
                    
                }
                fsdevUnmountDevice(SAVE_DEV);
            }
        }

        if (kDown & KEY_PLUS) {
            break; // break in order to return to hbmenu
        }

        consoleUpdate(NULL);
    }

    consoleExit(NULL);
    return 0;
}
