//
// Created by Oliver Waldhorst on 20.03.20.
// Copyright © 2017-2020 Oliver Waldhorst. All rights reserved.
//

#include "myondiskfs.h"

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
//#include "OpenFile.h"

int *fatBuffer;
DiskFileInfo *rootBuffer;

/// @brief Constructor of the on-disk file system class.
///
/// You may add your own constructor code here.
MyOnDiskFS::MyOnDiskFS() : MyFS()
{
    // create a block device object
    this->blockDevice = new BlockDevice(BLOCK_SIZE);

    //MyFsSuperBlock sb;
    //OpenFile openFiles[NUM_OPEN_FILES];
}

/// @brief Destructor of the on-disk file system class.
///
/// You may add your own destructor code here.
MyOnDiskFS::~MyOnDiskFS()
{
    // free block device object
    delete this->blockDevice;

    // TODO: [PART 2] Add your cleanup code here
}

// Definitions of private methods here

// Check if file with file_name exists
// \param [in] file_name File name to check.
// \return index of MyFsFileInfo if file exists, otherwise -1.
int MyOnDiskFS::getFileIndex(const char *file_name)
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
int MyOnDiskFS::getFreeSlot(void)
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
int MyOnDiskFS::checkPath(const char *path)
{
    size_t path_len;

    if (path == NULL)
        return -EINVAL;

    path_len = strnlen(path, NAME_LENGTH);
    if (path_len == 0 || path_len == NAME_LENGTH)
        return -EINVAL;

    return 0;
}

/// @brief Create a new file.
///
/// Create a new file with given name and permissions.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] mode Permissions for file access.
/// \param [in] dev Can be ignored.
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseMknod(const char *path, mode_t mode, dev_t dev)
{
    LOGM();

    // TODO: [PART 2] Implement this!

    RETURN(0);
}

/// @brief Delete a file.
///
/// Delete a file with given name from the file system.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseUnlink(const char *path)
{
    LOGM();

    // TODO: [PART 2] Implement this!

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
int MyOnDiskFS::fuseRename(const char *path, const char *newpath)
{
    LOGM();

    // TODO: [PART 2] Implement this!
    int ret;
    ret = checkPath(path);
    if (ret)
        return ret;
    ret = checkPath(newpath);
    if (ret)
        return ret;

    int index = getFileIndex(path); // holt sich index der gesuchten Datei path
    if (index >= 0)									// falls Datei path vorhanden
    {
        if (getFileIndex(newpath) >= 0) // checken, ob Datei mit neuem Namen existiert
        {
            fuseUnlink(newpath); // löscht Datei mit neuem Namen
        }
        strncpy(files[index].name, newpath, NAME_LENGTH - 1); // nennt path in newpath um
    }
    else
    {
        return -ENOENT; // ERROR, falls Datei path nicht existiert
    }

    return 0;
}

/// @brief Get file meta data.
///
/// Get the metadata of a file (user & group id, modification times, permissions, ...).
/// \param [in] path Name of the file, starting with "/".
/// \param [out] statbuf Structure containing the meta data, for details type "man 2 stat" in a terminal.
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseGetattr(const char *path, struct stat *statbuf)
{
    LOGM();

    // TODO: [PART 2] Implement this!

    RETURN(0);
}

/// @brief Change file permissions.
///
/// Set new permissions for a file.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] mode New mode of the file.
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseChmod(const char *path, mode_t mode)
{
    LOGM();

    // TODO: [PART 2] Implement this!

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
int MyOnDiskFS::fuseChown(const char *path, uid_t uid, gid_t gid)
{
    LOGM();

    // TODO: [PART 2] Implement this!

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
int MyOnDiskFS::fuseOpen(const char *path, struct fuse_file_info *fileInfo)
{
    LOGM();

    // TODO: [PART 2] Implement this!
    //fileInfo->fh = i;

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
int MyOnDiskFS::fuseRead(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo)
{
    LOGM();

    // TODO: [PART 2] Implement this!
    //Infos zur Datei stehen in     openFiles[fileInfo->fh]

    RETURN(0);
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
int MyOnDiskFS::fuseWrite(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo)
{
    LOGM();

    // TODO: [PART 2] Implement this!

    RETURN(0);
}

/// @brief Close a file.
///
/// \param [in] path Name of the file, starting with "/".
/// \param [in] File handel for the file set by fuseOpen.
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseRelease(const char *path, struct fuse_file_info *fileInfo)
{
    LOGM();

    // TODO: [PART 2] Implement this!

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
int MyOnDiskFS::fuseTruncate(const char *path, off_t newSize)
{
    LOGM();

    // TODO: [PART 2] Implement this!

    RETURN(0);
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
int MyOnDiskFS::fuseTruncate(const char *path, off_t newSize, struct fuse_file_info *fileInfo)
{
    LOGM();

    // TODO: [PART 2] Implement this!

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
int MyOnDiskFS::fuseReaddir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fileInfo)
{
    LOGM();

    // TODO: [PART 2] Implement this!

    RETURN(0);
}

/// Initialize a file system.
///
/// This function is called when the file system is mounted. You may add some initializing code here.
/// \param [in] conn Can be ignored.
/// \return 0.
void *MyOnDiskFS::fuseInit(struct fuse_conn_info *conn)
{
    // Open logfile
    this->logFile = fopen(((MyFsInfo *)fuse_get_context()->private_data)->logFile, "w+");
    if (this->logFile == NULL)
    {
        fprintf(stderr, "ERROR: Cannot open logfile %s\n", ((MyFsInfo *)fuse_get_context()->private_data)->logFile);
    }
    else
    {
        // turn of logfile buffering
        setvbuf(this->logFile, NULL, _IOLBF, 0);

        LOG("Starting logging...\n");

        LOG("Using on-disk mode");

        LOGF("Container file name: %s", ((MyFsInfo *)fuse_get_context()->private_data)->contFile);

        int ret = this->blockDevice->open(((MyFsInfo *)fuse_get_context()->private_data)->contFile);

        if (ret >= 0)
        {
            LOG("Container file does exist, reading");

            // TODO: [PART 2] Read existing structures form file
            char buffer[BLOCK_SIZE] = {0};
            // lese gespeicherte daten vom datentraeger ein
            this->blockDevice->read(0, buffer);
            // kopiere daten des ersten blocks in sb (Superblock)
            memcpy(&sb, buffer, BLOCK_SIZE);

			LOGF("fat = %ld, root = %ld, data = %ld\n",
				sb.fat_start, sb.root_start, sb.data_start);

			// here should sb input sanity checks happen, but we don't care for now

			fatBuffer = (int *)malloc(sb.fat_size);
			if (fatBuffer == NULL)
				return 0;
			rootBuffer = (DiskFileInfo *)malloc(sb.root_size);
			if (rootBuffer == NULL)
				return 0;

			// todo: find better return values in case of allocation failure above

			memset(fatBuffer, 0, sizeof(sb.fat_size));
			memset(rootBuffer, 0, sizeof(sb.root_size));

			uint32_t fat_end = sb.fat_start + sb.fat_size;
			char *bufptr;

			bufptr = (char *)fatBuffer;

			for (uint32_t i = sb.fat_start, offset = 0; i < fat_end; i++, offset++) {
				this->blockDevice->read(i, bufptr + offset);
			}

			uint32_t root_end = sb.root_start + sb.root_size;
			bufptr = (char *)rootBuffer;

			for (uint32_t i = sb.root_start, offset = 0; i < root_end; i++, offset++) {
				this->blockDevice->read(i, bufptr + offset);
			}

			LOGF("fat = %d, fat_size = %d, root = %d, root_size = %d, data = %d\n",
				sb.fat_start, sb.fat_size, sb.root_start, sb.root_size, sb.data_start);
        }
        else if (ret == -ENOENT)
        {
            LOG("Container file does not exist, creating a new one");

            ret = this->blockDevice->create(((MyFsInfo *)fuse_get_context()->private_data)->contFile);

            if (ret >= 0)
            {
				char buf[BLOCK_SIZE] = {0};
				sb.fat_start = 1;
				sb.fat_size = DATA_BLOCK_COUNT * sizeof(int);
				sb.root_start = sb.fat_start + sb.fat_size;
				sb.root_size = sizeof(struct DiskFileInfo) * NUM_DIR_ENTRIES;
				sb.data_start = sb.root_start + sizeof(struct DiskFileInfo) * NUM_DIR_ENTRIES;

				memcpy(buf, &sb, sizeof(sb));
				/* write the superblock back as it's empty after container creation */
				this->blockDevice->write(0, buf);

				fatBuffer = (int *)malloc(sb.fat_size);
				if (fatBuffer == NULL)
					return 0;
				rootBuffer = (DiskFileInfo *)malloc(sb.root_size);
				if (rootBuffer == NULL)
					return 0;

				memset(fatBuffer, 0, sizeof(sb.fat_size));
				memset(rootBuffer, 0, sizeof(sb.root_size));
            }
        }

        if (ret < 0)
        {
            LOGF("ERROR: Access to container file failed with error %d", ret);
        }
    }

    return 0;
}

/// @brief Clean up a file system.
///
/// This function is called when the file system is unmounted. You may add some cleanup code here.
void MyOnDiskFS::fuseDestroy()
{
    LOGM();

    // TODO: [PART 2] Implement this!
}

// TODO: [PART 2] You may add your own additional methods here!

// DO NOT EDIT ANYTHING BELOW THIS LINE!!!

/// @brief Set the static instance of the file system.
///
/// Do not edit this method!
void MyOnDiskFS::SetInstance()
{
    MyFS::_instance = new MyOnDiskFS();
}
