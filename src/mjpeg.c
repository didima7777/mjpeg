#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


#define HDLR_COMPONENT_NAME_SIZE 16
#define UNUSED(x) (void)(x)

#pragma pack(push,1)
typedef struct HANDLER_MEDIA_INDEX_s {
    uint8_t  ckid[4];
    uint32_t flags;
    uint32_t offset;
    uint32_t length;
}HANDLER_MEDIA_INDEX_sT;

typedef struct MEDIA_INDEX_s {
	uint8_t  ckid[4];
    uint8_t  flags;
    uint32_t offset;
    uint32_t length;
}MEDIA_INDEX_s;

typedef struct HANDLER_FILE_ATOM_s {
    uint8_t  mark[4];
    uint32_t Size;  //內容大小，不含4byte length, 4byte maker
    uint8_t  Type[4];
}HANDLER_FILE_ATOM_sT;

typedef struct AVIH_ATOM_INFO_s {
    uint32_t micro_sec_pre_frame;
    uint32_t max_byte_pre_sec;
    uint32_t reserved;
    uint32_t flags;
    uint32_t frames;
    uint32_t init_flames;
    uint32_t streams;
    uint32_t bufsize;
    uint32_t width;
    uint32_t height;
    uint32_t scale;
    uint32_t rate;
    uint32_t start;
    uint32_t length;
} AVIH_ATOM_INFO_sT;
#pragma pack(pop)

static MEDIA_INDEX_s     media_list[500];
static AVIH_ATOM_INFO_sT avih_info;
static uint8_t loadImage[1024 * 1024] = { 0x00 };

uint32_t swap_uint32(uint32_t data)
{
    uint32_t val;

    val = data;

#if 0
    return val;
#else
    val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF);
    return (val << 16) | (val >> 16);
#endif
}

uint32_t seek_movie_file_in_list(char chackAtomName[4], FILE *handle, uint32_t offset, uint32_t *length)
{
    uint32_t size, loadAtomSize, length_of_ulong;
    uint32_t file_offset;
    uint8_t  loadAtomName[4] = { 0x00 };
    uint8_t  cnt             = 0;

    if (handle == NULL)
    {
        printf("open file error!\r\n");
        return 0;
    }

    length_of_ulong = sizeof(uint32_t);
    file_offset     = offset;
    while (1)
    {
        // read list name
        fseek(handle, file_offset, SEEK_SET);
        fread((void *)&loadAtomName, 1, length_of_ulong, handle);

        // read header size
        file_offset += 4;
        fread((void *)&size, 1, length_of_ulong, handle);
        loadAtomSize = size;

        if (memcmp("LIST", loadAtomName, 4) == 0)
        {
            // read header name
            file_offset += 4;
            fread((void *)&loadAtomName, 1, length_of_ulong, handle);
            if (memcmp(chackAtomName, loadAtomName, 4) == 0)
            {
                *length = loadAtomSize;
                break;
            }
            else
            {
                file_offset += loadAtomSize;
            }
        }
        else
        {
            file_offset += (loadAtomSize + 4);
            if ((++cnt) >= 10) { return 0; }
        }
    }
    return file_offset;
}


uint32_t read_movie_index(FILE *handle, uint32_t offset, uint32_t movi_offset)
{
    uint32_t size, loadAtomSize, length_of_ulong, length_of_index_st, i = 0;
    uint32_t file_offset;
    uint8_t  loadAtomName[4] = { 0x00 };
    HANDLER_MEDIA_INDEX_sT media_index;

    if (handle == NULL)
    {
        printf("open file error!\r\n");
        return 0;
    }
    length_of_ulong    = sizeof(uint32_t);
    length_of_index_st = sizeof(HANDLER_MEDIA_INDEX_sT);

    file_offset = offset;
    // read list name
    fseek(handle, file_offset, SEEK_SET);
    fread((void *)&loadAtomName, 1, length_of_ulong, handle);

    // read header size
    file_offset += 4;
    fread((void *)&size, 1, length_of_ulong, handle);
    loadAtomSize = size;
    if (memcmp("idx1", loadAtomName, 4) == 0)
    {
        while (loadAtomSize > 0)
        {
            fread((void *)&media_index, 1, length_of_index_st, handle);
            memcpy(media_list[i].ckid, media_index.ckid, 4);
            media_list[i].flags  = media_index.flags & 0xFF;
            media_list[i].offset = media_index.offset + movi_offset + 4;
            media_list[i].length = media_index.length;

            memset((void *)&media_index, 0x00, length_of_index_st);
            loadAtomSize -= length_of_index_st;
            i++;
        }
    }

    return 0;
}

uint32_t read_movie_info(FILE *handle, uint32_t offset)
{
    uint32_t size, loadAtomSize, length_of_ulong;
    uint32_t file_offset;
    uint8_t  loadAtomName[4] = { 0x00 };

    if (handle == NULL)
    {
        printf("open file error!\r\n");
        return 0;
    }
    length_of_ulong = sizeof(uint32_t);

    file_offset = offset;
    // read list name
    fseek(handle, file_offset, SEEK_SET);
    fread((void *)&loadAtomName, 1, length_of_ulong, handle);
    if (memcmp("hdrl", loadAtomName, 4) == 0)
    {
        //next atom
        file_offset += 4;
        fseek(handle, file_offset, SEEK_SET);
        fread((void *)&loadAtomName, 1, length_of_ulong, handle);

        // read header size
        file_offset += 4;
        fread((void *)&size, 1, length_of_ulong, handle);
        loadAtomSize = size;
        UNUSED(loadAtomSize);
        if (memcmp("avih", loadAtomName, 4) == 0)
        {
            fread((void *)&avih_info, 1, sizeof(AVIH_ATOM_INFO_sT), handle);
        }
    }
    return 0;
}

long parser_atom_info(FILE *handle)
{
    uint32_t next_offset_ptr, movi_offset;
    uint32_t atom_length;

    if (handle == NULL)
    {
        return -1;
    }

    next_offset_ptr  = 0;
    atom_length      = 0;
    next_offset_ptr += sizeof(HANDLER_FILE_ATOM_sT);

    //
    next_offset_ptr = seek_movie_file_in_list("hdrl", handle, next_offset_ptr, &atom_length);
    if (next_offset_ptr == 0)
    {
        printf("Parse atom hdrl is not found! \r\n");
        return -2;
    }

    printf("(%s)(%d)get %s at 0x%08x, length = %d \r\n", __FUNCTION__, __LINE__, "hdrl", next_offset_ptr, atom_length);
    read_movie_info(handle, next_offset_ptr);
    //
    next_offset_ptr += atom_length;
    next_offset_ptr  = seek_movie_file_in_list("movi", handle, next_offset_ptr, &atom_length);
    if (next_offset_ptr == 0)
    {
        printf("Parse atom movi is not found! \r\n");
        return -2;
    }
    printf("(%s)(%d)get %s at 0x%08x, length = %d \r\n", __FUNCTION__, __LINE__, "movi", next_offset_ptr, atom_length);

    movi_offset      = next_offset_ptr;
    next_offset_ptr += atom_length;
    next_offset_ptr  = read_movie_index(handle, next_offset_ptr, movi_offset);
    if (next_offset_ptr != 0)
    {
        printf("Parse atom idx1 is not found! \r\n");
        return -2;
    }

    return 0;
}


uint32_t read_movie_image(FILE *handle, uint32_t offset)
{
    uint32_t size, loadAtomSize;
    uint32_t file_offset;

    if (handle == NULL)
    {
        printf("open file error!\r\n");
        return 0;
    }

    // read image size
    file_offset = offset;
    fseek(handle, file_offset, SEEK_SET);
    fread((void *)&size, 1, sizeof(uint32_t), handle);
    loadAtomSize = size;

    file_offset += 4;
    fseek(handle, file_offset, SEEK_SET);
    fread((void *)&loadImage, 1, loadAtomSize, handle);
    if (memcmp("\xFF\xD8", loadImage, 2) == 0)
    {
        printf("image[0x%02x] = 0x%02x, image[0x%02x] = 0x%02x, image[0x%02x] = 0x%02x, image[0x%02x] = 0x%02x\r\n",
               file_offset, loadImage[0] & 0xFF,
               file_offset + 1, loadImage[1] & 0xFF,
               file_offset + loadAtomSize - 2, loadImage[loadAtomSize - 2] & 0xFF,
               file_offset + loadAtomSize - 1, loadImage[loadAtomSize - 1] & 0xFF);
    }
    return 0;
}

int read_avinfo(const char *file)
{
    FILE   * pFile;
    uint32_t i = 0, count = 0;

    pFile = fopen(file, "rb");
    if (pFile == NULL)
    {
        printf("File error\r\n");
        return -1;
    }

    parser_atom_info(pFile);

    printf("(%s)(%d)width = %d\r\n", __FUNCTION__, __LINE__, avih_info.width);
    printf("(%s)(%d)height = %d\r\n", __FUNCTION__, __LINE__, avih_info.height);
    printf("(%s)(%d)frames = %d\r\n", __FUNCTION__, __LINE__, avih_info.frames);   // number of jpeg frames in file
    printf("(%s)(%d)streams = %d\r\n", __FUNCTION__, __LINE__, avih_info.streams); // stream : 1, only video
    // stream : 2, video & audio channel
    while (count < avih_info.frames)
    {
        if (memcmp("dc", media_list[i].ckid+2, 2) == 0)
        {
            //printf("index[%d]: flags = %d, length = %d, offset = 0x%08X\r\n", i, media_list[i].flags, media_list[i].length, media_list[i].offset);
            read_movie_image(pFile, media_list[i].offset);
            count++;
        }
        i++;
    }
    // terminate
    fclose(pFile);

    return 0;
}

int main()
{
    read_avinfo("test1.avi");
    return 0;
}
