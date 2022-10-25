//
// Created by Oliver Waldhorst on 20.03.20.
//  Copyright © 2017-2020 Oliver Waldhorst. All rights reserved.
//

#include "myinmemoryfs.h"

// The functions fuseGettattr(), fuseRead(), and fuseReadDir() are taken from
// an example by Mohammed Q. Hussain. Here are original copyrights & licence:

/**
 * Simple & Stupid Filesystem.
 *
 * Mohammed Q. Hussain - http://www.maastaar.net
 *
 * This is an example of using FUSE to build a simple filesystem. It is a part of a tutorial in MQH Blog with the title
 * "Writing a Simple Filesystem Using FUSE in C":
 * http://www.maastaar.net/fuse/linux/filesystem/c/2016/05/21/writing-a-simple-filesystem-using-fuse/
 *
 * License: GNU GPL
 */

// For documentation of FUSE methods see https://libfuse.github.io/doxygen/structfuse__operations.html

#undef DEBUG

// TODO: Comment lines to reduce debug messages
#define DEBUG
#define DEBUG_METHODS
#define DEBUG_RETURN_VALUES

#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "macros.h"
#include "myfs.h"
#include "myfs-info.h"
#include "blockdevice.h"

/// @brief Constructor of the in-memory file system class.
///
/// You may add your own constructor code here.
MyInMemoryFS::MyInMemoryFS()
		: MyFS()
{
	// TODO: [PART 1] Add your constructor code here
	numberOfOpenFiles = 0;
}

/// @brief Destructor of the in-memory file system class.
///
/// You may add your own destructor code here.
MyInMemoryFS::~MyInMemoryFS()
{
	// TODO: [PART 1] Add your cleanup code here
}

// Definitions of private methods here

// Check if file with file_name exists
// \param [in] file_name File name to check.
// \return index of MyFsFileInfo if file exists, otherwise -1.
int MyInMemoryFS::getFileIndex(const char *file_name)
{
	for (int i = 0; i < NUM_DIR_ENTRIES; i++)
	{
		if (strcmp(files[i].name, file_name) == 0)
			return i;
	}
	// Iterated through all file entries and file was not found
	return -1;
}

// Get a free slot which doesn't have a file stored
// \return index of MyFsFileInfo if free slot is found, otherwise -1.
int MyInMemoryFS::getFreeSlot(void)
{
	for (int i = 0; i < NUM_DIR_ENTRIES; i++)
	{
		if (files[i].name[0] == '\0')
		{
			// Empty name, so entry is free
			return i;
		}
	}
	return -1;
}

// Sanitize path
// \param [in] path Path to be sanitized.
// \return 0 on success, -ERRNO on failure.
int MyInMemoryFS::checkPath(const char *path)
{
	size_t path_len;

	if (path == NULL)
		return -EINVAL;

	path_len = strnlen(path, NAME_LENGTH);
	if (path_len == 0 || path_len == NAME_LENGTH)
		return -EINVAL;

	return 0;
}

// FUSE callbacks below this line

/// @brief Create a new file.
///
/// Create a new file with given name and permissions.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] mode Permissions for file access.
/// \param [in] dev Can be ignored.
/// \return 0 on success, -ERRNO on failure.
int MyInMemoryFS::fuseMknod(const char *path, mode_t mode, dev_t dev)
{
	int ret;
	char file_name[NAME_LENGTH];
	int index;
	MyFsFileInfo *new_file;
	time_t time_now;

	LOGM();

	// TODO: [PART 1] Implement this! Implemented by slno1011

	ret = checkPath(path);
	if (ret)
		return ret;

	strncpy(file_name, path, NAME_LENGTH - 1);
	file_name[NAME_LENGTH - 1] = '\0';

	if (getFileIndex(file_name) != -1)
		return -EEXIST;

	index = getFreeSlot();
	if (index == -1)
		return -ENOMEM;

	time_now = time(NULL);
	if (time_now == -1)
		return -EFAULT;

	new_file = &files[index];
	strncpy(new_file->name, file_name, NAME_LENGTH - 1);
	new_file->size = 0; /* size = 0, no data allocated yet */
	new_file->uid = getuid();
	new_file->gid = getgid();
	new_file->mode = mode;
	new_file->atime = new_file->mtime = new_file->ctime = time_now;
	new_file->data = NULL;

	RETURN(0);
}

/// @brief Delete a file.
///
/// Delete a file with given name from the file system.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \return 0 on success, -ERRNO on failure.
int MyInMemoryFS::fuseUnlink(const char *path)
{
	int ret, index;
	char file_name[NAME_LENGTH];
	MyFsFileInfo *file_ptr;

	LOGM();

	// TODO: [PART 1] Implement this! Implemented by slno1011
	ret = checkPath(path);
	if (ret)
		return ret;

	strncpy(file_name, path, NAME_LENGTH - 1);
	file_name[NAME_LENGTH - 1] = '\0';

	index = getFileIndex(file_name);
	if (index == -1)
		return -ENOENT;

	file_ptr = &files[index];
	if (file_ptr->data)
		free(file_ptr->data);
	/* Setting the first byte of name to '\0' would
	 * be enough, but in this case we are dealing with
	 * a structure which has user/group ids and the
	 * access mode saved. It's good security practice
	 * to override them with 0.
	 */
	memset(file_ptr, 0, sizeof(MyFsFileInfo));

	RETURN(0);
}

/// @brief Rename a file.
///
/// Rename the file with with a given name to a new name.
/// Note that if a file with the new name already exists it is replaced (i.e., removed
/// before renaming the file.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] newpath  New name of the file, starting with "/".
/// \return 0 on success, -ERRNO on failure.
int MyInMemoryFS::fuseRename(const char *path, const char *newpath)
{
	LOGM();

	// TODO: [PART 1] Implement this! Implemented by heli1017
	int ret;
	ret = checkPath(path);
	if (ret)
		return ret;
	ret = checkPath(newpath);
	if (ret)
		return ret;

	int index = getFileIndex(path);
	if (index >= 0)
	{
		if (getFileIndex(newpath) >= 0)
		{
			fuseUnlink(newpath);
		}
		strncpy(files[index].name, newpath, NAME_LENGTH - 1);
	}
	else
	{
		return -ENOENT;
	}

	return 0;
}

/// @brief Get file meta data.
///
/// Get the metadata of a file (user & group id, modification times, permissions, ...).
/// \param [in] path Name of the file, starting with "/".
/// \param [out] statbuf Structure containing the meta data, for details type "man 2 stat" in a terminal.
/// \return 0 on success, -ERRNO on failure.
int MyInMemoryFS::fuseGetattr(const char *path, struct stat *statbuf)
{
	int ret;
	MyFsFileInfo *file;

	LOGM();

	// TODO: [PART 1] Implement this! Implemented by slno1011

	// GNU's definitions of the attributes (http://www.gnu.org/software/libc/manual/html_node/Attribute-Meanings.html):
	// 		st_uid: 	The user ID of the file’s owner.
	//		st_gid: 	The group ID of the file.
	//		st_atime: 	This is the last access time for the file.
	//		st_mtime: 	This is the time of the last modification to the contents of the file.
	//		st_mode: 	Specifies the mode of the file. This includes file type information (see Testing File Type) and
	//		            the file permission bits (see Permission Bits).
	//		st_nlink: 	The number of hard links to the file. This count keeps track of how many directories have
	//	             	entries for this file. If the count is ever decremented to zero, then the file itself is
	//	             	discarded as soon as no process still holds it open. Symbolic links are not counted in the
	//	             	total.
	//		st_size:	This specifies the size of a regular file in bytes. For files that are really devices this field
	//		            isn’t usually meaningful. For symbolic links this specifies the length of the file name the link
	//		            refers to.
	statbuf->st_uid =
			getuid(); // The owner of the file/directory is the user who mounted the filesystem
	statbuf->st_gid =
			getgid(); // The group of the file/directory is the same as the group of the user who mounted the filesystem

	if (strcmp(path, "/") == 0)
	{
		LOGF("\tAttributes of dir %s requested\n", path);
		statbuf->st_mode = S_IFDIR | 0755;
		statbuf->st_nlink =
				2; // Why "two" hardlinks instead of "one"? The answer is here: http://unix.stackexchange.com/a/101536
		statbuf->st_atime = time(
				NULL); // The last "a"ccess of the file/directory is right now
		/* I don't think modification time should be now */
		statbuf->st_mtime = time(
				NULL); // The last "m"odification of the file/directory is right now
	}
	else
	{
		LOGF("\tAttributes of normal file %s requested\n", path);
		ret = checkPath(path);
		if (ret)
			return ret;

		ret = getFileIndex(path);
		if (ret == -1)
			return -ENOENT;

		file = &files[ret];
		/* access time is now */
		file->atime = time(NULL);

		statbuf->st_mode = S_IFREG | 0644;
		statbuf->st_nlink = 1;
		statbuf->st_size = file->size;
		statbuf->st_ctime = file->ctime;
		statbuf->st_mtime = file->mtime;
		statbuf->st_atime = file->atime;
	}

	RETURN(0);
}

/// @brief Change file permissions.
///
/// Set new permissions for a file.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] mode New mode of the file.
/// \return 0 on success, -ERRNO on failure.
int MyInMemoryFS::fuseChmod(const char *path, mode_t mode)
{
	LOGM();

	// TODO: [PART 1] Implement this!

	RETURN(0);
}

/// @brief Change the owner of a file.
///
/// Change the user and group identifier in the meta data of a file.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] uid New user id.
/// \param [in] gid New group id.
/// \return 0 on success, -ERRNO on failure.
int MyInMemoryFS::fuseChown(const char *path, uid_t uid, gid_t gid)
{
	// TODO: [PART 1] Implement this!
	LOGF("\tChange of the User and group ID of %s requested\n", path);
	if (checkPath(path))
		return -EINVAL;
	int fileIndex = getFileIndex(path);
	if (fileIndex == -1)
		return -ENOENT;
	MyFsFileInfo *file = &files[fileIndex];
	file->uid = uid;
	file->gid = gid;

	RETURN(0);
}

/// @brief Open a file.
///
/// Open a file for reading or writing. This includes checking the permissions of the current user and incrementing the
/// open file count.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [out] fileInfo Can be ignored in Part 1
/// \return 0 on success, -ERRNO on failure.
int MyInMemoryFS::fuseOpen(const char *path, struct fuse_file_info *fileInfo)
{
	int ret, index;
	char file_name[NAME_LENGTH];

	LOGM();
	// TODO: [PART 1] Implement this! implemented by danisltpi

	if (numberOfOpenFiles == NUM_OPEN_FILES)
	{
		return -EMFILE;
	}

	ret = checkPath(path);
	if (ret)
		return ret;

	strncpy(file_name, path, NAME_LENGTH - 1);
	file_name[NAME_LENGTH - 1] = '\0';
	index = getFileIndex(file_name);
	if (index == -1)
		return -ENOENT;

	LOGF("\topened %s, index = %d\n", path, index);

	// file handle uses index (because index starts at 0) of the file so if it is not set the file is not open;
	fileInfo->fh = index;
	numberOfOpenFiles++;

	RETURN(0);
}

/// @brief Read from a file.
///
/// Read a given number of bytes from a file starting form a given position.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// Note that the file content is an array of bytes, not a string. I.e., it is not (!) necessarily terminated by '\0'
/// and may contain an arbitrary number of '\0'at any position. Thus, you should not use strlen(), strcpy(), strcmp(),
/// ... on both the file content and buf, but explicitly store the length of the file and all buffers somewhere and use
/// memcpy(), memcmp(), ... to process the content.
/// \param [in] path Name of the file, starting with "/".
/// \param [out] buf The data read from the file is stored in this array. You can assume that the size of buffer is at
/// least 'size'
/// \param [in] size Number of bytes to read
/// \param [in] offset Starting position in the file, i.e., number of the first byte to read relative to the first byte of
/// the file
/// \param [in] fileInfo Can be ignored in Part 1
/// \return The Number of bytes read on success. This may be less than size if the file does not contain sufficient bytes.
/// -ERRNO on failure.
int MyInMemoryFS::fuseRead(const char *path, char *buf, size_t size,
													 off_t offset, struct fuse_file_info *fileInfo)
{
	int ret, index;
	char file_name[NAME_LENGTH];
	LOGM();

	// TODO: [PART 1] Implement this! // implemented by danisltpi

	LOGF("--> Trying to read %s, %lu, %lu\n", path, (unsigned long)offset,
			 size);

	ret = checkPath(path);
	if (ret)
		return ret;
	strncpy(file_name, path, NAME_LENGTH - 1);
	file_name[NAME_LENGTH - 1] = '\0';
	index = getFileIndex(file_name);
	if (index == -1)
		return -ENOENT;

	MyFsFileInfo *file = &files[index];
	char *selectedText = file->data;

	memcpy(buf, selectedText + offset, size);

	RETURN((int)(strlen(selectedText) - offset));
}

/// @brief Write to a file.
///
/// Write a given number of bytes to a file starting at a given position.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// Note that the file content is an array of bytes, not a string. I.e., it is not (!) necessarily terminated by '\0'
/// and may contain an arbitrary number of '\0'at any position. Thus, you should not use strlen(), strcpy(), strcmp(),
/// ... on both the file content and buf, but explicitly store the length of the file and all buffers somewhere and use
/// memcpy(), memcmp(), ... to process the content.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] buf An array containing the bytes that should be written.
/// \param [in] size Number of bytes to write.
/// \param [in] offset Starting position in the file, i.e., number of the first byte to read relative to the first byte of
/// the file.
/// \param [in] fileInfo Can be ignored in Part 1 .
/// \return Number of bytes written on success, -ERRNO on failure.
int MyInMemoryFS::fuseWrite(const char *path, const char *buf, size_t size,
														off_t offset, struct fuse_file_info *fileInfo)
{
	int ret, index;
	uintptr_t file_start, file_end, write_start;
	size_t alloc_size;
	char *alloc_buf;
	time_t time_now;
	MyFsFileInfo *file;

	// TODO: [PART 1] Implement this! Implemented by slno1011
	// WIP, Has to be tested thoroughly
	LOGM();

	LOGF("write: path: %s, buf: %s, size: %lu, offset: %ld",
		path, buf, size, offset);

	ret = checkPath(path);
	if (ret)
		return ret;

	index = getFileIndex(path);
	if (index == -1)
		return -ENOENT;

	file = &files[index];

	file_start = (uintptr_t)file->data;
	file_end = file_start + file->size;
	write_start = file_start + offset;

	if (file->data == NULL) {
		/* no data allocated yet, allocate it now */
		alloc_size = offset + size;
		file->data = (char *)malloc(alloc_size);
		if (file->data == NULL)
			return -ENOMEM;

		memset(file->data, 0, alloc_size);
		file->size = alloc_size;
	} else if (offset == 0) {
		/* file->data is already allocated, but offset is 0.
		 * The default UNIX behavior in this case is to overwrite the file.
		 */
		free(file->data);
		file->data = NULL;
		file->size = 0;

		alloc_size = offset + size;
		alloc_buf = (char *)malloc(alloc_size);
		if (alloc_buf == NULL)
			return -ENOMEM;

		file->data = alloc_buf;
		file->size = size;
	} else if ((write_start + size) > file_end) {
		alloc_size = offset + size;
		alloc_buf = (char *)malloc(alloc_size);
		if (alloc_buf == NULL)
			return -ENOMEM;

		memset(alloc_buf, 0, alloc_size);
		memcpy(alloc_buf, file->data, file->size);

		free(file->data);
		file->data = alloc_buf;
		file->size = alloc_size;
	}

	memcpy(file->data + offset, buf, size);

	/* do we have to check the return value? */
	time_now = time(NULL);
	file->atime = file->mtime = time_now;

	return size;
}

/// @brief Close a file.
///
/// In Part 1 this includes decrementing the open file count.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] fileInfo Can be ignored in Part 1 .
/// \return 0 on success, -ERRNO on failure.
int MyInMemoryFS::fuseRelease(const char *path, struct fuse_file_info *fileInfo)
{
	int ret, index;
	char file_name[NAME_LENGTH];
	LOGM();

	// TODO: [PART 1] Implement this! // implemented by danisltpi
	ret = checkPath(path);
	if (ret)
		return ret;
	strncpy(file_name, path, NAME_LENGTH - 1);
	file_name[NAME_LENGTH - 1] = '\0';
	index = getFileIndex(file_name);
	if (index == -1)
		return -ENOENT;

	fileInfo->fh = -1;
	numberOfOpenFiles--;
	RETURN(0);
}

/// @brief Truncate a file.
///
/// Set the size of a file to the new size. If the new size is smaller than the old size, spare bytes are removed. If
/// the new size is larger than the old size, the new bytes may be random.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] newSize New size of the file.
/// \return 0 on success, -ERRNO on failure.
int MyInMemoryFS::fuseTruncate(const char *path, off_t newSize)
{
	int ret, index;
	char *buf;
	bool copyNewSize;
	size_t copySize;
	MyFsFileInfo *file;

	LOGM();

	ret = checkPath(path);
	if (ret)
		return ret;

	index = getFileIndex(path);
	if (index == -1)
		return -ENOENT;

	file = &files[index];
	if (file->data == NULL || file->size == 0)
		return -ENOENT;

	if (file->size == (size_t)newSize)
		return 0;
	else if (file->size > (size_t)newSize)
		copyNewSize = true;
	else
		copyNewSize = false;

	buf = (char *)malloc(newSize);
	if (buf == NULL)
		return -ENOMEM;

	copySize = (copyNewSize) ? newSize : file->size;
	memcpy(buf, file->data, copySize);
	free(file->data);
	file->data = buf;
	file->size = copySize;

	// TODO: [PART 1] Implement this!

	return 0;
}

/// @brief Truncate a file.
///
/// Set the size of a file to the new size. If the new size is smaller than the old size, spare bytes are removed. If
/// the new size is larger than the old size, the new bytes may be random. This function is called for files that are
/// open.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] newSize New size of the file.
/// \param [in] fileInfo Can be ignored in Part 1.
/// \return 0 on success, -ERRNO on failure.
int MyInMemoryFS::fuseTruncate(const char *path, off_t newSize,
															 struct fuse_file_info *fileInfo)
{
	LOGM();

	// TODO: [PART 1] Implement this!

	RETURN(0);
}

/// @brief Read a directory.
///
/// Read the content of the (only) directory.
/// You do not have to check file permissions, but can assume that it is always ok to access the directory.
/// \param [in] path Path of the directory. Should be "/" in our case.
/// \param [out] buf A buffer for storing the directory entries.
/// \param [in] filler A function for putting entries into the buffer.
/// \param [in] offset Can be ignored.
/// \param [in] fileInfo Can be ignored.
/// \return 0 on success, -ERRNO on failure.
int MyInMemoryFS::fuseReaddir(const char *path, void *buf,
															fuse_fill_dir_t filler, off_t offset,
															struct fuse_file_info *fileInfo)
{
	LOGM();

	// TODO: [PART 1] Implement this!

	int ret = checkPath(path);
	if (ret)
		return ret;

	LOGF("--> Getting The List of Files of %s\n", path);

	filler(buf, ".", NULL, 0);	// Current Directory
	filler(buf, "..", NULL, 0); // Parent Directory

	if (strcmp(path, "/") == 0)
	{
		for (int i = 0; i < NUM_DIR_ENTRIES; i++)
		{
			if (files[i].name[0] != '\0')
			{
				filler(buf, files[i].name + 1, NULL, 0);
				LOGF("\t\t%s\n", files[i].name);
			}
		}
	}

	RETURN(0);
}

/// Initialize a file system.
///
/// This function is called when the file system is mounted. You may add some initializing code here.
/// \param [in] conn Can be ignored.
/// \return 0.
void *MyInMemoryFS::fuseInit(struct fuse_conn_info *conn)
{
	// Open logfile
	this->logFile = fopen(
			((MyFsInfo *)fuse_get_context()->private_data)->logFile, "w+");
	if (this->logFile == NULL)
	{
		fprintf(stderr, "ERROR: Cannot open logfile %s\n",
						((MyFsInfo *)fuse_get_context()->private_data)->logFile);
	}
	else
	{
		// turn of logfile buffering
		setvbuf(this->logFile, NULL, _IOLBF, 0);

		LOG("Starting logging...\n");

		LOG("Using in-memory mode");

		// TODO: [PART 1] Implement your initialization methods here
	}

	RETURN(0);
}

/// @brief Clean up a file system.
///
/// This function is called when the file system is unmounted. You may add some cleanup code here.
void MyInMemoryFS::fuseDestroy()
{
	LOGM();

	// TODO: [PART 1] Implement this!
}

// TODO: [PART 1] You may add your own additional methods here!

// DO NOT EDIT ANYTHING BELOW THIS LINE!!!

/// @brief Set the static instance of the file system.
///
/// Do not edit this method!
void MyInMemoryFS::SetInstance()
{
	MyFS::_instance = new MyInMemoryFS();
}
