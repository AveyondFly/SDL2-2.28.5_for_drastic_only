#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "drastic_detour.h"
#include "drastic_pmparser.h"
#include "drastic_video.h"

#include "drastic.h"

//#include "../sdl2/src/video/mmiyoo/SDL_video_mmiyoo.h"

#define PREFIX "[DTR] "




// define prototype
void mkpath(char *p);
extern ADVANCE_DRASTIC      g_advdrastic;

static int is_hooked = 0;
static size_t page_size = 4096;
static char states_path[255] = {0};

uint64_t base_addr=0;
uint64_t base_addr_r=0;
uint64_t base_addr_rx=0;
uint64_t base_addr_rw=0;
/*
int dtr_fastforward(uint8_t v)
{
    uint32_t *ff = (uint32_t*)CODE_FAST_FORWARD;

    // 0xe3a03006
    mprotect(ALIGN_ADDR(CODE_FAST_FORWARD), page_size, PROT_READ | PROT_WRITE);
    *ff = 0xe3a03000 | v;
    return 0;
}
*/

/*
[DTR] dtr_load_state_index
[DTR] trngaje_var1=/userdata/system/drastic
[DTR] trngaje_var2=0168 - Mario Kart DS (U)(SCZ)
[DTR] trngaje_index=0
[DTR] trngaje_buf=/userdata/system/drastic/savestates/0168 - Mario Kart DS (U)(SCZ)_0.dss
*/

static int32_t dtr_load_state_index(void *system, uint32_t index, uint16_t *snapshot_top, uint16_t *snapshot_bottom, uint32_t snapshot_only)
//static int32_t dtr_load_state_index(system_struct *system, uint32_t index, uint16_t *snapshot_top, uint16_t *snapshot_bottom, uint32_t snapshot_only)
{
    char buf[2080] = {0};
    nds_load_state _func = (nds_load_state)FUN_LOAD_STATE;
  
    printf("[trngaje] system = %p\n", system - base_addr_rx);
    //printf("[trngaje] VAR_SYSTEM_GAMECARD_NAME = 0x%p\n", system + 0x8ab38 - base_addr_rx);
    //printf("[trngaje] states_path=%s\n",  states_path);
    //printf("[trngaje] user_root_path=%s\n",  system->user_root_path);
    //printf("[trngaje] gamecard_name=%s\n",  system->gamecard_name);
    
    //sprintf(buf, "%s/%s_%d.dss", states_path, VAR_SYSTEM_GAMECARD_NAME, index);
    sprintf(buf, "%s/savestates/%s_%d.dss", states_path, (char *)(system + 0x8ab38), index);
    //sprintf(buf, "%s/savestates/%s_%d.dss", (char *)(system) + 0x8a338, (char *)(system) + 0x8ab38, index);
    _func((void*)system, buf, snapshot_top, snapshot_bottom, snapshot_only);

}
#if 0
/// aarch64
void load_state_index(long param_1,undefined4 param_2,undefined8 param_3,undefined8 param_4,
                     undefined4 param_5)

{
  undefined auStack_828 [2080];
  long local_8;
  
  local_8 = ___stack_chk_guard;
  __sprintf_chk(0,auStack_828,1,0x820,"%s%csavestates%c%s_%d.dss",param_1 + 0x8a338,0x2f,0x2f,
                param_1 + 0x8ab38,param_2);
  load_state(param_1,auStack_828,param_3,param_4,param_5);
  if (local_8 == ___stack_chk_guard) {
    return;
  }
                    /* WARNING: Subroutine does not return */
  __stack_chk_fail();
}
#endif
/*
[DTR] dtr_save_state_index
[DTR] trngaje_var2=0168 - Mario Kart DS (U)(SCZ)
Saving state to /userdata/saves/drastic/savesates/_savestate_temp.dss.
Error: could not open /userdata/saves/drastic/savesates/_savestate_temp.dss.
*/


static void dtr_save_state_index(long param_1, unsigned int  param_2, unsigned long param_3, unsigned long param_4)

{
  unsigned char auStack_848 [1056];
  unsigned char auStack_428 [1056];
  long local_8;
  nds_save_state save_state = (nds_save_state)FUN_SAVE_STATE;
  
  __sprintf_chk(auStack_848,1,0x420,"%s%csavestates", states_path/*param_1 + 0x8a338*/,0x2f,0);
  __sprintf_chk(auStack_428,1,0x420,"%s_%d.dss",param_1 + 0x8ab38, param_2);
  save_state((ulong *)param_1,(char *)auStack_848,(char *)auStack_428,(void *)param_3,(void *)param_4);
}


/*
static void dtr_save_state_index(void *system, uint32_t index, uint16_t *snapshot_top, uint16_t *snapshot_bottom)
{
    char buf[255] = {0};
    nds_save_state _func1 = (nds_save_state)FUN_SAVE_STATE;
    printf(PREFIX"dtr_save_state_index\n");

    sprintf(buf, "savestates/%s_%d.dss", VAR_SYSTEM_GAMECARD_NAME, index);
    _func1((void*)VAR_SYSTEM, states_path, buf, snapshot_top, snapshot_bottom);
}
*/


// [trngaje] dtr_initialize_backup:param_5=/userdata/system/drastic/backup/1062 - Picross DS (E)(Legacy).dsv

void dtr_initialize_backup(void *param_1,int param_2,void *param_3,uint param_4,char *param_5)
{
    uint uVar1;
    int iVar2;
    unsigned char uVar3;
    FILE *__stream;
    size_t sVar4;
    long __off;
    ulong uVar5;
    void *pvVar6;
    uint uVar7;
    ulong uVar8;
    char *basedup;
    char *basen;
    char *extn;
    char sbackup[2080];
  
    printf("[trngaje] dtr_initialize_backup++\n");
    *(int *)((long)param_1 + 0x2400) = param_2;
    *(uint *)((long)param_1 + 0x2408) = param_4 - 1;
    *(void **)((long)param_1 + 0x2410) = param_3;
    *(unsigned char *)((long)param_1 + 0x2426) = 0;
    *(unsigned int *)((long)param_1 + 0x240c) = 0;
    if (param_2 != 1) {
        if (param_2 == 0) {
            *(unsigned char *)((long)param_1 + 0x2424) = 0;
            goto joined_r0x00086290;
        }
        if (param_2 != 2) goto joined_r0x00086290;
        if (param_4 < 0x10001) {
            uVar3 = 2;
            if (param_4 < 0x201) {
                uVar3 = 1;
            }
            *(unsigned char *)((long)param_1 + 0x2424) = uVar3;
            goto joined_r0x00086290;
        }
    }
    *(unsigned char *)((long)param_1 + 0x2424) = 3; // backup->address_bytes = 3; // 0x003f6000 + 100 + 0x940 + 0x2424
joined_r0x00086290:
    if (param_5 == (char *)0x0) {
        *(char *)((long)param_1 + 0x2000) = '\0';
        *(unsigned char *)((long)param_1 + 0x2423) = 0;
        return;
    }
  
#if 1
    basedup = strdup(param_5);
    basen = basename(basedup);

    if (g_advdrastic.uiUseSAVformat) {
        // 1st : .sav
        // 2nd : .dsv
        // remove .dsv from file name
        extn=strrchr(basen, '.');
        *extn=0;
        sprintf(g_advdrastic.cBackupFileName, "%s/backup/%s.sav", states_path, basen);
        printf("[trngaje] dtr_initialize_backup:1st try:cBackupFileName=%s\n", g_advdrastic.cBackupFileName);
        
        __stream = fopen(g_advdrastic.cBackupFileName,"rb");
        if (__stream == (FILE *)0x0) {
            sprintf(sbackup, "%s/backup/%s.dsv", states_path, basen);
            if (access(sbackup, F_OK) == 0) {
                    rename(sbackup, g_advdrastic.cBackupFileName);
                    printf("[trngaje] dtr_initialize_backup:2nd try:sbackup=%s\n", sbackup);
                    __stream = fopen(g_advdrastic.cBackupFileName,"rb");
            }


        }
    }
    else {
        // 1st : .dsv
        // 2nd : .sav
        sprintf(g_advdrastic.cBackupFileName, "%s/backup/%s", states_path, basen);
        printf("[trngaje] dtr_initialize_backup:1st try:cBackupFileName=%s\n", g_advdrastic.cBackupFileName);
        __stream = fopen(g_advdrastic.cBackupFileName,"rb");
        if (__stream == (FILE *)0x0) {
            extn=strrchr(basen, '.');
            *extn=0;
            sprintf(sbackup, "%s/backup/%s.sav", states_path, basen);
            if (access(sbackup, F_OK) == 0) {
                rename(sbackup, g_advdrastic.cBackupFileName);
                printf("[trngaje] dtr_initialize_backup:2nd try:sbackup=%s\n", sbackup);
                __stream = fopen(g_advdrastic.cBackupFileName,"rb");
            }
        }
    }
#else
    __stream = fopen(param_5,"rb");
#endif
  
    *(unsigned char *)((long)param_1 + 0x2427) = 0;
    if (__stream == (FILE *)0x0) {
        puts("Failed to load backup file.");
        memset(param_3,param_4,0xff);
        memset(param_1,0xff,(ulong)(param_4 >> 0xc));
    }
    else {
        sVar4 = fread(param_3,(ulong)param_4,1,__stream); // size_t fread(void *buffer, size_t size, size_t count, FILE *stream);
        printf("[trngaje] dtr_initialize_backup:param_4=0x%x\n", param_4);
        if (sVar4 != 1) {
            puts(" Failed to load entire size.");
        }
        __off = ftell(__stream);
        fseek(__stream,0, SEEK_END);
        uVar5 = ftell(__stream);    // 파일 사이즈
        fseek(__stream,__off, SEEK_SET);
        uVar8 = uVar5 & 0xffffffff;
        fclose(__stream);
        //__printf_chk(1,"Loading backup file %s, %d bytes\n",param_5,uVar5 & 0xffffffff);
        __printf_chk(1,"Loading backup file %s, %d bytes\n",g_advdrastic.cBackupFileName,uVar5 & 0xffffffff);
        uVar7 = (uint)uVar5;
        if (param_4 + 0x7a != uVar7) {
            *(uint *)((long)param_1 + 0x240c) = param_4 + 0x7a; // 풋터 포함한 전체 사이즈 인듯.
        }
        uVar1 = param_4 + 0x3fff >> 0xe;
        if (uVar7 < param_4) {
            iVar2 = uVar7 - 0x400;
            if (iVar2 < 0) {
                iVar2 = 0;
            }
            //        void *memmem(const void haystack[.haystacklen], size_t haystacklen,
            //        const void needle[.needlelen], size_t needlelen);
            pvVar6 = memmem((void *)((long)param_3 + (long)iVar2),(ulong)(uVar7 - iVar2),
                      (const void *)VAR_DESMUME_FOOTER_STR/*desmume_footer_str.10935*/,0x52);
            // pvVar4 = memmem(data + uVar3,uVar2 - uVar3, (const void *)VAR_DESMUME_FOOTER_STR, 0x52);
            if (pvVar6 != (void *)0x0) {
                uVar8 = (long)pvVar6 - (long)param_3;
                __printf_chk(1," Found DeSmuME footer at position %d. Truncating.\n",uVar8 & 0xffffffff);
            }
            uVar5 = uVar8 >> 0xe & 0x3ffff;
            __printf_chk(1," Backup file less than full size (should be %d, loaded %d).\n",(ulong)param_4,
                   uVar8 & 0xffffffff);
            memset((void *)((long)param_3 + (uVar8 & 0xffffffff)),param_4 - (int)uVar8,0xff);
            memset(param_1,0,uVar5 << 2);
            memset((void *)((long)param_1 + uVar5 * 4),0xff,(ulong)((uVar1 - (int)uVar5) * 4));
        }
        else {
            memset(param_1,0,(ulong)uVar1);
        }
    }
    //strncpy((char *)((long)param_1 + 0x2000),param_5,0x3ff);
    strncpy((char *)((long)param_1 + 0x2000),g_advdrastic.cBackupFileName,0x3ff);
    *(unsigned char *)((long)param_1 + 0x23ff) = 0;
    *(unsigned char *)((long)param_1 + 0x2423) = 0;
    return;
}

/*
DTCM moved off of main RAM, remapping main RAM to it.
Saving backup data file.
 Saving DeSmuME footer.
Turning on rumble (93903).
Stopping rumble (93951).
Turning on rumble (94316).
Stopping rumble (94364).
Turning on rumble (100169).
Stopping rumble (100217).
Turning on rumble (100309).
Stopping rumble (100355).
Turning on rumble (101400).
Stopping rumble (101453).
Turning on rumble (102656).
Stopping rumble (102704).
Turning on rumble (103517).
Stopping rumble (103566).
Turning on rumble (104703).
Stopping rumble (104750).
Turning on rumble (105633).
Stopping rumble (105683).
Turning on rumble (106069).
Stopping rumble (106115).
Turning on rumble (107092).
Stopping rumble (107141).
Turning on rumble (109099).
Stopping rumble (109161).
Saving backup data file.
SS: 800 480
[trngaje] states_path=/userdata/saves/drastic
[trngaje] user_root_path=
[trngaje] gamecard_name=
Error: savestate /userdata/system/drastic/savestates/1062 - Picross DS (E)(Legacy)_-816673588.dss does not exist
[trngaje] states_path=/userdata/saves/drastic
[trngaje] user_root_path=
[trngaje] gamecard_name=
Error: savestate /userdata/system/drastic/savestates/1062 - Picross DS (E)(Legacy)_-816673588.dss does not exist
0 mini hash hits out of 0 accesses (nan%)
0 hash accesses:
 nan% hit in one hop
 nan% hit in two hops
 nan% hit in three hops
 nan% hit in four or more hops
Saving directory config to file named /userdata/system/drastic/config/drastic.cf2
*/



void dtr_backup_save(long param_1) // 사용하지 않은 함수 , 정리 필요함
{
    if (*(char *)(param_1 + 0x2000) == '\0') {
        return;
    }


}
/*
struct backup_struct {
    u32 dirty_page_bitmap[2048]; // size 0x2000
    char file_path[1024]; // size 0x400
    enum backup_type_enum type; // size 4
    u32 access_address;
    u32 address_mask;
    u32 fix_file_size;
    
    u8 *data; // size 8
    u8 jedec_id[4];
    u32 write_frame_counter;
    
    u16 mode;
    u8 state;
    u8 status;
    
    u8 address_bytes;
    u8 state_step;
    u8 firmware;
    u8 footer_written;
};
*/

int dtr_savestate(int slot)
{
    char buf[255] = {0};
    nds_screen_copy16 _func0 = (nds_screen_copy16)FUN_SCREEN_COPY16;

    void *d0 = malloc(0x18000);
    void *d1 = malloc(0x18000);

    if ((d0 != NULL) && (d1 != NULL)) {
        _func0(d0, 0);
        _func0(d1, 1);

        if (is_hooked == 0) {
            nds_save_state_index _func1 = (nds_save_state_index)FUN_SAVE_STATE_INDEX;

            _func1((void *)VAR_SYSTEM, slot, d0, d1);
        }
        else {
            nds_save_state _func1 = (nds_save_state)FUN_SAVE_STATE;

            sprintf(buf, "savestates/%s_%d.dss", (char *)VAR_SYSTEM_GAMECARD_NAME, slot);
            _func1((ulong *)VAR_SYSTEM, states_path, buf, d0, d1);
        }
    }
    if (d0 != NULL) {
        free(d0);
    }
    if (d1 != NULL) {
        free(d1);
    }
}

int dtr_loadstate(int slot)
{
    //printf("[trngaje] VAR_SYSTEM_GAMECARD_NAME=%s\n", VAR_SYSTEM_GAMECARD_NAME);
    //return 0;

    char buf[255] = {0};

    if (is_hooked == 0) {
        nds_load_state_index _func = (nds_load_state_index)FUN_LOAD_STATE_INDEX;

        _func((void*)VAR_SYSTEM, slot, 0, 0, 0);
    }
    else {
        nds_load_state _func = (nds_load_state)FUN_LOAD_STATE;

        sprintf(buf, "%s/savestates/%s_%d.dss", states_path, VAR_SYSTEM_GAMECARD_NAME, slot);
        _func((void*)VAR_SYSTEM, buf, 0, 0, 0);
    }
}

void dtr_backup_auto_save_step(long param_1)
{
    int iVar1;
    nds_backup_save_part_0  backup_save_part_0;
    backup_save_part_0 = (nds_backup_save_part_0)FUN_BACKUP_SAVE_PART_0;
    
    //printf("[trngaje] dtr_backup_auto_save_step\n"); // 여긴 항상 실행되는 부분
    // 아래 수식 재 확인 필요함.. 안에 진입 안됨
    if ((*(int *)(param_1 + 0xb) != 0) &&
        (iVar1 = *(int *)(param_1 + 0xb) + -1,
        *(int *)(param_1 + 0xb) = iVar1, iVar1 == 0)) {
        if (*(char *)(param_1 + 0x2000) != '\0') {
            printf("[trngaje] backup_save_part_0 pre\n");
            backup_save_part_0(param_1);
            return;
        }
    }
    return;
}

// [trngaje] dtr_backup_save_part_0: __filename=/userdata/system/drastic/backup/1062 - Picross DS (E)(Legacy).dsv
//[trngaje] dtr_backup_save_part_0: backup_name=/userdata/system/test/backup/1062 - Picross DS (E)(Legacy).dsv
//Saving backup data file.

void dtr_backup_save_part_0(long param_1)
{
    char *__filename;
    int iVar1;
    uint uVar2;
    uint uVar3;
    uint uVar4;
    int iVar5;
    FILE *pFVar6;
    long lVar7;
    unsigned long local_88;
    unsigned long uStack_80;
    unsigned long local_78;
    unsigned long uStack_70;
    unsigned long local_68;
    unsigned long uStack_60;
    unsigned long local_58;
    unsigned long uStack_50;
    unsigned long local_48;
    unsigned long uStack_40;
    unsigned short local_38;
    unsigned char local_36;
    unsigned char local_35;
    unsigned char local_34;
    unsigned char local_33;
    unsigned char local_32;
    unsigned char local_31;
    unsigned char local_30;
    unsigned char local_2f;
    unsigned int local_2e;
    char local_2a;
    unsigned short local_29;
    unsigned char local_27;
    unsigned char local_26;
    unsigned char local_25;
    unsigned char local_24;
    unsigned char local_23;
    unsigned long local_22;
    unsigned long local_1a;
    unsigned int  local_12;
    long local_8;
    unsigned char desmume_footer[122];
    //char backup_name[2048];
    char *basedup;
    char *basen;
    char *extn;
    
    __filename = (char *)(param_1 + 0x2000);
    printf("[trngaje] dtr_backup_save_part_0: __filename=%s\n", __filename);
/*
    basedup = strdup(__filename);
    basen = basename(basedup);
    if (g_advdrastic.uiUseSAVformat) {
        // .sav
        // remove .dsv from file name
        extn=strrchr(basen, '.');
        *extn=0;
        sprintf(backup_name, "%s/backup/%s.sav", states_path, basen);
    }
    else {
        // .dsv
        sprintf(backup_name, "%s/backup/%s", states_path, basen);
    }
    
    
    printf("[trngaje] dtr_backup_save_part_0: backup_name=%s\n", backup_name);
*/    
    iVar1 = *(int *)(param_1 + 0x2408);
    pFVar6 = fopen(__filename,"rb+");
    //pFVar6 = fopen(backup_name,"rb+");
    if (pFVar6 == (FILE *)0x0) {
        __printf_chk(1," Couldn\'t open backup file %s. Trying to create.\n",__filename);
        //__printf_chk(1," Couldn\'t open backup file %s. Trying to create.\n",backup_name);
        pFVar6 = fopen(__filename,"wb");
        //pFVar6 = fopen(backup_name,"wb");
        fclose(pFVar6);
        pFVar6 = fopen(__filename,"rb+");
        //pFVar6 = fopen(backup_name,"rb+");
        if (pFVar6 == (FILE *)0x0) {
            __printf_chk(1,"  Failed to open %s for writing.\n",__filename);
            //__printf_chk(1,"  Failed to open %s for writing.\n", backup_name);
            goto LAB_00085914;
        }
    }
    puts("Saving backup data file.");
    //[trngaje] dtr_backup_save_part_0: backup_name=/userdata/system/r4_save/backup/젤다의 전설 몽환의 모래시계 (K).sav
    //Saving backup data file.
    //[trngaje] dtr_backup_save_part_0: iVar1=0x7ffff
    //[trngaje] dtr_backup_save_part_0: uVar4=0x20

    printf("[trngaje] dtr_backup_save_part_0: iVar1=0x%x\n", iVar1);

    uVar4 = iVar1 + 0x4000U >> 0xe;

    if (*(int *)(param_1 + 0x240c ) != 0) {
        __printf_chk(1," Fixing file size to %d bytes.\n", *(int *)(param_1 + 0x240c ));
        iVar5 = fileno(pFVar6);
        iVar5 = ftruncate(iVar5,(ulong)*(uint *)(param_1 + 0x240c));
        if (iVar5 == 0) {
            *(unsigned int *)(param_1 + 0x240c) = 0;
        }
        else {
            puts(" Truncation failed.");
            *(unsigned int *)(param_1 + 0x240c) = 0;
        }
    }
    if (uVar4 != 0) {
        printf("[trngaje] dtr_backup_save_part_0: uVar4=0x%x\n", uVar4);
        lVar7 = 0;
        do {
            uVar3 = (int)lVar7 << 0xe;
            printf("[trngaje] dtr_backup_save_part_0: uVar3=0x%x\n", uVar3);
            printf("[trngaje] dtr_backup_save_part_0: uVar2=0x%x\n", *(uint *)(param_1 + lVar7 * 4));
            for (uVar2 = *(uint *)(param_1 + lVar7 * 4); uVar2 != 0; uVar2 = uVar2 >> 1) {
                if ((uVar2 & 1) != 0) {
                    printf("[trngaje] dtr_backup_save_part_0: uVar3=0x%x\n", uVar3);
                    fseek(pFVar6,(ulong)uVar3,0);
                    fwrite((void *)(*(long *)(param_1 + 0x2410) + (ulong)uVar3),
                         0x200,1,pFVar6);
                }
                uVar3 = uVar3 + 0x200;
            }
            *(unsigned int *)(param_1 + lVar7 * 4) = 0;
            lVar7 = lVar7 + 1;
        } while ((uint)lVar7 < uVar4);
    }
    
    printf("[trngaje] dtr_backup_save_part_0: lVar7=0x%x, uVar4=0x%x\n", lVar7, uVar4);
    
    if (*(unsigned char *)(param_1 + 0x2427) == '\0') { // backup->footer_written;
        uVar4 = iVar1 + 1;

        memcpy(desmume_footer,
               "|<--Snip above here to create a raw sav by excluding this DeSmuME savedata footer:",0x52
              );
    
        desmume_footer[0x52] = (u8)uVar4;
        desmume_footer[0x53] = (u8)((uint)uVar4 >> 8);
        desmume_footer[0x54] = (u8)((uint)uVar4 >> 0x10);        
        desmume_footer[0x55] = (u8)((uint)uVar4 >> 0x18);

        desmume_footer[0x56] = desmume_footer[0x52];
        desmume_footer[0x57] = desmume_footer[0x53];
        desmume_footer[0x58] = desmume_footer[0x54];
        desmume_footer[0x59] = desmume_footer[0x55];
        
        desmume_footer[0x5a] = 0;//uVar2;
        desmume_footer[0x5b] = 0;//uVar2;
        desmume_footer[0x5c] = 0;//uVar2;
        desmume_footer[0x5d] = 0;//uVar2;

        desmume_footer[0x5e] = *(unsigned char *)(param_1 + 0x2424); //backup->address_bytes; // 0x003f6000 + 100 + 0x940 + 0x2424
        desmume_footer[0x5f] = 0;//uVar2;
        desmume_footer[0x60] = 0;//uVar2;
        desmume_footer[0x61] = 0;//uVar2;
        
        desmume_footer[0x62] = desmume_footer[0x52];
        desmume_footer[99] = desmume_footer[0x53];
        desmume_footer[100] = desmume_footer[0x54];
        desmume_footer[0x65] = desmume_footer[0x55];
        
        desmume_footer[0x66] = 0;//uVar2;
        desmume_footer[0x67] = 0;//uVar2;
        desmume_footer[0x68] = 0;//uVar2;
        desmume_footer[0x69] = 0;//uVar2;        

        desmume_footer[0x6a] = '|';
        desmume_footer[0x6b] = '-';
        desmume_footer[0x6c] = 'D';
        desmume_footer[0x6d] = 'E';
        desmume_footer[0x6e] = 'S';
        desmume_footer[0x6f] = 'M';
        desmume_footer[0x70] = 'U';
        desmume_footer[0x71] = 'M';
        desmume_footer[0x72] = 'E';
        desmume_footer[0x73] = ' ';
        desmume_footer[0x74] = 'S';
        desmume_footer[0x75] = 'A';        
        desmume_footer[0x76] = 'V';
        desmume_footer[0x77] = 'E';
        desmume_footer[0x78] = '-';
        desmume_footer[0x79] = '|';

        // for .dsv format
        fseek(pFVar6,(ulong)uVar4,0);
        //fwrite(&local_88,0x7a,1,pFVar6);
        fwrite(desmume_footer,0x7a,1,pFVar6);
        puts(" Saving DeSmuME footer.");

        *(unsigned char *)(param_1 + 0x2427) /*"_ZdlPvm"[param_1 + 6]*/ = '\x01';
    }

    fclose(pFVar6);
    
    if (g_advdrastic.uiUseSAVformat != 0) {
        // truncate footer
        //truncate(backup_name, iVar1+1);
        truncate(__filename, iVar1+1);
    }    

LAB_00085914:

    return;
}

int dtr_quit(void)
{
    nds_quit _func = (nds_quit)FUN_QUIT;

    _func((void*)VAR_SYSTEM);
}

void detour_init(size_t page, const char *path)
{
    char addr1[20],addr2[20], perm[8], offset[20], dev[10],inode[30],pathname[PATH_MAX];
    FILE* fp;
    
    page_size = page;

    is_hooked = 0;

    fp=fopen("/proc/self/maps", "r");
    if (fp != NULL) {
    	char buf[200];
    	while (fgets(buf, 200, fp) != NULL) {
    		//printf("%s", buf);
    		_pmparser_split_line(buf, addr1, addr2, perm, offset, dev, inode, pathname);
    		if (strcmp(basename(pathname), "drastic") == 0) {
    			printf("%s, %s, %s, %s, %s, %s, %s\n", addr1, addr2, perm, offset, dev, inode, pathname);
    			
    			base_addr = (uint64_t)strtol(addr1, NULL, 16);
    			if (strcmp(perm, "r-xp") == 0)
    				base_addr_rx = base_addr; // - 0xd100;
    			else if (strcmp(perm, "r--p") == 0)
    				base_addr_r = base_addr;
    			else if (strcmp(perm, "rw-p") == 0)
    				base_addr_rw = base_addr - 0x15b000;
    			printf("base_addr = 0x%lx\n", base_addr);
    		}
    	}
    	fclose(fp);
    	printf("base_addr_rx = 0x%lx\n", base_addr_rx);
    	printf("base_addr_r = 0x%lx\n", base_addr_r);
    	printf("base_addr_rw = 0x%lx\n", base_addr_rw);
    }
    
    //mkpath("/userdata/system/test1/test2/test3"); // working well
    if ((path != NULL) && (path[0] != 0)) {
        char backup_path[2048];
        
        sprintf(backup_path, "%s/backup", path);
        mkpath(backup_path);
        sprintf(backup_path, "%s/savestates", path);
        mkpath(backup_path);
        is_hooked = 1;
        strcpy(states_path, path);
        detour_hook(FUN_LOAD_STATE_INDEX, (intptr_t)dtr_load_state_index);
        detour_hook(FUN_SAVE_STATE_INDEX, (intptr_t)dtr_save_state_index);
        detour_hook(FUN_INITIALIZE_BACKUP, (intptr_t)dtr_initialize_backup);
        //detour_hook(FUN_BACKUP_SAVE, (intptr_t)dtr_backup_save);
        //detour_hook(FUN_BACKUP_AUTO_SAVE_STEP, (intptr_t)dtr_backup_auto_save_step);
        detour_hook(FUN_BACKUP_SAVE_PART_0, (intptr_t)dtr_backup_save_part_0);
        
        printf(PREFIX"Enabled savestate hooking\n");
    }

    //detour_hook(FUN_BACKUP_SAVE, (intptr_t)dtr_backup_save); 이거 함수 수정할 것
}

void detour_hook(uint64_t old_func, uint64_t new_func)
{
    int i;
    
    volatile uint8_t *base = (uint8_t *)(intptr_t)(/*base_addr_rx + */old_func);
    printf(PREFIX"detour_hook++: old_func=0x%lx\n", base);
    printf(PREFIX"detour_hook++: new_func=0x%lx\n", new_func);
    mprotect(ALIGN_ADDR(base), page_size, PROT_READ | PROT_WRITE | PROT_EXEC);

    for(i=0; i<16; i++) {
    	printf("0x%x,", base[i]);
    }
    
/*

// for armhf
    base[0] = 0x04;
    base[1] = 0xf0;
    base[2] = 0x1f;
    base[3] = 0xe5;
    base[4] = new_func >> 0;
    base[5] = new_func >> 8;
    base[6] = new_func >> 16;
    base[7] = new_func >> 24;
	*/
	
	//for aarch64
	/*
    400078: 58000049 ldr x9, 400080 <_start+0x8>
    40007c: d61f0120 br x9
    400080: 55667788 .word 0x55667788
    400084: 11223344 .word 0x11223344
  	*/
  	/*
    base[0] = 0x70;
    base[1] = 0x00;
    base[2] = 0x00;
    base[3] = 0x10;
    base[4] = 0x11;
    base[5] = 0x02;
    base[6] = 0x40;
    base[7] = 0xf9;
    base[8] = 0x20;
    base[9] = 0x02;
    base[10] = 0x1f;
    base[11] = 0xd6;

 */
/*
    memcpy(base, "\xe1\x03\xbe\xa9\x40\x00\x00\x58\x00\x00\x1f\xd6\x00\x00\x00\x00"
		"\x00\x00\x00\x00\xe1\x03\xc2\xa8", 24);
    //memcpy(base + 12, new_func, 8);
    base[12] = new_func >> 0;
    base[13] = new_func >> 8;
    base[14] = new_func >> 16;
    base[15] = new_func >> 24;
    base[16] = new_func >> 32;
    base[17] = new_func >> 40;
    base[18] = new_func >> 48;
    base[19] = new_func >> 56;  
    */
    //base[0] = 0x40; // x0 : 1st parameter of function is corrupted
    base[0] = 0x51; // x17
    //base[0] = 0x47;
    //base[0] = 0x48; // x8
    //base[0] = 0x49; // x9
    base[1] = 0x00;
    base[2] = 0x00;
    base[3] = 0x58;
    //base[4] = 0x00; // x0
    //base[5] = 0x00; // x0
    
    base[4] = 0x20; // x17
    base[5] = 0x02; // x17
         
    base[6] = 0x1f;
    base[7] = 0xd6;
    base[8] = new_func >> 0;
    base[9] = new_func >> 8;
    base[10] = new_func >> 16;
    base[11] = new_func >> 24;
    base[12] = new_func >> 32;
    base[13] = new_func >> 40;
    base[14] = new_func >> 48;
    base[15] = new_func >> 56;
    printf(PREFIX"detour_hook--\n");
}

void detour_quit(void)
{
}

// *****************************************************************
// add util functions
// *****************************************************************

void mkpath(char *p) {
    char *path = strdup(p);
    char  *save_path = path;
    char *sep1;
    char *sep2=0;
    do {
        int idx = (sep2-path)<0 ? 0 : sep2-path;
        sep1 = strchr(path + idx , '/');    
        sep2 = strchr(sep1+1, '/');
        if (sep2) {
            path[sep2-path]=0;
        }
        if(mkdir(path, 0777) && errno != EEXIST)
            break;
        if (sep2) {
            path[sep2-path]='/';
        }
    } while (sep2);

    free(save_path);

}

