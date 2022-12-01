//
// Created by user on 29.11.22.
//

#ifndef MYFS_MYFS_SUPERBLOCK_H
#define MYFS_MYFS_SUPERBLOCK_H

#include <cstdint>

class myFsSuperblock {
    private:
        uint32_t size; //Größe Dateisystem
        uint32_t numBlocks; //Anzahl Blöcke
        uint32_t sizeBlocks; //Größe der Blöcke

    public:
        int getSize();
        int getNumBlocks();
        int getSizeBlocks();
};

#endif //MYFS_MYFS_SUPERBLOCK_H
