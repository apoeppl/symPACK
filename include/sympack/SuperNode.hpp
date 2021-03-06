/*
	 Copyright (c) 2016 The Regents of the University of California,
	 through Lawrence Berkeley National Laboratory.  

   Author: Mathias Jacquelin
	 
   This file is part of symPACK. All rights reserved.

	 Redistribution and use in source and binary forms, with or without
	 modification, are permitted provided that the following conditions are met:

	 (1) Redistributions of source code must retain the above copyright notice, this
	 list of conditions and the following disclaimer.
	 (2) Redistributions in binary form must reproduce the above copyright notice,
	 this list of conditions and the following disclaimer in the documentation
	 and/or other materials provided with the distribution.
	 (3) Neither the name of the University of California, Lawrence Berkeley
	 National Laboratory, U.S. Dept. of Energy nor the names of its contributors may
	 be used to endorse or promote products derived from this software without
	 specific prior written permission.

	 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
	 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
	 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
	 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
	 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
	 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
	 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
	 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

	 You are under no obligation whatsoever to provide any bug fixes, patches, or
	 upgrades to the features, functionality or performance of the source code
	 ("Enhancements") to anyone; however, if you choose to make your Enhancements
	 available either publicly, or directly to Lawrence Berkeley National
	 Laboratory, without imposing a separate written license agreement for such
	 Enhancements, then you hereby grant the following license: a non-exclusive,
	 royalty-free perpetual license to install, use, modify, prepare derivative
	 works, incorporate into other computer software, distribute, and sublicense
	 such enhancements or derivative works thereof, in binary and source code form.
*/
/// @file SuperNode.hpp
/// @brief TODO
/// @author Mathias Jacquelin
/// @date 2013-09-10
#ifndef _SUPERNODE_FACT_HPP_
#define _SUPERNODE_FACT_HPP_

#include "sympack/Environment.hpp"
#include "sympack/blas.hpp"
#include "sympack/lapack.hpp"
//#include "sympack/NumMat.hpp"
#include "sympack/IntervalTree.hpp"
#include "sympack/CommTypes.hpp"

#include <upcxx.h>
#include <list>

#ifdef NO_INTRA_PROFILE
#if defined (SPROFILE)
#define SYMPACK_TIMER_START(a) 
#define SYMPACK_TIMER_STOP(a) 
#define scope_timer(b,a)
#endif
#endif


//#define _INDEFINITE_

namespace symPACK{

struct NZBlockDesc{
    bool Last;
    Int GIndex;
    size_t Offset;
    NZBlockDesc():GIndex(-1),Offset(-1),Last(false){};
    NZBlockDesc(Int aGIndex, size_t aOffset):GIndex(aGIndex),Offset(aOffset),Last(false){};
};



class MemoryAllocator{
  protected:
#ifdef _TRACK_MEMORY_
    static std::map<char*,size_t> cnt_;
    static size_t total_;
    static size_t hwm_;
#endif

  public:
#ifdef _TRACK_MEMORY_
    static void printStats(){
logfileptr->OFS()<<"Memory HWM: "<<hwm_<<std::endl;
    }
#endif

    static char * allocate(size_t count){};

    static void deallocate(char* ptr) {};
};


class MallocAllocator: public MemoryAllocator{
  public:
    static char * allocate(size_t count){
      char * locTmpPtr = new char[count];
#ifdef _TRACK_MEMORY_
      if(cnt_.size()==0){total_ = 0;}
      cnt_[locTmpPtr] = count;
      total_ += count;
      hwm_ = std::max(hwm_,total_);
//      logfileptr->OFS()<<"Allocating "<<" "<<count<<" bytes at "<<(uint64_t)locTmpPtr<<", total "<< total_<<std::endl;
#endif
      return locTmpPtr;
    }

    static void deallocate(char* ptr){
#ifdef _TRACK_MEMORY_
      total_-=cnt_[ptr];
      //logfileptr->OFS()<<"Deallocating "<<(uint64_t)ptr<<" "<<cnt_[ptr]<<" bytes, total "<< total_<< std::endl;
      cnt_.erase(ptr);
#endif
      delete [] ptr;
    }
};





class UpcxxAllocator: public MemoryAllocator{
  public:
    static char * allocate(size_t count){
      upcxx::global_ptr<char> tmpPtr = upcxx::allocate<char>(upcxx::myrank(),count);
      char * locTmpPtr = (char*)tmpPtr;
#ifdef _TRACK_MEMORY_
      if(cnt_.size()==0){total_ = 0;}
      cnt_[locTmpPtr] = count;
      total_ += count;
      hwm_ = std::max(hwm_,total_);
      //logfileptr->OFS()<<"Allocating UPCXX "<<" "<<count<<" bytes at "<<(uint64_t)locTmpPtr<<", total "<< total_<<std::endl;
#endif
      return locTmpPtr;
    }

    static void deallocate(char* ptr){
#ifdef _TRACK_MEMORY_
      total_-=cnt_[ptr];
      //logfileptr->OFS()<<"Deallocating UPCXX "<<(uint64_t)ptr<<" "<<cnt_[ptr]<<" bytes, total "<< total_<<std::endl;
      cnt_.erase(ptr);
#endif
      upcxx::global_ptr<char> tmpPtr((char*)ptr);
      upcxx::deallocate(tmpPtr);
    }
};


    struct SuperNodeDesc{
      bool b_own_storage_;
      Int iId_;
      Int iSize_;
      Int iFirstCol_;
      Int iLastCol_;
      Int iN_; 
      Int blocks_cnt_;
      Int nzval_cnt_;
      //Ptr nzval_cnt_;
    };



////////////////////////////////////////
/// Class representing a supernode.
/// Class representing a supernode stored as a collection of 
/// blocks of contiguous rows in row-major format.
/////////////////////////////////////////
template<typename T, class Allocator = UpcxxAllocator>
class SuperNode{
  public:

  protected:

#ifndef ITREE
  Int iLastRow_;
  std::vector<Int> * globalToLocal_;
#else
  ITree * idxToBlk_;
#endif

  //actual storage
  //std::vector<char> storage_container_;
  //upcxx::global_ptr<char> storage_container_;
  char * storage_container_;
  char * loc_storage_container_;
  size_t storage_size_;
  
  //utility pointers
  SuperNodeDesc * meta_;
  T * nzval_;
  NZBlockDesc * blocks_;
  

  protected:
  inline ITree * CreateITree();

  public:

  inline void InitIdxToBlk();

  inline Int & Id(){ return meta_->iId_;}
  inline Int FirstCol(){ return meta_->iFirstCol_;}
  inline Int LastCol(){ return meta_->iLastCol_;}
  inline Int Size(){ return meta_->iSize_;}
  inline Int N(){ return meta_->iN_;}
  inline Ptr NNZ(){ return meta_->nzval_cnt_;}
  inline Int NZBlockCnt(){ return meta_->blocks_cnt_;}
  inline NZBlockDesc & GetNZBlockDesc(Int aiLocIndex){ return *(blocks_ -aiLocIndex);}
  inline T* GetNZval(size_t offset){ return &nzval_[offset];}
  inline SuperNodeDesc * GetMeta(){return meta_;}
  inline Int NRows(Int blkidx){
      NZBlockDesc & desc = GetNZBlockDesc(blkidx);
      size_t end = (blkidx<NZBlockCnt()-1)?GetNZBlockDesc(blkidx+1).Offset:meta_->nzval_cnt_;
      Int nRows = (end-desc.Offset)/Size();
      return nRows;
  }

  inline Int NRowsBelowBlock(Int blkidx){
      NZBlockDesc & desc = GetNZBlockDesc(blkidx);
      size_t end = meta_->nzval_cnt_;
      Int nRows = (end-desc.Offset)/Size();
      return nRows;
  }


  inline Int StorageSize(){ return storage_size_;}

//  upcxx::global_ptr<char> GetGlobalPtr(Int row){
//    //TODO fix this
//    return storage_container_;
//  }
  
  char * GetStoragePtr(Int row){
    //TODO fix this
    return loc_storage_container_;
  }
  
  virtual inline void clear(){ 
    if(NNZ()>0){
      std::fill(nzval_,nzval_+NNZ(),T(0));
    }
  }

  SuperNode();
  SuperNode(Int aiId, Int aiFc, Int aiLc, Int aiN, std::set<Idx> & rowIndices);
  SuperNode(Int aiId, Int aiFc, Int aiLc, Int ai_num_rows, Int aiN, Int aiNZBlkCnt=-1);
  SuperNode(Int aiId, Int aiFc, Int aiLc, Int aiN);
  SuperNode(char * storage_ptr,size_t storage_size, Int GIndex = -1);
  ~SuperNode();

  virtual void Init(char * storage_ptr,size_t storage_size, Int GIndex = -1);

  virtual inline void AddNZBlock(Int aiNRows, Int aiNCols, Int aiGIndex);
  inline void Reserve(size_t storage_size);

  inline Int FindBlockIdx(Int aiGIndex);
  inline Int FindBlockIdx(Int aiGIndex,Int & closestR, Int & closestL);
  inline Int FindBlockIdx(Int fr, Int lr, ITree::Interval & overlap);

  inline void DumpITree();
  virtual inline Int Shrink();
  
#ifdef ITREE
  inline bool ITreeInitialized(){return idxToBlk_->StorageSize()!=0;};
#endif

  inline void FindUpdatedFirstCol(SuperNode<T,Allocator> * src_snode, Int pivot_fr, Int & tgt_fc, Int & first_pivot_idx);
  inline void FindUpdatedLastCol(SuperNode<T,Allocator> * src_snode, Int tgt_fc, Int first_pivot_idx, Int & tgt_lc, Int & last_pivot_idx);

virtual inline Int Aggregate(SuperNode<T,Allocator> * src_snode);
  //this function merge structure of src_snode into the structure of the current supernode
  //right now the destination will have a pretty stupid one line per block structure
 virtual inline Int Merge(SuperNode<T,Allocator> * src_snode, SnodeUpdate &update);

  //Update an Aggregate
 virtual inline Int UpdateAggregate(SuperNode<T,Allocator> * src_snode, SnodeUpdate &update, 
              TempUpdateBuffers<T> & tmpBuffers,Int iTarget, Int iam);

  //Update from a factor
 virtual inline Int Update(SuperNode<T,Allocator> * src_snode, SnodeUpdate &update, 
              TempUpdateBuffers<T> & tmpBuffers);

  //Factorize the supernode
  virtual inline Int Factorize(TempUpdateBuffers<T> & tmpBuffers);
  inline bool FindNextUpdate(SnodeUpdate & nextUpdate, const std::vector<Int> & Xsuper,  const std::vector<Int> & SupMembership,bool isLocal=true); 


  //forward and backward solve phases
  virtual inline void forward_update(SuperNode<T,Allocator> * src_contrib,Int iOwner,Int iam);
  virtual inline void forward_update_contrib( T * RHS, SuperNode<T> * cur_snode, std::vector<Int> & perm);
  virtual inline void back_update(SuperNode<T,Allocator> * src_contrib);
  virtual inline void back_update_contrib(SuperNode<T> * cur_snode);


 virtual inline void Serialize(Icomm & buffer, Int first_blkidx=0, Idx first_row=0);
 virtual inline size_t Deserialize(char * buffer, size_t size);






};



template <typename T, class Allocator> inline void Serialize(Icomm & buffer,SuperNode<T,Allocator> & snode, Int first_blkidx=0, Idx first_row=0);
template <typename T, class Allocator> inline std::ostream& operator<<( std::ostream& os,  SuperNode<T,Allocator>& snode);
template <typename T, class Allocator> inline size_t Deserialize(char * buffer, size_t size, SuperNode<T,Allocator> & snode);







inline std::ostream& operator<<( std::ostream& os,  SuperNodeDesc& desc);
inline std::ostream& operator<<( std::ostream& os,  NZBlockDesc& block);




} // namespace SYMPACK


#include "sympack/SuperNode_impl.hpp"




#ifdef NO_INTRA_PROFILE
#if defined (SPROFILE)
#define SYMPACK_TIMER_START(a) SYMPACK_FSTART(a);
#define SYMPACK_TIMER_STOP(a) SYMPACK_FSTOP(a);
#define scope_timer(b,a) symPACK_scope_timer b(#a)
#endif
#endif



#endif // _SUPERNODE_FACT_HPP_
