//
//  myfs-structs.h
//  myfs
//
//  Created by Oliver Waldhorst on 07.09.17.
//  Copyright © 2017 Oliver Waldhorst. All rights reserved.
//

#ifndef myfs_structs_h
#define myfs_structs_h

#define NAME_LENGTH 255
#define NUM_DIR_ENTRIES 64
#define NUM_OPEN_FILES 64

#define BLOCK_SIZE 512
#define FS_SIZE_MIB 20
#define FS_SIZE_BYTES (FS_SIZE_MIB << 20)
#define DATA_BLOCK_COUNT ((size_t)(FS_SIZE_BYTES / BLOCK_SIZE))

#define EMPTY_BLOCK 0
#define EOC_BLOCK -1

// TODO: Add structures of your file system here

struct MyFsFileInfo
{
	char name[NAME_LENGTH];
	size_t size;
	uid_t uid;
	gid_t gid;
	mode_t mode;
	time_t atime;
	time_t mtime;
	time_t ctime;
	char *data;
};

struct DiskFileInfo
{
	char name[NAME_LENGTH];
	size_t size;
	uid_t uid;
	gid_t gid;
	mode_t mode;
	time_t atime;
	time_t mtime;
	time_t ctime;
	int firstblock;
};

struct MyFsSuperBlock
{
	uint32_t fat_start;
	uint32_t root_start;
	uint32_t data_start;
};

/*
struct MyFsFAT
{
};

struct MyFsDmap
{
};
*/

struct OpenFile {
    int buffer[BLOCK_SIZE];
    int blockNo;
};

#endif /* myfs_structs_h */
