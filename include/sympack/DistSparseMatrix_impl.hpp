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
/// @file sparse_matrix_impl.hpp
/// @brief Sparse matrix and Distributed sparse matrix in compressed
/// column format.
/// @author Lin Lin
/// @date 2012-11-10
#ifndef _DIST_SPARSE_MATRIX_IMPL_HPP_
#define _DIST_SPARSE_MATRIX_IMPL_HPP_

#include "sympack/Environment.hpp"
#include "sympack/utility.hpp"
#include "sympack/DistSparseMatrix.hpp"

#include <vector>

namespace symPACK{


  template <typename T, typename Compare> std::vector<std::size_t> & sort_permutation( const std::vector<T>& vec, Compare compare, std::vector<std::size_t>& p);
  template <typename T, typename Compare> std::vector<std::size_t> & sort_permutation( const T*begin, const T* end, Compare compare, std::vector<std::size_t>& p);
  template <typename T, typename Compare> std::vector<std::size_t> sort_permutation( const std::vector<T>& vec, Compare compare);
  template <typename T, typename Compare> std::vector<std::size_t> sort_permutation( const T*begin, const T* end, Compare compare);
  template <typename T> std::vector<T> apply_permutation( const std::vector<T>& vec, const std::vector<std::size_t>& p);
  template <typename T> std::vector<T> apply_permutation( const T*vec, const std::vector<std::size_t>& p);
  template <typename T> void apply_permutation( T*begin, T* end, const std::vector<std::size_t>& p);
  template <typename T> std::vector<T> & apply_permutation( const std::vector<T>& vec, const std::vector<std::size_t>& p, std::vector<T> &sorted_vec);
  template <typename T> std::vector<T> & apply_permutation( const T*vec, const std::vector<std::size_t>& p, std::vector<T> &sorted_vec);
  template <typename T> void apply_permutation( T*begin, T* end, const std::vector<std::size_t>& p, std::vector<T> &sorted_vec);

  void PtrSum( void *in, void *inout, int *len, MPI_Datatype *dptr ); 
  void PtrMax( void *in, void *inout, int *len, MPI_Datatype *dptr );





  template <class F> const DistSparseMatrixGraph &  DistSparseMatrix<F>::GetLocalGraph() const {
    return Localg_;
  }

  template <class F> DistSparseMatrixGraph &  DistSparseMatrix<F>::GetLocalGraph() {
    return Localg_;
  }

  template <class F> void DistSparseMatrix<F>::SetLocalGraph(const DistSparseMatrixGraph & pgraph){
    Localg_ = pgraph;
  }


  template <class F>
    DistSparseMatrix<F>::DistSparseMatrix(){
      comm = MPI_COMM_NULL;
      size = 0;
      nnz=0;
    }

  template <class F>
    DistSparseMatrix<F>::DistSparseMatrix(MPI_Comm aComm):DistSparseMatrix(){
      comm = aComm;
    }

  template <class F> void DistSparseMatrix<F>::Dump() const{
    Int baseval = Localg_.baseval;
    Idx firstLocCol = Localg_.LocalFirstVertex()-Localg_.baseval;
    Idx LocalVertexCount = Localg_.LocalVertexCount();

    for(Idx locCol = 0 ; locCol< LocalVertexCount; locCol++){
      Idx col = locCol + firstLocCol;
      Ptr colbeg = Localg_.colptr[locCol]-baseval; //now 0 based
      Ptr colend = Localg_.colptr[locCol+1]-baseval; // now 0 based 

      logfileptr->OFS()<<col<<": ";
      for(Ptr pos = colbeg; pos<colend; pos++){
        Idx row = Localg_.rowind[pos];
        logfileptr->OFS()<<row<<" ";
      }
      logfileptr->OFS()<<std::endl;
    }
  }


  template <class F> void DistSparseMatrix<F>::DumpMatlab() const{
    Int baseval = Localg_.baseval;
    Idx firstLocCol = Localg_.LocalFirstVertex()-Localg_.baseval;
    Idx LocalVertexCount = Localg_.LocalVertexCount();

    //I
    logfileptr->OFS()<<" sparse([";
    for(Idx locCol = 0 ; locCol< LocalVertexCount; locCol++){
      Idx col = locCol + firstLocCol;
      Ptr colbeg = Localg_.colptr[locCol]-baseval; //now 0 based
      Ptr colend = Localg_.colptr[locCol+1]-baseval; // now 0 based 

      for(Ptr pos = colbeg; pos<colend; pos++){
        Idx row = Localg_.rowind[pos];
        logfileptr->OFS()<<row<<" ";
      }
    }
    logfileptr->OFS()<<"],";//<<std::endl;

    //J
    logfileptr->OFS()<<"[";
    for(Idx locCol = 0 ; locCol< LocalVertexCount; locCol++){
      Idx col = locCol + firstLocCol;
      Ptr colbeg = Localg_.colptr[locCol]-baseval; //now 0 based
      Ptr colend = Localg_.colptr[locCol+1]-baseval; // now 0 based 

      for(Ptr pos = colbeg; pos<colend; pos++){
        logfileptr->OFS()<<col+1<<" ";
      }
    }
    logfileptr->OFS()<<"],";//<<std::endl;

    //V
    logfileptr->OFS()<<"[";
    logfileptr->OFS().precision(std::numeric_limits< F >::max_digits10);
    for(Idx locCol = 0 ; locCol< LocalVertexCount; locCol++){
      Idx col = locCol + firstLocCol;
      Ptr colbeg = Localg_.colptr[locCol]-baseval; //now 0 based
      Ptr colend = Localg_.colptr[locCol+1]-baseval; // now 0 based 

      for(Ptr pos = colbeg; pos<colend; pos++){
        F val = nzvalLocal[pos];
        logfileptr->OFS()<<std::scientific<<ToMatlabScalar(val)<<" ";
      }
    }
    logfileptr->OFS()<<"]);"<<std::endl;

  }

  template< typename F>
    void DistSparseMatrix<F>::Permute(Int * invp, Idx * newVertexDist){
      scope_timer(a,DistMat_Permute);
      bool needPermute = !isPermuted(invp,newVertexDist);
      if (needPermute ){
        Int invpbaseval = 1;

        int mpisize,mpirank;
        MPI_Comm_size(comm,&mpisize);
        MPI_Comm_rank(comm,&mpirank);

        //vector<Ptr> tnewVertexDist;
        if(newVertexDist==NULL){
          //tnewVertexDist.resize(Localg_.vertexDist.size());
          //for(Int i =0; i<Localg_.vertexDist.size(); i++){
          //  tnewVertexDist[i] = Localg_.vertexDist[i];
          //}
          //newVertexDist=&tnewVertexDist[0];
          newVertexDist = &Localg_.vertexDist[0];
        }

        auto baseval = Localg_.baseval;

        auto & keepDiag = Localg_.keepDiag;
        auto & expanded = Localg_.expanded;
        auto & colptr = Localg_.colptr;
        auto & rowind = Localg_.rowind;

        Idx newFirstCol = newVertexDist[mpirank]-baseval; //0 based
        Idx newVtxCount = newVertexDist[mpirank+1] - newVertexDist[mpirank];


        std::vector<int> sizes(mpisize,0);

        Idx firstCol = Localg_.LocalFirstVertex()-baseval;
        Idx LocalVertexCount = Localg_.LocalVertexCount();

        for(Idx locCol = 0; locCol<LocalVertexCount; locCol++){
          Idx col = firstCol + locCol; //0-based
          Ptr colbeg = colptr[locCol] - baseval;
          Ptr colend = colptr[locCol+1] - baseval;
          Idx permCol = invp[col]-invpbaseval; // 0 based;
          //find destination processors
          Idx pdest; for(pdest = 0; pdest<mpisize; pdest++){ if(permCol>=newVertexDist[pdest]-baseval && permCol < newVertexDist[pdest+1]-baseval){ break;} }

          //now permute rows
          for(Ptr jptr = colbeg; jptr<colend; jptr++){
            Idx row = rowind[jptr] - baseval; //0 based
            Idx permRow = invp[row] - invpbaseval; // 0 based
            if(permRow<permCol && !expanded){
              Idx pdestR; for(pdestR = 0; pdestR<mpisize; pdestR++){ if(permRow>=newVertexDist[pdestR]-baseval && permRow < newVertexDist[pdestR+1]-baseval){ break;} }
              sizes[pdestR]++;
            }
            else if(permRow>permCol || (permRow==permCol && keepDiag) || expanded){
              sizes[pdest]++;
            }
          }
        }


        //First allgatherv to get the receive sizes
        std::vector<int> displs(mpisize+1,0);
        displs[0] = 0;
        std::partial_sum(sizes.begin(),sizes.end(),displs.begin()+1);
        Int totSend = displs.back();
        std::vector<triplet<F> > sbuf(totSend);

        //pack
        for(Idx locCol = 0; locCol<LocalVertexCount; locCol++){
          Idx col = firstCol + locCol; 
          Ptr colbeg = colptr[locCol]-baseval;
          Ptr colend = colptr[locCol+1]-baseval;
          Idx permCol = invp[col]-invpbaseval; // perm is 1 based;
          //find destination processors
          Idx pdest; for(pdest = 0; pdest<mpisize; pdest++){ if(permCol>=newVertexDist[pdest]-baseval && permCol < newVertexDist[pdest+1]-baseval){ break;} }


          for(Ptr jptr = colbeg; jptr<colend; jptr++){
            Idx row = rowind[jptr] - baseval; //0 based
            Idx permRow = invp[row] - invpbaseval; // 0 based
            if(permRow<permCol && !expanded){
              Idx pdestR; for(pdestR = 0; pdestR<mpisize; pdestR++){ if(permRow>=newVertexDist[pdestR]-baseval && permRow < newVertexDist[pdestR+1]-baseval){ break;} }
              auto & trip = sbuf[displs[pdestR]++];
              trip.val = nzvalLocal[jptr];
              trip.row = permCol;
              trip.col = permRow;
            }
            else if(permRow>permCol || (permRow==permCol && keepDiag) || expanded){
              auto & trip = sbuf[displs[pdest]++];
              trip.val = nzvalLocal[jptr];
              trip.row = permRow;
              trip.col = permCol;
            }

          }
        }

        MPI_Datatype type;
        MPI_Type_contiguous( sizeof(triplet<F>), MPI_BYTE, &type );
        MPI_Type_commit(&type);

        //re compute send displs in bytes
        //for(auto it = sizes.begin();it!=sizes.end();it++){ (*it)*=sizeof(triplet<F>);}
        displs[0] = 0;
        std::partial_sum(sizes.begin(),sizes.end(),&displs[1]);


        std::vector<int> rsizes(mpisize,0);
        std::vector<int> rdispls(mpisize+1,0);
        MPI_Alltoall(&sizes[0],sizeof(int),MPI_BYTE,&rsizes[0],sizeof(int),MPI_BYTE,comm);

        //now compute receiv displs with actual sizes 
        rdispls[0] = 0;
        std::partial_sum(rsizes.begin(),rsizes.end(),&rdispls[1]);
        Ptr totRecv = rdispls.back();
        std::vector<triplet<F> > rbuf(totRecv);
        MPI_Alltoallv(&sbuf[0],&sizes[0],&displs[0],type,&rbuf[0],&rsizes[0],&rdispls[0],type,comm);


        //recompute displacements in triplets
        //for(auto it = rsizes.begin();it!=rsizes.end();it++){ (*it)/=sizeof(triplet<F>);}
        //rdispls[0] = 0;
        //std::partial_sum(rsizes.begin(),rsizes.end(),&rdispls[1]);
        //totRecv = rdispls.back();


        MPI_Type_free(&type);

        std::vector<Ptr> newColptr(newVtxCount+1,0);
        //compute column counts
        for(auto it = rbuf.begin(); it!=rbuf.end(); it++){
          //triplets are 0-based but everything is currently shifted by one
          newColptr[it->col-newFirstCol+1]++;
        }
        //turn it back to colptr
        newColptr[0]=baseval;
        std::partial_sum(newColptr.begin(),newColptr.end(),newColptr.begin());

        Ptr newNNZ = newColptr.back()-baseval;
        rowind.resize(newNNZ);

        nzvalLocal.resize(newNNZ);

        colptr = newColptr;

        //now use newColptr as a position backup
        for(auto it = rbuf.begin(); it!=rbuf.end(); it++){
          triplet<F> & trip = *it;
          Idx locCol = trip.col-newFirstCol;//0-based
          //colptr contains column count shifted by one
          Ptr pos = newColptr[locCol]++;//baseval-based
          rowind[pos-baseval] = trip.row+baseval;
          nzvalLocal[pos-baseval] = trip.val;
        }

        for(Int i = 0; i<mpisize;i++){
          Localg_.vertexDist[i] = newVertexDist[i];
        }

        //  logfileptr->OFS()<<"**********permuted unsorted******"<<std::endl;
        //  DumpMatlab();
        //  logfileptr->OFS()<<"*********************************"<<std::endl;

        //store it as a distributed perm ?
        this->cinvp.resize(Localg_.LocalVertexCount());
        if(Localg_.LocalVertexCount()>0){
          std::copy(invp+Localg_.LocalFirstVertex()-Localg_.GetBaseval(),
                invp+Localg_.LocalFirstVertex()-Localg_.GetBaseval()+Localg_.LocalVertexCount(),
                  this->cinvp.begin());
        }

        if(Localg_.GetSorted()){
          Localg_.SetSorted(false);
          SortGraph(); 
        }
      }
    }


  template< typename F>
    void DistSparseMatrix<F>::SortGraph(){
      scope_timer(a,DistMat_SortGraph);
      if(!Localg_.GetSorted()){
        std::vector<std::size_t> lperm;
        std::vector<Idx> work;
        std::vector<F> work2;
        Int baseval = Localg_.GetBaseval();
        Idx FirstLocalCol = Localg_.LocalFirstVertex() - baseval;
        for(Idx locCol = 0 ; locCol< Localg_.LocalVertexCount(); locCol++){
          Idx col = FirstLocalCol + locCol;  // 0 based
          Ptr colbeg = Localg_.colptr[locCol]-baseval; //now 0 based
          Ptr colend = Localg_.colptr[locCol+1]-1-baseval; // now 0 based, inclusive
          sort_permutation(&Localg_.rowind[colbeg],&Localg_.rowind[colend]+1,std::less<Idx>(),lperm);
          apply_permutation(&Localg_.rowind[colbeg],&Localg_.rowind[colend]+1,lperm,work);
          apply_permutation(&nzvalLocal[colbeg],&nzvalLocal[colend]+1,lperm,work2);
        }
        Localg_.SetSorted(true);
      }
    }

  template< typename F>
    void DistSparseMatrix<F>::ToLowerTriangular(){
      auto & expanded = Localg_.expanded;
      if(expanded)
      {
        Ptr localNNZ = 0;
        Int baseval = Localg_.GetBaseval();
        Idx FirstLocalCol = Localg_.LocalFirstVertex() - baseval;

        std::vector<Ptr> newColptr(Localg_.colptr.size());
        for(Idx locCol = 0 ; locCol< Localg_.LocalVertexCount(); locCol++){
          Idx col = FirstLocalCol + locCol;  // 0 based
          newColptr[locCol] = localNNZ+baseval;
          Ptr colbeg = Localg_.colptr[locCol]-baseval; //now 0 based
          Ptr colend = Localg_.colptr[locCol+1]-baseval; // now 0 based
          for(Ptr rptr = colbeg ; rptr< colend ; rptr++ ){
            Idx row = Localg_.rowind[rptr]-baseval; //0 based
            if(row>col || (row==col && Localg_.GetKeepDiag())){
              localNNZ++;
            }
          }
        }
        newColptr.back() = localNNZ+baseval;

        //allocate new rowind & nzval
        std::vector<Idx> newRowind(localNNZ);
        std::vector<F> newNzvalLocal(localNNZ);
        localNNZ = 0;
        for(Idx locCol = 0 ; locCol< Localg_.LocalVertexCount(); locCol++){
          Idx col = FirstLocalCol + locCol;  // 0 based
          Ptr colbeg = Localg_.colptr[locCol]-baseval; //now 0 based
          Ptr colend = Localg_.colptr[locCol+1]-baseval; // now 0 based
          for(Ptr rptr = colbeg ; rptr< colend ; rptr++ ){
            Idx row = Localg_.rowind[rptr]-baseval; //0 based
            if(row>col || (row==col && Localg_.GetKeepDiag())){
              F & val = nzvalLocal[rptr];
              newRowind[localNNZ] = row+baseval;
              newNzvalLocal[localNNZ] = val;
              localNNZ++;
            }
          }
        }

        //swap the contents 
        Localg_.colptr.swap(newColptr);
        Localg_.rowind.swap(newRowind);
        nzvalLocal.swap(newNzvalLocal);
        nnz = (nnz - size)/2 + size;
        //TODO this has been commented because nnz contains the global nnz
        //Localg_.nnz = localNNZ;
        Localg_.nnz = nnz;

        //if(Localg_.GetSorted()){
        //  Localg_.SetSorted(false);
        //  SortGraph(); 
        //}


        expanded = false;
      }
    }

  template< typename F>
    void DistSparseMatrix<F>::ExpandSymmetric(){
      auto & expanded = Localg_.expanded;
      if(!expanded)
      {
        scope_timer(a,DistMat_Expand);
        int ismpi=0;
        MPI_Initialized( &ismpi);
        int isnull= (comm == MPI_COMM_NULL);

        if(!ismpi || isnull){
          //throw an exception
          throw std::logic_error("MPI communicator needs to be set.");
        }

        int mpisize,mpirank;
        MPI_Comm_size(comm,&mpisize);
        MPI_Comm_rank(comm,&mpirank);

        auto baseval = this->Localg_.baseval;
        auto & sorted = Localg_.sorted;
        auto & keepDiag = Localg_.keepDiag;
        auto & colptr = Localg_.colptr;
        auto & rowind = Localg_.rowind;

        Idx N = size; 


        Idx firstLocCol = Localg_.LocalFirstVertex()-Localg_.baseval;
        Idx locColCnt = Localg_.LocalVertexCount();

        std::vector<triplet<F> > Isend,Irecv;
        std::vector<int> ssizes(mpisize,0);
        std::vector<int> rsizes(mpisize,0);
        std::vector<int> sdispls(mpisize+1,0);
        std::vector<int> rdispls(mpisize+1,0);

        std::vector<Ptr> curPos;
        std::vector<Ptr> prevPos;

        //loop through my local columns and figure out the extra nonzero per col on prow
        SYMPACK_TIMER_START(DistMat_Expand_count);
        for(Idx locCol = 0 ; locCol< locColCnt; locCol++){
          Idx col = firstLocCol + locCol;  // 0 based
          Ptr colbeg = colptr[locCol]-baseval; //now 0 based
          Ptr colend = colptr[locCol+1]-baseval; // now 0 based
          for(Ptr rptr = colbeg ; rptr< colend ; rptr++ ){
            Idx row = rowind[rptr]-baseval; //0 based
            if(row>col){
              //where should it go ?
              Idx pdest; for(pdest = 0; pdest<mpisize; pdest++){ if(row>=Localg_.vertexDist[pdest]-baseval && row < Localg_.vertexDist[pdest+1]-baseval){ break;} }
              ssizes[pdest]++;
            }
          }
        }
        SYMPACK_TIMER_STOP(DistMat_Expand_count);


        sdispls[0] = 0;
        std::partial_sum(ssizes.begin(),ssizes.end(),sdispls.begin()+1);
        int totSend = sdispls.back();
        Isend.resize(totSend);
        //serialize
        SYMPACK_TIMER_START(DistMat_Expand_pack);
        for(Idx locCol = 0 ; locCol< locColCnt; locCol++){
          Idx col = firstLocCol + locCol;  // 0 based
          Ptr colbeg = colptr[locCol]-baseval; //now 0 based
          Ptr colend = colptr[locCol+1]-baseval; // now 0 based
          for(Ptr rptr = colbeg ; rptr< colend ; rptr++ ){
            Idx row = rowind[rptr]-baseval; //0 based
            if(row>col){
              Idx pdest; for(pdest = 0; pdest<mpisize; pdest++){ if(row>=Localg_.vertexDist[pdest]-baseval && row < Localg_.vertexDist[pdest+1]-baseval){ break;} }
              triplet<F> & trip = Isend[sdispls[pdest]++];
              trip.row = col;
              trip.col = row;
              trip.val = nzvalLocal[rptr];
            }
          }
        }  
        SYMPACK_TIMER_STOP(DistMat_Expand_pack);

        //for(auto it = ssizes.begin();it!=ssizes.end();it++){  (*it)*=sizeof(triplet<F>);}
        sdispls[0] = 0;
        std::partial_sum(ssizes.begin(),ssizes.end(),sdispls.begin()+1);
        totSend = sdispls.back();

        MPI_Datatype type;
        MPI_Type_contiguous( sizeof(triplet<F>), MPI_BYTE, &type );
        MPI_Type_commit(&type);

        SYMPACK_TIMER_START(DistMat_Expand_communication);
        MPI_Alltoall(&ssizes[0],sizeof(int),MPI_BYTE,&rsizes[0],sizeof(int),MPI_BYTE,comm);


        rdispls[0] = 0;
        std::partial_sum(rsizes.begin(),rsizes.end(),rdispls.begin()+1);
        int totRecv = rdispls.back();///sizeof(triplet<F>);
        Irecv.resize(totRecv);

        MPI_Alltoallv(&Isend[0],&ssizes[0],&sdispls[0],type,&Irecv[0],&rsizes[0],&rdispls[0],type,comm);
        SYMPACK_TIMER_STOP(DistMat_Expand_communication);

        MPI_Type_free(&type);
        //now parse
        //for(auto it = rsizes.begin();it!=rsizes.end();it++){  (*it)/=sizeof(triplet<F>);}


        std::vector<Ptr> newColptr(colptr.size(),0);
        for(int col=colptr.size()-1;col>0;col--){
          //convert to count instead
          newColptr[col] = colptr[col] - colptr[col-1];//baseval-based
        }

        //update the column counts
        for(auto it = Irecv.begin(); it!=Irecv.end(); it++){
          //triplets are 0-based but everything is currently shifted by one
          newColptr[it->col-firstLocCol+1]++;
        }

        //turn it back to colptr
        newColptr[0]=baseval;
        std::partial_sum(newColptr.begin(),newColptr.end(),newColptr.begin());
      

        Ptr newNNZ = newColptr.back()-baseval;
        rowind.resize(newNNZ);
        this->nnz = (this->nnz - this->size)*2 + this->size;

        nzvalLocal.resize(newNNZ);

        SYMPACK_TIMER_START(DistMat_Expand_shift);
        //shift the content
        for(int col=colptr.size()-2;col>=0;col--){
          std::copy_backward(&rowind[colptr[col]-baseval],&rowind[colptr[col+1]-baseval],&rowind[newColptr[col+1]-baseval]);
          std::copy_backward(&nzvalLocal[colptr[col]-baseval],&nzvalLocal[colptr[col+1]-baseval],&nzvalLocal[newColptr[col+1]-baseval]);
        }
        SYMPACK_TIMER_STOP(DistMat_Expand_shift);

        //add the new content, using colptr as a position backup
        //turn colptr to count
        for(int col=colptr.size()-1;col>0;col--){
          colptr[col] = colptr[col] - colptr[col-1];//baseval-based
        }

        SYMPACK_TIMER_START(DistMat_Expand_unpack);
        for(auto it = Irecv.begin(); it!=Irecv.end(); it++){
          triplet<F> & trip = *it;
          Idx locCol = trip.col-firstLocCol;//0-based
          //colptr contains column count shifted by one
          Ptr pos = newColptr[locCol + 1] - colptr[locCol+1]-1;//baseval-based
          colptr[locCol+1]++;

          rowind[pos-baseval] = trip.row+baseval;
          nzvalLocal[pos-baseval] = trip.val;
        }
        //exchange content for the colptr array
        colptr.swap(newColptr);
        SYMPACK_TIMER_STOP(DistMat_Expand_unpack);

        expanded =true;
        //keepDiag = 1;

        if(Localg_.GetSorted()){
          scope_timer(a,DistMat_Expand_sort);
          Localg_.SetSorted(false);
          SortGraph(); 
        }

      }
    }


  template< typename F>
  bool DistSparseMatrix<F>::isPermuted(Int * ainvp,Idx * avertexDist){
//    //if the current invp is empty, then matrix is unpermuted
//    if(cinvp.empty()){
//      return false;
//    }
  
    //if it is not the same permutation, consider that it is unpermuted
    int mpisize,mpirank;
    MPI_Comm_size(comm,&mpisize);
    MPI_Comm_rank(comm,&mpirank);

    if(avertexDist == NULL){
      avertexDist = &Localg_.vertexDist[0];
    }

    //quick check on the size
    if(cinvp.size()!=avertexDist[mpirank+1]-avertexDist[mpirank]){
      return false;
    }
    else{
      //offset the ainvp pointer, avertexDist[0] contains the baseval
      ainvp = ainvp+ avertexDist[mpirank] - avertexDist[0];
      for(size_t i = 0; i<cinvp.size(); i++){
        if(cinvp[i]!=ainvp[i]){
          return false;
        }
      }
    }

    return true;
  }




}


#endif // _DIST_SPARSE_MATRIX_IMPL_HPP_
