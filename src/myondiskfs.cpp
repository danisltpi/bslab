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

static int *fatBuffer;
static DiskFileInfo *rootBuffer;

size_t align_to_block_size(size_t x)
{
	return (x + BLOCK_SIZE - 1) & ~(BLOCK_SIZE - 1);
}

/// @brief Constructor of the on-disk file system class.
///
/// You may add your own constructor code here.
MyOnDiskFS::MyOnDiskFS() : MyFS()
{
    // create a block device object
	// allocation failure check is lacking here
    this->blockDevice = new BlockDevice(BLOCK_SIZE);
}

/// @brief Destructor of the on-disk file system class.
///
/// You may add your own destructor code here.
MyOnDiskFS::~MyOnDiskFS()
{
    // free block device object
    delete this->blockDevice;
}

// Definitions of private methods here

// Check if file with file_name exists
// \param [in] file_name File name to check.
// \return index of MyFsFileInfo if file exists, otherwise -1.
int MyOnDiskFS::getFileIndex(const char *file_name)
{
    for (int i = 0; i < NUM_DIR_ENTRIES; i++)
    {
        if (strcmp(rootBuffer[i].name, file_name) == 0)
            return i;
    }
    // Iterated through all file entries and file was not found
    return -1;
}

// Get a free slot which doesn't have a file stored
// \return index of MyFsFileInfo if free slot is found, otherwise -1.
int MyOnDiskFS::getFreeRootSlot(void)
{
    for (int i = 0; i < NUM_DIR_ENTRIES; i++) {
		if (rootBuffer[i].name[0] == '\0') {
            // Empty name, so entry is free
            return i;
        }
    }

    return -1;
}

//checks if entry is in one block only
bool MyOnDiskFS::entryInOneBlock(int fileindex)
{
    int entrypos = sb.root_start + fileindex * sizeof(struct DiskFileInfo);
	int entryoffset = entrypos % BLOCK_SIZE;
	size_t entrylen = sizeof(struct DiskFileInfo);

    if((entryoffset + entrylen) < BLOCK_SIZE)
        return true;

    return false;
}

//Return the block index of the changed rootentry
int MyOnDiskFS::getChangedBlockIndex(int fileIndex)
{
	//STEP 1: fileIndex * Größe der Struct für DiskFileInfo + rootStart um auf die Position im Container zu kommen
	//STEP 2: Position % BLOCKSIZE um den Index des Blocks zu bekommen indem die File ist

    int entryPosition = fileIndex * sizeof(struct DiskFileInfo) + sb.root_start;
    int index = entryPosition % BLOCK_SIZE;
	return index;
}

//Return the number of changed blocks
int MyOnDiskFS::getNumChangedBlocks(int fileIndex)
{
	//STEP 1: get Position des entries in dem Container (siehe oben)
	//STEP 2: schauen ob Start des Entries im Block + sizeof(DiskFileInfo) < BLOCK_SIZE wenn ja return 1 else 2

    int entryPosition = getChangedBlockIndex(fileIndex);
    if(rootBuffer[entryPosition].firstblock + sizeof(DiskFileInfo) < BLOCK_SIZE)
        return 1;

    return 2;
}

int MyOnDiskFS::getEmptyBlockFAT(void)
{
	int fat_entries = sb.fat_size / sizeof(int);

	for (int i = 0; i < fat_entries; i++) {
		if (fatBuffer[i] == EMPTY_BLOCK)
			return i;
	}

	return -1;
}

void MyOnDiskFS::sync(uint32_t dest, void *src, size_t len)
{
	char *bufptr = (char *)src;

	LOGF("SYNC: fat = %d, fat_size = %ld, root = %d, root_size = %ld, data = %d\n",
		sb.fat_start, sb.fat_size, sb.root_start, sb.root_size, sb.data_start);

	for (uint32_t block = 0; block != len; block += BLOCK_SIZE) {
		this->blockDevice->write(dest + block, bufptr + block);
	}
}

void MyOnDiskFS::syncFAT(void)
{
	sync(sb.fat_start, fatBuffer, sb.fat_size);
}

void MyOnDiskFS::syncRoot(void)
{
	sync(sb.root_start, rootBuffer, sb.root_size);
}

int MyOnDiskFS::fatToDataAddress(int fat_index)
{
	return sb.data_start + fat_index * BLOCK_SIZE;
}

int MyOnDiskFS::getEmptyBlockChain(int num_blocks)
{
	/* used to temporarily store blocks to free in case
	 * we fail to claim all required blocks
	 */
	int *freelist;
	char *zeromem;
	int claimed_blocks = 0;
	int start_block = -1, prev_block = -1;

	freelist = (int *)malloc(sizeof(int) * num_blocks);
	if (freelist == NULL)
		return -ENOMEM;

	zeromem = (char *)malloc(BLOCK_SIZE);
	if (zeromem == NULL) {
		free(freelist);
		return -ENOMEM;
	}

	memset(zeromem, 0, BLOCK_SIZE);

	while (claimed_blocks < num_blocks) {
		int block = getEmptyBlockFAT();

		if (block == EOC_BLOCK)
			goto free_blocks;

		freelist[claimed_blocks++] = block;

		if (start_block == -1)
			start_block = block;

		if (prev_block != -1)
			fatBuffer[prev_block] = block;
		fatBuffer[block] = EOC_BLOCK;
		prev_block = block;
		/* clear claimed memory */
		blockDevice->write(fatToDataAddress(block), zeromem);
	}

	free(freelist);
	free(zeromem);

	return start_block;

free_blocks:
	for (int i = 0; i < claimed_blocks; i++) {
		fatBuffer[freelist[i]] = EMPTY_BLOCK;
	}

	free(freelist);
	free(zeromem);

	return -ENOMEM;
}

void MyOnDiskFS::appendBlock(int start_block, int block)
{
	int current_block = start_block;

	while (fatBuffer[current_block] != EOC_BLOCK)
		current_block = fatBuffer[current_block];

	fatBuffer[current_block] = block;
}

/// @brief Write buffer to data segment in container.
///
/// \param [in] block_index Index refers to FAT
/// \param [in] buf Source buffer to be written in the container
/// \param [in] size Length of source buffer
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::writeData(int block_index, const char *buf,
	size_t size, int offset_in_block)
{
	int ret;
	int buf_offset = 0;
	size_t writelen = 0;
	char *block;

	if (size <= 0)
		return 0;

	block = (char *)malloc(BLOCK_SIZE);
	if (block == NULL)
		return -ENOMEM;

	while (size > 0) {
		ret = this->blockDevice->read(fatToDataAddress(block_index), block);
		if (ret < 0)
			goto exit;

		writelen = (size > BLOCK_SIZE) ? BLOCK_SIZE : size;

		/* this can happen for the first block */
		if (offset_in_block != 0) {
			size_t block_space_left = BLOCK_SIZE - offset_in_block;
			writelen = (size > block_space_left) ? block_space_left : size;
			memcpy(block + offset_in_block, buf + buf_offset, writelen);
			offset_in_block = 0;
		} else {
			memcpy(block, buf + buf_offset, writelen);
		}

		size -= writelen;
		buf_offset += writelen;
		ret = this->blockDevice->write(fatToDataAddress(block_index), block);
		if (ret < 0)
			goto exit;
		block_index = fatBuffer[block_index];
	}

	ret = 0;

exit:
	free(block);

	return ret;
}

int MyOnDiskFS::readData(int block_index, const char *buf, size_t size,
							int offset_in_block)
{
	int ret;
	int buf_offset = 0;
	int readlen;
	char *block;

	block = (char *)malloc(BLOCK_SIZE);
	if (block == NULL)
		return -ENOMEM;

	memset(block, 0, BLOCK_SIZE);

	while (size > 0) {
		readlen = (size > BLOCK_SIZE) ? BLOCK_SIZE : size;

		if (offset_in_block != 0) {
			size_t block_space_left = BLOCK_SIZE - offset_in_block;
			readlen = (size > block_space_left) ? block_space_left : size;
			offset_in_block = 0;
		}

		ret = this->blockDevice->read(fatToDataAddress(block_index), block);
		if (ret < 0) {
			goto exit;
		}
		memcpy((char *)(buf + buf_offset), (char *)(block + offset_in_block), readlen);

		size -= readlen;
		buf_offset += readlen;
		block_index = fatBuffer[block_index];
	}

exit:
	free(block);

	return ret;
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
	int ret;
	int emptyblock, slot;
	time_t time_now;
	DiskFileInfo *new_file;

    LOGM();

	ret = checkPath(path);
	if (ret)
		return ret;

	if (getFileIndex(path) != -1)
		return -EEXIST;

	time_now = time(NULL);
	if (time_now == -1)
		return -EFAULT;

	emptyblock = getEmptyBlockFAT();
	if (emptyblock == -1)
		return -ENOMEM;

	slot = getFreeRootSlot();
	if (slot == -1)
		return -ENOMEM;

	/* at this point, we have an empty data block and a free root slot */
	new_file = &rootBuffer[slot];
	strncpy(new_file->name, path, NAME_LENGTH - 1);
	new_file->size = 0;
	new_file->uid = getuid();
	new_file->gid = getgid();
	new_file->mode = mode;
	new_file->atime = new_file->mtime = new_file->ctime = time_now;
	new_file->firstblock = -1;
	/* mark the empty block as used now */
	//fatBuffer[emptyblock] = EOC_BLOCK;

	/* sync root and fat back to container block device */
	syncFAT();
	syncRoot();

    // TODO: [PART 2] Implement this!

    RETURN(0);
}

void MyOnDiskFS::freeFileData(int start_block)
{
	/* file is deleted, set all linked blocks in FAT to EMPTY_BLOCK */
	int next, current_block = start_block;
	while (current_block != EOC_BLOCK) {
		next = fatBuffer[current_block];
		fatBuffer[current_block] = EMPTY_BLOCK;
		current_block = next;
	}
	syncFAT();
}

/// @brief Delete a file.
///
/// Delete a file with given name from the file system.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseUnlink(const char *path)
{
	int ret, index;
	DiskFileInfo *file_ptr;

    LOGM();

	ret = checkPath(path);
	if (ret)
		return ret;

	index = getFileIndex(path);
	if (index == -1)
		return -ENOENT;

	file_ptr = &rootBuffer[index];
	if (file_ptr->firstblock != EOC_BLOCK)
		freeFileData(file_ptr->firstblock);

	memset(file_ptr, 0, sizeof(struct DiskFileInfo));

	syncRoot();
    RETURN(0);
	//Implemented by Maik 
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
	int ret, index;

    LOGM();

	ret = checkPath(path);
	if (ret)
		return ret;
	ret = checkPath(newpath);
	if (ret)
		return ret;

	index = getFileIndex(path);
	if (index == -1)
		return -ENOENT;

	if (getFileIndex(newpath) != -1)
		fuseUnlink(newpath);

	strncpy(rootBuffer[index].name, newpath, NAME_LENGTH - 1);

	syncRoot();

	RETURN(0);
}

/// @brief Get file meta data.
///
/// Get the metadata of a file (user & group id, modification times, permissions, ...).
/// \param [in] path Name of the file, starting with "/".
/// \param [out] statbuf Structure containing the meta data, for details type "man 2 stat" in a terminal.
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseGetattr(const char *path, struct stat *statbuf)
{
	int ret;
	DiskFileInfo *file;

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
		statbuf->st_atime = time(NULL); // The last "a"ccess of the file/directory is right now
		/* I don't think modification time should be now */
		statbuf->st_mtime = time(NULL); // The last "m"odification of the file/directory is right now
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

		file = &rootBuffer[ret];
		/* access time is now */
		file->atime = time(NULL);

		statbuf->st_mode = S_IFREG | file->mode;
		statbuf->st_nlink = 1;
		statbuf->st_size = file->size;
		statbuf->st_ctime = file->ctime;
		statbuf->st_mtime = file->mtime;
		statbuf->st_atime = file->atime;

		syncRoot();
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
int MyOnDiskFS::fuseChmod(const char *path, mode_t mode)
{
	int ret, index;

	LOGM();

	// TODO: [PART 1] Implement this!
	ret = checkPath(path);
	if (ret)
		return ret;

	index = getFileIndex(path);
	if (index == -1)
		return -ENOENT;

	rootBuffer[index].mode = mode;

	syncRoot();

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
	int fileIndex;
	DiskFileInfo *file;

    LOGM();

	if (checkPath(path))
		return -EINVAL;

	fileIndex = getFileIndex(path);
	if (fileIndex == -1)
		return -ENOENT;

	file = &rootBuffer[fileIndex];
	file->uid = uid;
	file->gid = gid;

	syncRoot();

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
	int ret, index;

    LOGM();

	if (numberOfOpenFiles == NUM_OPEN_FILES)
		return -EMFILE;

	ret = checkPath(path);
	if (ret)
		return ret;

	index = getFileIndex(path);
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
int MyOnDiskFS::fuseRead(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo)
{
	int ret, index;
	DiskFileInfo *file;

    LOGM();
	LOGF("--> Trying to read %s, %lu, %lu\n", path, (unsigned long)offset, size);

	if (size == 0)
		return 0;

	ret = checkPath(path);
	if (ret)
		return ret;

	index = getFileIndex(path);
	if(index == -1)
		return -ENOENT;

	file = &rootBuffer[index];
	if (file->firstblock == EOC_BLOCK)
		return -ENOBUFS;

	/* offset is out of bounds */
	if (file->size < offset)
		return -ENOBUFS;

	/* read would be out of bounds */
	if (file->size < size)
		size = file->size;
	else if (file->size < (offset + size))
		size -= offset;

	int offset_in_blocks = offset / BLOCK_SIZE;
	int read_offset_in_block = offset % BLOCK_SIZE;
	int current_block = file->firstblock;

	for (int block_iter = 0; block_iter < offset_in_blocks; block_iter++)
		current_block = fatBuffer[current_block];

	ret = readData(current_block, buf, size, read_offset_in_block);
	if (ret < 0) {
		RETURN(int(ret));
	}

	/*
	int file_size = file->size;
	if(file_size < (size + offset)){
		size = file_size - offset;
	}
	int next, blockcount, current_block = file->firstblock;
	blockcount  = (int) offset / BLOCK_SIZE;
	for (int i = 0; i < blockcount; i++){
		next = fatBuffer[current_block];
		current_block = next;
	}
	memcpy(buf, &fatBuffer[current_block]+offset, size);
	*/

	RETURN((int)size);
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
	int ret, index;
	DiskFileInfo *file;

    LOGM();
    // TODO: [PART 2] Implement this!
	ret = checkPath(path);
	if (ret)
		return ret;

	index = getFileIndex(path);
	if (index == -1)
		return -ENOENT;

	file = &rootBuffer[index];
	/* no data yet, fresh allocation */
	if (file->firstblock == -1) {
		int firstblock;
		int num_alloc_blocks = (offset + size) / BLOCK_SIZE;
		int write_start_block = offset / BLOCK_SIZE;
		int offset_in_block = offset % BLOCK_SIZE;

		if (num_alloc_blocks == 0)
			num_alloc_blocks = 1;

		if ((offset_in_block + size) >= BLOCK_SIZE)
			num_alloc_blocks++;

		firstblock = getEmptyBlockChain(num_alloc_blocks);
		if (firstblock < 0)
			/* return -ERRNO */
			return firstblock;

		ret = writeData(firstblock, buf, size, offset_in_block);
		if (ret < 0)
			/* TODO: we should free claimed blocks just like in getEmptyBlockChain */
			return ret;

		file->size = offset + size;
		file->firstblock = firstblock;

		return size;
	} else if ((offset + size) > file->size) {
		int needed_blocks = (offset + size) / BLOCK_SIZE;
		int avail_blocks = file->size / BLOCK_SIZE;
		int num_append = needed_blocks - avail_blocks;
		int start_block = offset / BLOCK_SIZE;
		int offset_in_block = offset % BLOCK_SIZE;

		if (num_append != 0) {
			int block = getEmptyBlockChain(num_append);
			if (block < 0)
				return block;
			
			appendBlock(file->firstblock, block);
		}

		ret = writeData(start_block, buf, size, offset_in_block);
		if (ret < 0)
			return ret;

		file->size = offset + size;
	} else {
		int start_block = offset / BLOCK_SIZE;
		int offset_in_block = offset % BLOCK_SIZE;

		ret = writeData(start_block, buf, size, offset_in_block);
		if (ret < 0)
			return ret;
	}

	syncRoot();
	syncFAT();

	RETURN((int)size);

    //RETURN(0);
}

/// @brief Close a file.
///
/// \param [in] path Name of the file, starting with "/".
/// \param [in] File handel for the file set by fuseOpen.
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseRelease(const char *path, struct fuse_file_info *fileInfo)
{
	int ret, index;

    LOGM();

	ret = checkPath(path);
	if (ret)
		return ret;

	index = getFileIndex(path);
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
int MyOnDiskFS::fuseTruncate(const char *path, off_t newSize)
{
	int ret, index;
	DiskFileInfo *file;

    LOGM();

	ret = checkPath(path);
	if (ret)
		return ret;

	index = getFileIndex(path);
	if (index == -1)
		return -ENOENT;

	file = &rootBuffer[index];

	if (newSize == 0) {
		if (file->firstblock != EOC_BLOCK) {
			freeFileData(file->firstblock);
			file->firstblock = -1;
		}
		file->size = 0;
		return 0;
	}

	if (file->size == (size_t)newSize)
		return 0;

	if (newSize > file->size) {
		int diff = newSize - file->size;
		int blocks_to_append = diff / BLOCK_SIZE;
		if (blocks_to_append == 0) {
			int offset_in_block = file->size % BLOCK_SIZE;
			if ((offset_in_block + diff) > BLOCK_SIZE)
				blocks_to_append = 1;
		}
		
		int block = getEmptyBlockChain(blocks_to_append);
		if (block < 0)
			return block;
		if (file->firstblock == -1)
			file->firstblock = block;
		else
			appendBlock(file->firstblock, block);

		file->size = newSize;
		return 0;
	} else {	// newSize < file->size
		int new_blocks = newSize / BLOCK_SIZE;
		int blocks_avail = file->size / BLOCK_SIZE;
		int blocks_to_remove = blocks_avail - newSize;

		file->size = newSize;

		if (!blocks_to_remove)
			return 0;

		/* should check if file->firstblock == -1 */
		int current_block = file->firstblock;

		for (int block_iter = 0; block_iter < new_blocks; block_iter++)
			current_block = fatBuffer[current_block];

		/* i didn't check it thoroughly, might need +1, -1 adjustments */
		freeFileData(current_block);
	}

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

	int ret = checkPath(path);
	if (ret)
		return ret;

	filler(buf, ".", NULL, 0);	// Current Directory
	filler(buf, "..", NULL, 0); // Parent Directory

	if (strcmp(path, "/") == 0)
	{
		for (int i = 0; i < NUM_DIR_ENTRIES; i++)
		{
			if (rootBuffer[i].name[0] != '\0')
			{
				filler(buf, rootBuffer[i].name + 1, NULL, 0);
				LOGF("\t\t%s\n", rootBuffer[i].name);
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
void *MyOnDiskFS::fuseInit(struct fuse_conn_info *conn)
{
    // Open logfile
    this->logFile = fopen(((MyFsInfo *)fuse_get_context()->private_data)->logFile, "w+");
    if (this->logFile == NULL)
    {
        fprintf(stderr, "ERROR: Cannot open logfile %s\n", ((MyFsInfo *)fuse_get_context()->private_data)->logFile);
		return 0;
	}

	// turn of logfile buffering
	setvbuf(this->logFile, NULL, _IOLBF, 0);

	LOG("Starting logging...\n");
	LOG("Using on-disk mode");
	LOGF("Container file name: %s", ((MyFsInfo *)fuse_get_context()->private_data)->contFile);

	int ret = this->blockDevice->open(((MyFsInfo *)fuse_get_context()->private_data)->contFile);
	if (ret < 0 && ret != -ENOENT) {
		LOGF("ERROR: Access to container file failed with error %d", ret);
		return 0;
	}

	if (ret >= 0)
	{
		LOG("Container file does exist, reading");

		char *buf = (char *)malloc(BLOCK_SIZE);
		if (buf == NULL)
			return 0;

		memset(buf, 0, BLOCK_SIZE);

		ret = this->blockDevice->read(0, buf);
		if (ret < 0)
			LOGF("FATAL in %s: blockDevice read returned %d\n", __func__, ret);
		// kopiere daten des ersten blocks in sb (Superblock)
		memcpy(&sb, buf, sizeof(sb));

		LOGF("fat = %d, fat_size = %ld, root = %d, root_size = %ld, data = %d\n",
			sb.fat_start, sb.fat_size, sb.root_start, sb.root_size, sb.data_start);

		// here should sb input sanity checks happen, but we don't care for now

		fatBuffer = (int *)malloc(sb.fat_size);
		if (fatBuffer == NULL) {
			free(buf);
			return 0;
		}

		rootBuffer = (DiskFileInfo *)malloc(sb.root_size);
		if (rootBuffer == NULL) {
			free(buf);
			free(fatBuffer);
			return 0;
		}

		// TODO: find better return values in case of allocation failures above

		memset(fatBuffer, 0, sizeof(sb.fat_size));
		memset(rootBuffer, 0, sizeof(sb.root_size));

		int fat_end = sb.fat_start + sb.fat_size;
		int offset;
		char *bufptr = (char *)fatBuffer;

		/* read FAT into RAM, reading is done one block of BLOCK_SIZE at a time */
		for (int block = sb.fat_start; block != fat_end; block += BLOCK_SIZE) {
			offset = block - sb.fat_start;
			ret = this->blockDevice->read(block, bufptr + offset);
			if (ret < 0)
				LOGF("FATAL in %s: blockDevice read returned %d\n", __func__, ret);
		}

		int root_end = sb.root_start + sb.root_size;
		bufptr = (char *)rootBuffer;

		/* read root entries into RAM, reading is done one block of BLOCK_SIZE at a time */
		for (int block = sb.root_start; block != root_end; block += BLOCK_SIZE) {
			offset = block - sb.root_start;
			ret = this->blockDevice->read(block, bufptr + offset);
			if (ret < 0)
				LOGF("FATAL in %s: blockDevice read returned %d\n", __func__, ret);
		}
	}
	else if (ret == -ENOENT)
	{
		LOG("Container file does not exist, creating a new one");

		ret = this->blockDevice->create(((MyFsInfo *)fuse_get_context()->private_data)->contFile);
		if (ret < 0) {
			LOGF("ERROR: Creation of container file failed with error %d", ret);
			return 0;
		}

		char *buf = (char *)malloc(BLOCK_SIZE);
		if (buf == NULL)
			return 0;

		memset(buf, 0, BLOCK_SIZE);

		/* fat starts at block 1, because the superblock is at the previous block */
		sb.fat_start = 1;
		/* fat size is aligned to block size */
		sb.fat_size = DATA_BLOCK_COUNT * sizeof(int);
		sb.root_start = sb.fat_start + sb.fat_size;
		/* root size might need alignment, this makes it easier to
		 * read/write from/to RAM as it's done one block at a time
		 */
		sb.root_size = align_to_block_size(sizeof(struct DiskFileInfo) * NUM_DIR_ENTRIES);
		sb.data_start = sb.root_start + sb.root_size;

		memcpy(buf, &sb, sizeof(sb));
		/* write the superblock back as it's empty after container creation */
		ret = this->blockDevice->write(0, buf);
		if (ret < 0)
			LOGF("FATAL in %s: blockDevice write returned %d\n", __func__, ret);

		fatBuffer = (int *)malloc(sb.fat_size);
		if (fatBuffer == NULL) {
			free(buf);
			return 0;
		}

		rootBuffer = (DiskFileInfo *)malloc(sb.root_size);
		if (rootBuffer == NULL) {
			free(buf);
			free(fatBuffer);
			return 0;
		}

		memset(fatBuffer, 0, sizeof(sb.fat_size));
		memset(rootBuffer, 0, sizeof(sb.root_size));

		free(buf);

		/* write back FAT and root entries to newly created container */
		//syncFAT();
		//syncRoot();
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
