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
#define BLOCK_SIZE 512
#define NUM_DIR_ENTRIES 64
#define NUM_OPEN_FILES 64

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

struct MyFsSuperBlock
{
	uint64_t fs_size;
	uint64_t fat_start_block;
	uint64_t dmap_start_block;
	uint64_t root_start_block;
	uint64_t data_start_block;
	uint64_t block_count;
};

struct MyFsFAT
{
};

struct MyFsDmap
{
};

#endif /* myfs_structs_h */
