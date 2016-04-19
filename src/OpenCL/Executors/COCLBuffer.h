/*
 * ------------------------------------------
 *
 *  HIGH-PERFORMANCE INTEGRATED MODELLING SYSTEM (HiPIMS)
 *  Luke S. Smith and Qiuhua Liang
 *  luke@smith.ac
 *
 *  School of Civil Engineering & Geosciences
 *  Newcastle University
 * 
 * ------------------------------------------
 *  This code is licensed under GPLv3. See LICENCE
 *  for more information.
 * ------------------------------------------
 *  OpenCL memory buffer
 * ------------------------------------------
 *
 */

#ifndef HIPIMS_OPENCL_COCLBUFFER_H
#define HIPIMS_OPENCL_COCLBUFFER_H

#include <boost/lexical_cast.hpp>

class COCLProgram;
class COCLDevice;
class COCLBuffer
{
public:
	COCLBuffer( std::string, COCLProgram*, bool Read_Only = false, bool Exists_On_Host = true, cl_ulong Length_In_Bytes = NULL, bool Allocate_Now = false );
	~COCLBuffer();
	std::string		getName()							{ return sName; }
	cl_mem			getBuffer()							{ return clBuffer; }
	cl_ulong		getSize()							{ return ulSize; }
	bool			isReady()							{ return bReady; }
	void			setCallbackRead( void (__stdcall * cb)( cl_event, cl_int, void* ) )
														{ fCallbackRead = cb; }
	void			setCallbackWrite( void (__stdcall * cb)( cl_event, cl_int, void* ) )
														{ fCallbackWrite = cb; }
	template <typename blockType>
	blockType		getHostBlock()						{ return static_cast<blockType>( this->pHostBlock ); }
	bool			createBuffer();
	bool			createBufferAndInitialise();
	void			setPointer( void*, cl_ulong );
	void			allocateHostBlock( cl_ulong );
	void			queueReadAll();
	void			queueReadPartial( cl_ulong, size_t, void* = NULL );
	void			queueWriteAll();
	void			queueWritePartial( cl_ulong, size_t, void* = NULL );

protected:
	cl_uint			uiDeviceID;
	std::string		sName;
	cl_mem_flags	clFlags;
	cl_context		clContext;
	cl_command_queue clQueue;
	cl_mem			clBuffer;
	void*			pHostBlock;
	COCLDevice*		pDevice;
	cl_ulong		ulSize;
	bool			bReady;
	bool			bInternalBlock;
	bool			bReadOnly;
	bool			bExistsOnHost;
	void (__stdcall *fCallbackRead)( cl_event, cl_int, void* );
	void (__stdcall *fCallbackWrite)( cl_event, cl_int, void* );
};

#endif
