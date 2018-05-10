#include <string.h>
#include <stdio.h>
#include <dirent.h>

#include <switch.h>

//This example shows how to access savedata for (official) applications/games.

Result get_save(u64 *titleID, u128 *userID)
{
    Result rc=0;
    FsSaveDataIterator iterator;
    size_t total_entries=0;
    FsSaveDataInfo info;

    rc = fsOpenSaveDataIterator(&iterator, FsSaveDataSpaceId_NandUser);//See libnx fs.h.
    if (R_FAILED(rc)) {
        printf("fsOpenSaveDataIterator() failed: 0x%x\n", rc);
        return rc;
    }

    while(1) {
        rc = fsSaveDataIteratorRead(&iterator, &info, 1, &total_entries);//See libnx fs.h.
        if (R_FAILED(rc) || total_entries==0) break;

        if (info.SaveDataType == FsSaveDataType_SaveData) {//Filter by FsSaveDataType_SaveData, however note that NandUser can have non-FsSaveDataType_SaveData.
            *titleID = info.titleID;
            *userID = info.userID;
            return 0;
        }
    }

    fsSaveDataIteratorClose(&iterator);

    if (R_SUCCEEDED(rc)) return MAKERESULT(Module_Libnx, LibnxError_NotFound);

    return rc;
}

int isDirectory(const char *path) {
   struct stat statbuf;
   if (stat(path, &statbuf) != 0)
       return 0;
   return S_ISDIR(statbuf.st_mode);
}

int dumpAll(char * path) {
    DIR* dir;
    struct dirent* ent;
    
    dir = opendir("save:/");//Open the "save:/" directory.
    if(dir==NULL)
    {
        printf("Failed to open dir.\n");
        return -1;
    }
    else
    {
        printf("Dir-listing for 'save:/':\n");
        while ((ent = readdir(dir)))
        {
            printf("d_name: %s\n", ent->d_name);

            char filenameI[0x100];
            char * filenameO = ent->d_name;
            strcpy(filenameI, path);
            strcat(filenameI, filenameO);

            if(isDirectory(filenameI)) {
                //needs to make path here
                dumpAll(filenameI);
            } else {
                FILE *fptr1, *fptr2;
                char c;
                
                // Open one file for reading
                fptr1 = fopen(filenameI, "r");
                if (fptr1 == NULL)
                {
                    printf("Cannot open file %s \n", filenameI);
                    break;
                }
             
                // Open another file for writing
                fptr2 = fopen(filenameO, "w");
                if (fptr2 == NULL)
                {
                    printf("Cannot open file %s \n", filenameO);
                    break;
                }

                printf("Trying to dump...\n");

                fseek(fptr1, 0L, SEEK_END);
                int sz = ftell(fptr1);
                printf("Save size %x?\n", sz);
                fseek(fptr1, 0L, SEEK_SET);
             
                // Read contents from file
                for(int i = 0; i < sz; i++) {
                    c = fgetc(fptr1);
                    fputc(c, fptr2);
                }
             
                printf("%s contents copied to %s\n", filenameI, filenameO);
             
                fclose(fptr1);
                fclose(fptr2);
            }
        }
        closedir(dir);
        printf("Done.\n");
        return 0;
    }
}

int main(int argc, char **argv)
{
    Result rc=0;
    int ret=0;

    DIR* dir;
    struct dirent* ent;

    FsFileSystem tmpfs;
    u128 userID=0;
    bool account_selected=0;
    u64 titleID=0;

    gfxInitDefault();
    consoleInit(NULL);

    //Get the userID for save mounting. To mount common savedata, use FS_SAVEDATA_USERID_COMMONSAVE.

    //Try to find savedata to use with get_save() first, otherwise fallback to the above hard-coded TID + the userID from accountGetActiveUser(). Note that you can use either method.
    //See the account example for getting account info for an userID.
    //See also the app_controldata example for getting info for a titleID.
    if (R_FAILED(get_save(&titleID, &userID))) {
        rc = accountInitialize();
        if (R_FAILED(rc)) {
            printf("accountInitialize() failed: 0x%x\n", rc);
        }

        if (R_SUCCEEDED(rc)) {
            rc = accountGetActiveUser(&userID, &account_selected);
            accountExit();

            if (R_FAILED(rc)) {
                printf("accountGetActiveUser() failed: 0x%x\n", rc);
            }
            else if(!account_selected) {
                printf("No user is currently selected.\n");
                rc = -1;
            }
        }
    }

    if (R_SUCCEEDED(rc)) {
        printf("Using titleID=0x%016lx userID: 0x%lx 0x%lx\n", titleID, (u64)(userID>>64), (u64)userID);
    }

    if (R_SUCCEEDED(rc)) {
        rc = fsMount_SaveData(&tmpfs, titleID, userID);//See also libnx fs.h.
        if (R_FAILED(rc)) {
            printf("fsMount_SaveData() failed: 0x%x\n", rc);
        }
    }

    //You can use any device-name. If you want multiple saves mounted at the same time, you must use different device-names for each one.
    if (R_SUCCEEDED(rc)) {
        ret = fsdevMountDevice("save", tmpfs);
        if (ret==-1) {
            printf("fsdevMountDevice() failed.\n");
            rc = ret;
        }
    }

    //At this point you can use the mounted device with standard stdio.
    //After modifying savedata, in order for the changes to take affect you must use: rc = fsdevCommitDevice("save");

    if (R_SUCCEEDED(rc)) {
        dir = opendir("save:/");//Open the "save:/" directory.
        if(dir==NULL)
        {
            printf("Failed to open dir.\n");
        }
        else
        {
            printf("Dir-listing for 'save:/':\n");
            while ((ent = readdir(dir)))
            {
                printf("d_name: %s\n", ent->d_name);
            }
            closedir(dir);
            printf("Done.\n");
        }

        //When you are done with savedata, you can use the below.
        //Any devices still mounted at app exit are automatically unmounted.
    }

    // Main loop
    while(appletMainLoop())
    {
        //Scan all the inputs. This should be done once for each frame
        hidScanInput();

        //hidKeysDown returns information about which buttons have been just pressed (and they weren't in the previous frame)
        u64 kDown = hidKeysDown(CONTROLLER_P1_AUTO);

        if (kDown & KEY_A) {
            dumpAll("save:/");
        }

        if (kDown & KEY_X) {
            char * filenameI = "save.dat";
            char * filenameO = "save:/save.dat";

            FILE *fptr1, *fptr2;
            char c;
            
            // Open one file for reading
            fptr1 = fopen(filenameI, "r");
            if (fptr1 == NULL)
            {
                printf("Cannot open file %s \n", filenameI);
                break;
            }
         
            // Open another file for writing
            fptr2 = fopen(filenameO, "w");
            if (fptr2 == NULL)
            {
                printf("Cannot open file %s \n", filenameO);
                break;
            }

            printf("Trying to inject...\n");

            fseek(fptr1, 0L, SEEK_END);
            int sz = ftell(fptr1);
            printf("Save size %x?\n", sz);
            fseek(fptr1, 0L, SEEK_SET);
         
            // Read contents from file
            for(int i = 0; i < sz; i++) {
                c = fgetc(fptr1);
                fputc(c, fptr2);
            }
         
            printf("%s contents copied to %s\n", filenameI, filenameO);
         
            fclose(fptr1);
            fclose(fptr2);

            rc = fsdevCommitDevice("save");
            if (R_SUCCEEDED(rc)) {
                printf("Done.\n");
            } else {
                printf("fsdevCommitDevice() failed\n");
            }
        }

        if (kDown & KEY_PLUS) {
            fsdevUnmountDevice("save");
            break; // break in order to return to hbmenu
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gfxWaitForVsync();
    }

    gfxExit();
    return 0;
}
