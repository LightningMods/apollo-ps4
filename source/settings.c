#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <stdbool.h>
#include <store_api.h>
#include <orbis/libkernel.h>
#include <orbis/SaveData.h>
#include <orbis/UserService.h>

#include "types.h"
#include "menu.h"
#include "saves.h"
#include "common.h"

#define ORBIS_USER_SERVICE_USER_ID_INVALID	-1

static char * sort_opt[] = {"Disabled", "by Name", "by Title ID", NULL};
#define SCE_SYSMODULE_INTERNAL_BGFT 0x8000002A
#define SCE_SYSMODULE_INTERNAL_APPINSTUTIL 0x80000014

menu_option_t menu_options[] = {
	{ .name = "\nBackground Music", 
		.options = NULL, 
		.type = APP_OPTION_BOOL, 
		.value = &apollo_config.music, 
		.callback = music_callback 
	},
	{ .name = "Menu Animations", 
		.options = NULL, 
		.type = APP_OPTION_BOOL, 
		.value = &apollo_config.doAni, 
		.callback = ani_callback 
	},
	{ .name = "Sort Saves",
		.options = sort_opt,
		.type = APP_OPTION_LIST,
		.value = &apollo_config.doSort,
		.callback = sort_callback
	},
	{ .name = "\nVersion Update Check", 
		.options = NULL, 
		.type = APP_OPTION_BOOL, 
		.value = &apollo_config.update, 
		.callback = update_callback 
	},
	{ .name = "Clear Local Cache", 
		.options = NULL, 
		.type = APP_OPTION_CALL, 
		.value = NULL, 
		.callback = clearcache_callback 
	},
	{ .name = "Update Application Data", 
		.options = NULL, 
		.type = APP_OPTION_CALL, 
		.value = NULL, 
		.callback = upd_appdata_callback 
	},
	{ .name = "\nEnable Debug Log",
		.options = NULL,
		.type = APP_OPTION_CALL,
		.value = NULL,
		.callback = log_callback 
	},
	{ .name = NULL }
};


void music_callback(int sel)
{
	apollo_config.music = !sel;
}

void sort_callback(int sel)
{
	apollo_config.doSort = sel;
}

void ani_callback(int sel)
{
	apollo_config.doAni = !sel;
}

void clearcache_callback(int sel)
{
	LOG("Cleaning folder '%s'...", APOLLO_LOCAL_CACHE);
	clean_directory(APOLLO_LOCAL_CACHE);

	show_message("Local cache folder cleaned:\n" APOLLO_LOCAL_CACHE);
}

void unzip_app_data(const char* zip_file)
{
	if (extract_zip(zip_file, APOLLO_DATA_PATH))
		show_message("Successfully installed local application data");

	unlink_secure(zip_file);
}

void upd_appdata_callback(int sel)
{
	if (http_download(ONLINE_URL, "PS4/ps4appdata.zip", APOLLO_LOCAL_CACHE "appdata.zip", 1))
		unzip_app_data(APOLLO_LOCAL_CACHE "appdata.zip");
}
void SIG_Handler(int sig){
	
	
}
void update_callback(int sel)
{
    apollo_config.update = !sel;
	
    sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_BGFT);
    sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_APPINSTUTIL);

    if (!apollo_config.update)
        return;
	
	    struct sigaction new_SIG_action;

   // new_SIG_action.sa_sigaction = SIG_IGN;
    new_SIG_action.sa_handler = SIG_Handler;
    sigemptyset(&new_SIG_action.sa_mask);
    new_SIG_action.sa_flags = 0;

    for (int i = 0; i < 43; i++) {
      //  if(i != SIGQUIT)
          sigaction(i, &new_SIG_action, NULL);
    }

	if (show_dialog(1, "New version available! Download update?"))
	{
		if (!sceStoreApiLaunchStore("Apollo"))
			show_message("An Store API Errror has occurred\ncheck /data/store_api.log for more info");
	}


	return;
}

void log_callback(int sel)
{
	dbglogger_init_mode(FILE_LOGGER, APOLLO_PATH "apollo.log", 0);
	show_message("Debug Logging Enabled!\n\n" APOLLO_PATH "apollo.log");
}

/*
char** get_logged_users()
{
	char buff[ORBIS_USER_SERVICE_MAX_USER_NAME_LENGTH+1];
	OrbisUserServiceLoginUserIdList userIdList;
	char** users;

	owner_sel = 0;
	users = calloc(1, sizeof(char*) * (ORBIS_USER_SERVICE_MAX_LOGIN_USERS+1));

	if (sceUserServiceGetLoginUserIdList(&userIdList) < 0)
	{
		sceUserServiceGetUserName(apollo_config.user_id, buff, sizeof(buff));
		users[0] = strdup(buff);
		return users;
	}

	for (int i = 0; i < ORBIS_USER_SERVICE_MAX_LOGIN_USERS; i++)
		if (userIdList.userId[i] != ORBIS_USER_SERVICE_USER_ID_INVALID)
		{
			sceUserServiceGetUserName(userIdList.userId[i], buff, sizeof(buff));
			users[i] = strdup(buff);
		}

	return users;
}
*/

int save_app_settings(app_config_t* config)
{
	char filePath[256];
	OrbisSaveDataMount2 mount;
	OrbisSaveDataDirName dirName;
	OrbisSaveDataMountResult mountResult;

	memset(&mount, 0x00, sizeof(mount));
	memset(&mountResult, 0x00, sizeof(mountResult));
	strlcpy(dirName.data, "Settings", sizeof(dirName.data));

	mount.userId = apollo_config.user_id;
	mount.dirName = &dirName;
	mount.blocks = ORBIS_SAVE_DATA_BLOCKS_MIN2;
	mount.mountMode = (ORBIS_SAVE_DATA_MOUNT_MODE_CREATE2 | ORBIS_SAVE_DATA_MOUNT_MODE_RDWR | ORBIS_SAVE_DATA_MOUNT_MODE_COPY_ICON);

	if (sceSaveDataMount2(&mount, &mountResult) < 0) {
		LOG("sceSaveDataMount2 ERROR");
		return 0;
	}

	LOG("Saving Settings...");
	snprintf(filePath, sizeof(filePath), APOLLO_SANDBOX_PATH "settings.bin", mountResult.mountPathName);
	write_buffer(filePath, (uint8_t*) config, sizeof(app_config_t));

	orbis_UpdateSaveParams(mountResult.mountPathName, "Apollo Save Tool", "User Settings", "www.bucanero.com.ar");
	orbis_SaveUmount(mountResult.mountPathName);

	return 1;
}

int load_app_settings(app_config_t* config)
{
	char filePath[256];
	app_config_t* file_data;
	size_t file_size;
	OrbisSaveDataMount2 mount;
	OrbisSaveDataDirName dirName;
	OrbisSaveDataMountResult mountResult;

	sceUserServiceGetNpAccountId(config->user_id, &config->account_id);
	sceKernelGetOpenPsIdForSystem(config->psid);
	config->psid[0] = ES64(config->psid[0]);
	config->psid[1] = ES64(config->psid[1]);

	if (sceSaveDataInitialize3(0) != SUCCESS)
	{
		LOG("Failed to initialize save data library");
		return 0;
	}

	memset(&mount, 0x00, sizeof(mount));
	memset(&mountResult, 0x00, sizeof(mountResult));
	strlcpy(dirName.data, "Settings", sizeof(dirName.data));

	mount.userId = apollo_config.user_id;
	mount.dirName = &dirName;
	mount.blocks = ORBIS_SAVE_DATA_BLOCKS_MIN2;
	mount.mountMode = ORBIS_SAVE_DATA_MOUNT_MODE_RDONLY;

	if (sceSaveDataMount2(&mount, &mountResult) < 0) {
		LOG("sceSaveDataMount2 ERROR");
		return 0;
	}

	LOG("Loading Settings...");
	snprintf(filePath, sizeof(filePath), APOLLO_SANDBOX_PATH "settings.bin", mountResult.mountPathName);

	if (read_buffer(filePath, (uint8_t**) &file_data, &file_size) == SUCCESS && file_size == sizeof(app_config_t))
	{
		file_data->user_id = config->user_id;
		file_data->account_id = config->account_id;
		file_data->psid[0] = config->psid[0];
		file_data->psid[1] = config->psid[1];
		memcpy(config, file_data, file_size);

		LOG("Settings loaded: UserID (%08x) AccountID (%016lX)", config->user_id, config->account_id);
		free(file_data);
	}

	orbis_SaveUmount(mountResult.mountPathName);

	return 1;
}
