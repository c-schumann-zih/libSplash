/**
 * Copyright 2013 Felix Schmitt
 *
 * This file is part of libSplash. 
 * 
 * libSplash is free software: you can redistribute it and/or modify 
 * it under the terms of of either the GNU General Public License or 
 * the GNU Lesser General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, or 
 * (at your option) any later version. 
 * libSplash is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License and the GNU Lesser General Public License 
 * for more details. 
 * 
 * You should have received a copy of the GNU General Public License 
 * and the GNU Lesser General Public License along with libSplash. 
 * If not, see <http://www.gnu.org/licenses/>. 
 */

#include <assert.h>
#include <mpi/mpi.h>

#include "ParallelDataCollector.hpp"
#include "DCParallelDataSet.hpp"
#include "DCAttribute.hpp"

using namespace DCollector;

/*******************************************************************************
 * PRIVATE FUNCTIONS
 *******************************************************************************/

void ParallelDataCollector::setFileAccessParams(hid_t& fileAccProperties)
{
    fileAccProperties = H5Pcreate(H5P_FILE_ACCESS);
    H5Pset_fapl_mpio(fileAccProperties, options.mpiComm, options.mpiInfo);

    int metaCacheElements = 0;
    size_t rawCacheElements = 0;
    size_t rawCacheSize = 0;
    double policy = 0.0;

    // set new cache size
    H5Pget_cache(fileAccProperties, &metaCacheElements, &rawCacheElements, &rawCacheSize, &policy);
    rawCacheSize = 64 * 1024 * 1024;
    H5Pset_cache(fileAccProperties, metaCacheElements, rawCacheElements, rawCacheSize, policy);

    //H5Pset_chunk_cache(0, H5D_CHUNK_CACHE_NSLOTS_DEFAULT, H5D_CHUNK_CACHE_NBYTES_DEFAULT, H5D_CHUNK_CACHE_W0_DEFAULT);

#if defined SDC_DEBUG_OUTPUT
    std::cerr << "Raw Data Cache = " << rawCacheSize / 1024 << " KiB" << std::endl;
#endif
}

std::string ParallelDataCollector::getFullFilename(uint32_t id, std::string baseFilename)
{
    std::stringstream serial_filename;
    serial_filename << baseFilename << "_" << id << ".h5";

    return serial_filename.str();
}

std::string ParallelDataCollector::getExceptionString(std::string func, std::string msg,
        const char *info)
{
    std::stringstream full_msg;
    full_msg << "Exception for ParallelDataCollector::" << func <<
            ": " << msg;

    if (info != NULL)
        full_msg << " (" << info << ")";

    return full_msg.str();
}

void ParallelDataCollector::indexToPos(int index, Dimensions mpiSize, Dimensions &mpiPos)
{
    /*mpiPos[2] = index % mpiSize[2];
    mpiPos[1] = (index / mpiSize[2]) % mpiSize[1];
    mpiPos[0] = index / (mpiSize[1] * mpiSize[2]);*/

    mpiPos[2] = index / (mpiSize[0] * mpiSize[1]);
    mpiPos[1] = (index % (mpiSize[0] * mpiSize[1])) / mpiSize[0];
    mpiPos[0] = index % mpiSize[0];
}

/*******************************************************************************
 * PUBLIC FUNCTIONS
 *******************************************************************************/

ParallelDataCollector::ParallelDataCollector(MPI_Comm comm, MPI_Info info,
        const Dimensions topology, int mpiRank, uint32_t maxFileHandles) :
handles(maxFileHandles, HandleMgr::FNS_ITERATIONS),
fileStatus(FST_CLOSED)
{
    options.enableCompression = false;
    options.mpiComm = comm;
    options.mpiInfo = info;
    options.mpiRank = mpiRank;
    options.mpiSize = topology.getDimSize();
    options.mpiTopology.set(topology);
    options.maxID = -1;

    if (H5open() < 0)
        throw DCException(getExceptionString("ParallelDataCollector",
            "failed to initialize/open HDF5 library"));

    // surpress automatic output of HDF5 exception messages
    if (H5Eset_auto2(H5E_DEFAULT, NULL, NULL) < 0)
        throw DCException(getExceptionString("ParallelDataCollector",
            "failed to disable error printing"));

    // set some default file access parameters
    setFileAccessParams(fileAccProperties);

    handles.registerFileCreate(fileCreateCallback, &options);
    handles.registerFileOpen(fileOpenCallback, &options);

    indexToPos(mpiRank, options.mpiTopology, options.mpiPos);
}

ParallelDataCollector::~ParallelDataCollector()
{
    H5Pclose(fileAccProperties);
}

void ParallelDataCollector::open(const char* filename, FileCreationAttr &attr)
throw (DCException)
{
    if (filename == NULL)
        throw DCException(getExceptionString("open", "filename must not be null"));

    if (fileStatus != FST_CLOSED)
        throw DCException(getExceptionString("open", "this access is not permitted"));

    switch (attr.fileAccType)
    {
        case FAT_READ:
        case FAT_READ_MERGED:
            openRead(filename, attr);
            break;
        case FAT_WRITE:
            openWrite(filename, attr);
            break;
        case FAT_CREATE:
            openCreate(filename, attr);
            break;
    }
}

void ParallelDataCollector::close()
{
    // close opened hdf5 file handles
    handles.close();

    fileStatus = FST_CLOSED;
}

int32_t ParallelDataCollector::getMaxID()
{
    return options.maxID;
}

void ParallelDataCollector::getMPISize(Dimensions& mpiSize)
{
    mpiSize.set(options.mpiTopology);
}

void ParallelDataCollector::getEntryIDs(int32_t *ids, size_t *count)
throw (DCException)
{
    return;
}

void ParallelDataCollector::getEntriesForID(int32_t id, DCEntry *entries,
        size_t *count)
throw (DCException)
{
    throw DCException("Not yet implemented!");
}

void ParallelDataCollector::readGlobalAttribute(int32_t id,
        const char* name,
        void *data)
throw (DCException)
{
    if (name == NULL || data == NULL)
        throw DCException(getExceptionString("readGlobalAttribute", "a parameter was null"));

    if (fileStatus == FST_CLOSED || fileStatus == FST_CREATING)
        throw DCException(getExceptionString("readGlobalAttribute", "this access is not permitted"));

    hid_t group_custom = H5Gopen(handles.get(id), SDC_GROUP_CUSTOM, H5P_DEFAULT);
    if (group_custom < 0)
    {
        throw DCException(getExceptionString("readGlobalAttribute",
                "failed to open custom group", SDC_GROUP_CUSTOM));
    }

    try
    {
        DCAttribute::readAttribute(name, group_custom, data);
    } catch (DCException e)
    {
        throw DCException(getExceptionString("readGlobalAttribute", "failed to open attribute", name));
    }

    H5Gclose(group_custom);
}

void ParallelDataCollector::writeGlobalAttribute(int32_t id,
        const CollectionType& type,
        const char *name,
        const void* data)
throw (DCException)
{
    if (name == NULL || data == NULL)
        throw DCException(getExceptionString("writeGlobalAttribute", "a parameter was null"));

    if (fileStatus == FST_CLOSED || fileStatus == FST_READING)
        throw DCException(getExceptionString("writeGlobalAttribute", "this access is not permitted"));

    hid_t group_custom = H5Gopen(handles.get(id), SDC_GROUP_CUSTOM, H5P_DEFAULT);
    if (group_custom < 0)
        throw DCException(getExceptionString("writeGlobalAttribute",
            "custom group not found", SDC_GROUP_CUSTOM));

    try
    {
        DCAttribute::writeAttribute(name, type.getDataType(), group_custom, data);
    } catch (DCException e)
    {
        std::cerr << e.what() << std::endl;
        throw DCException(getExceptionString("writeGlobalAttribute", "failed to write attribute", name));
    }

    H5Gclose(group_custom);
}

void ParallelDataCollector::readAttribute(int32_t id,
        const char *dataName,
        const char *attrName,
        void* data,
        Dimensions *mpiPosition)
throw (DCException)
{
    // mpiPosition is ignored
    if (attrName == NULL || dataName == NULL || data == NULL)
        throw DCException(getExceptionString("readAttribute", "a parameter was null"));

    if (fileStatus == FST_CLOSED || fileStatus == FST_CREATING)
        throw DCException(getExceptionString("readAttribute", "this access is not permitted"));

    std::stringstream group_id_name;
    group_id_name << SDC_GROUP_DATA << "/" << id;
    std::string group_id_string = group_id_name.str();

    hid_t group_id = H5Gopen(handles.get(id), group_id_string.c_str(), H5P_DEFAULT);
    if (group_id < 0)
        throw DCException(getExceptionString("readAttribute", "group not found",
            group_id_string.c_str()));

    hid_t dataset_id = -1;
    dataset_id = H5Dopen(group_id, dataName, H5P_DEFAULT);
    if (dataset_id < 0)
    {
        H5Gclose(group_id);
        throw DCException(getExceptionString("readAttribute", "dataset not found", dataName));
    }

    try
    {
        DCAttribute::readAttribute(attrName, dataset_id, data);
    } catch (DCException)
    {
        H5Dclose(dataset_id);
        H5Gclose(group_id);
        throw;
    }
    H5Dclose(dataset_id);

    // cleanup
    H5Gclose(group_id);
}

void ParallelDataCollector::writeAttribute(int32_t id,
        const CollectionType& type,
        const char *dataName,
        const char *attrName,
        const void* data)
throw (DCException)
{
    if (attrName == NULL || dataName == NULL || data == NULL)
        throw DCException(getExceptionString("writeAttribute", "a parameter was null"));

    if (fileStatus == FST_CLOSED || fileStatus == FST_READING)
        throw DCException(getExceptionString("writeAttribute", "this access is not permitted"));

    std::stringstream group_id_name;
    group_id_name << SDC_GROUP_DATA << "/" << id;
    std::string group_id_string = group_id_name.str();

    hid_t group_id = H5Gopen(handles.get(id), group_id_string.c_str(), H5P_DEFAULT);
    if (group_id < 0)
    {
        throw DCException(getExceptionString("writeAttribute", "group not found",
                group_id_string.c_str()));
    }

    hid_t dataset_id = H5Dopen(group_id, dataName, H5P_DEFAULT);
    if (dataset_id < 0)
    {
        H5Gclose(group_id);
        throw DCException(getExceptionString("writeAttribute", "dataset not found", dataName));
    }

    try
    {
        DCAttribute::writeAttribute(attrName, type.getDataType(), dataset_id, data);
    } catch (DCException)
    {
        H5Dclose(dataset_id);
        H5Gclose(group_id);
        throw;
    }
    H5Dclose(dataset_id);

    // cleanup
    H5Gclose(group_id);
}

void ParallelDataCollector::read(int32_t id,
        const CollectionType& type,
        const char* name,
        Dimensions &sizeRead,
        void* data)
throw (DCException)
{
    this->read(id, type, name, Dimensions(0, 0, 0), sizeRead, Dimensions(0, 0, 0), data);
}

void ParallelDataCollector::read(int32_t id,
        const CollectionType& type,
        const char* name,
        Dimensions dstBuffer,
        Dimensions &sizeRead,
        Dimensions dstOffset,
        void* data)
throw (DCException)
{
    throw DCException("Not yet implemented!");
}

void ParallelDataCollector::write(int32_t id, const CollectionType& type, uint32_t rank,
        const Dimensions srcData, const char* name, const void* data)
throw (DCException)
{
    write(id, srcData * options.mpiTopology, srcData * options.mpiPos,
            type, rank, srcData, Dimensions(1, 1, 1),
            srcData, Dimensions(0, 0, 0), name, data);
}

void ParallelDataCollector::write(int32_t id, const CollectionType& type, uint32_t rank,
        const Dimensions srcBuffer, const Dimensions srcData, const Dimensions srcOffset,
        const char* name, const void* data)
throw (DCException)
{
    write(id, srcData * options.mpiTopology, srcData * options.mpiPos,
            type, rank, srcBuffer, Dimensions(1, 1, 1),
            srcData, srcOffset, name, data);
}

void ParallelDataCollector::write(int32_t id, const CollectionType& type, uint32_t rank,
        const Dimensions srcBuffer, const Dimensions srcStride, const Dimensions srcData,
        const Dimensions srcOffset, const char* name, const void* data)
throw (DCException)
{
    write(id, srcData * options.mpiTopology, srcData * options.mpiPos,
            type, rank, srcBuffer, srcStride, srcData,
            srcOffset, name, data);
}

void ParallelDataCollector::write(int32_t id, const Dimensions globalSize,
        const Dimensions globalOffset,
        const CollectionType& type, uint32_t rank, const Dimensions srcData,
        const char* name, const void* data)
{
    write(id, globalSize, globalOffset, type, rank, srcData, Dimensions(1, 1, 1), srcData,
            Dimensions(0, 0, 0), name, data);
}

void ParallelDataCollector::write(int32_t id, const Dimensions globalSize,
        const Dimensions globalOffset,
        const CollectionType& type, uint32_t rank, const Dimensions srcBuffer,
        const Dimensions srcData, const Dimensions srcOffset, const char* name,
        const void* data)
{
    write(id, globalSize, globalOffset, type, rank, srcBuffer, Dimensions(1, 1, 1),
            srcData, srcOffset, name, data);
}

void ParallelDataCollector::write(int32_t id, const Dimensions globalSize,
        const Dimensions globalOffset,
        const CollectionType& type, uint32_t rank, const Dimensions srcBuffer,
        const Dimensions srcStride, const Dimensions srcData,
        const Dimensions srcOffset, const char* name, const void* data)
{
    if (name == NULL || data == NULL)
        throw DCException(getExceptionString("write", "a parameter was NULL"));

    if (fileStatus == FST_CLOSED || fileStatus == FST_READING)
        throw DCException(getExceptionString("write", "this access is not permitted"));

    if (rank < 1 || rank > 3)
        throw DCException(getExceptionString("write", "maximum dimension is invalid"));

    // create group for this id/iteration
    std::stringstream group_id_name;
    group_id_name << SDC_GROUP_DATA << "/" << id;

    hid_t group_id = -1;
    H5Handle file_handle = handles.get(id);

    group_id = H5Gopen(file_handle, group_id_name.str().c_str(), H5P_DEFAULT);
    if (group_id < 0)
        group_id = H5Gcreate(file_handle, group_id_name.str().c_str(), H5P_LINK_CREATE_DEFAULT,
            H5P_GROUP_CREATE_DEFAULT, H5P_GROUP_ACCESS_DEFAULT);

    if (group_id < 0)
        throw DCException(getExceptionString("write", "failed to open or create group"));

    // write data to the group
    try
    {
        writeDataSet(group_id, globalSize, globalOffset, type, rank,
                srcBuffer, srcStride, srcData, srcOffset, name, data);
    } catch (DCException)
    {
        H5Gclose(group_id);
        throw;
    }

    // cleanup
    H5Gclose(group_id);
}

void ParallelDataCollector::append(int32_t id, const CollectionType& type,
        uint32_t count, const char* name, const void* data)
throw (DCException)
{
    append(id, type, count, 0, 1, name, data);
}

void ParallelDataCollector::append(int32_t id, const CollectionType& type,
        uint32_t count, uint32_t offset, uint32_t stride, const char* name, const void* data)
throw (DCException)
{
    throw DCException("Not yet implemented!");
}

void ParallelDataCollector::remove(int32_t id)
throw (DCException)
{
    throw DCException("Not yet implemented!");
}

void ParallelDataCollector::remove(int32_t id, const char* name)
throw (DCException)
{
    throw DCException("Not yet implemented!");
}

void ParallelDataCollector::createReference(int32_t srcID,
        const char *srcName,
        const CollectionType& colType,
        int32_t dstID,
        const char *dstName,
        Dimensions count,
        Dimensions offset,
        Dimensions stride)
throw (DCException)
{
    throw DCException("Not yet implemented!");
}

/*******************************************************************************
 * PROTECTED FUNCTIONS
 *******************************************************************************/

void ParallelDataCollector::fileCreateCallback(H5Handle handle, uint32_t index, void *userData)
throw (DCException)
{
    Options *options = (Options*) userData;

    // the custom group holds user-specified attributes
    hid_t group_custom = H5Gcreate(handle, SDC_GROUP_CUSTOM, H5P_LINK_CREATE_DEFAULT,
            H5P_GROUP_CREATE_DEFAULT, H5P_GROUP_ACCESS_DEFAULT);
    if (group_custom < 0)
        throw DCException(getExceptionString("openCreate",
            "failed to create custom group", SDC_GROUP_CUSTOM));

    H5Gclose(group_custom);

    // the data group holds the actual simulation data
    hid_t group_data = H5Gcreate(handle, SDC_GROUP_DATA, H5P_LINK_CREATE_DEFAULT,
            H5P_GROUP_CREATE_DEFAULT, H5P_GROUP_ACCESS_DEFAULT);
    if (group_data < 0)
        throw DCException(getExceptionString("openCreate",
            "failed to create custom group", SDC_GROUP_DATA));

    H5Gclose(group_data);

    writeHeader(handle, index, options->enableCompression, options->mpiTopology);
}

void ParallelDataCollector::fileOpenCallback(H5Handle handle, uint32_t index, void *userData)
throw (DCException)
{
    Options *options = (Options*) userData;

    options->maxID = std::max(options->maxID, (int32_t)index);
}

void ParallelDataCollector::writeHeader(hid_t fHandle, uint32_t id,
        bool enableCompression, Dimensions mpiTopology)
throw (DCException)
{
    // create group for header information (position, grid size, ...)
    hid_t group_header = H5Gcreate(fHandle, SDC_GROUP_HEADER, H5P_LINK_CREATE_DEFAULT,
            H5P_GROUP_CREATE_DEFAULT, H5P_GROUP_ACCESS_DEFAULT);
    if (group_header < 0)
        throw DCException(getExceptionString("writeHeader",
            "Failed to create header group", NULL));

    try
    {
        ColTypeDim dim_t;

        int32_t index = id;

        // create master specific header attributes
        DCAttribute::writeAttribute(SDC_ATTR_MAX_ID, H5T_NATIVE_INT32,
                group_header, &index);

        DCAttribute::writeAttribute(SDC_ATTR_COMPRESSION, H5T_NATIVE_HBOOL,
                group_header, &enableCompression);

        DCAttribute::writeAttribute(SDC_ATTR_MPI_SIZE, dim_t.getDataType(),
                group_header, mpiTopology.getPointer());

    } catch (DCException attr_error)
    {
        throw DCException(getExceptionString("writeHeader",
                "Failed to write header attribute.",
                attr_error.what()));
    }

    H5Gclose(group_header);
}

void ParallelDataCollector::openCreate(const char *filename,
        FileCreationAttr& attr)
throw (DCException)
{
    this->fileStatus = FST_CREATING;
    this->options.enableCompression = attr.enableCompression;

#if defined SDC_DEBUG_OUTPUT
    if (attr.enableCompression)
        std::cerr << "compression is ON" << std::endl;
    else
        std::cerr << "compression is OFF" << std::endl;
#endif

    // open file
    handles.open(Dimensions(1, 1, 1), filename, fileAccProperties, H5F_ACC_TRUNC);
}

void ParallelDataCollector::openRead(const char* filename, FileCreationAttr& attr)
throw (DCException)
{
    this->fileStatus = FST_READING;

    handles.open(Dimensions(1, 1, 1), filename, fileAccProperties, H5F_ACC_RDONLY);
}

void ParallelDataCollector::openWrite(const char* filename, FileCreationAttr& attr)
throw (DCException)
{
    fileStatus = FST_WRITING;
    this->options.enableCompression = attr.enableCompression;

    handles.open(Dimensions(1, 1, 1), filename, fileAccProperties, H5F_ACC_RDWR);
}

void ParallelDataCollector::writeDataSet(hid_t &group, const Dimensions globalSize,
        const Dimensions globalOffset,
        const CollectionType& datatype, uint32_t rank,
        const Dimensions srcBuffer, const Dimensions srcStride,
        const Dimensions srcData, const Dimensions srcOffset,
        const char* name, const void* data) throw (DCException)
{
#if defined SDC_DEBUG_OUTPUT
    std::cerr << "# ParallelDataCollector::writeDataSet #" << std::endl;
#endif
    DCParallelDataSet dataset(name);
    // always create dataset but write data only if all dimensions > 0
    dataset.create(datatype, group, globalSize, rank, this->options.enableCompression);
    if (srcData.getDimSize() > 0)
        dataset.write(srcBuffer, srcStride, srcOffset, srcData, globalOffset, data);
    dataset.close();
}
