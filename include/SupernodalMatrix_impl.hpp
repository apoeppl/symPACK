#ifndef _SUPERNODAL_MATRIX_IMPL_HPP_
#define _SUPERNODAL_MATRIX_IMPL_HPP_

#include "SupernodalMatrix.hpp"

#define TAG_FACTOR 0

namespace LIBCHOLESKY{

  inline void gdb_lock(){
    int lock = 1;
    while (lock == 1){
      lock =1;
    }
  }


template <typename T> SupernodalMatrix<T>::SupernodalMatrix(){
}

template <typename T> SupernodalMatrix<T>::SupernodalMatrix(const DistSparseMatrix<T> & pMat,MAPCLASS & pMapping, MPI_Comm & pComm ){
  Int iam;
  MPI_Comm_rank(pComm, &iam);
  Int np;
  MPI_Comm_size(pComm, &np);

  iSize_ = pMat.size;
  Local_ = pMat.GetLocalStructure();
  Local_.ToGlobal(Global_);

  ETree_.ConstructETree(Global_);


  IntNumVec cc,rc;
  Global_.GetLColRowCount(ETree_,cc,rc);

  logfileptr->OFS()<<"colcnt "<<cc<<std::endl;
  logfileptr->OFS()<<"rowcnt "<<rc<<std::endl;

  Global_.FindSupernodes(ETree_,cc,SupMembership_,Xsuper_);

  logfileptr->OFS()<<"Membership list is "<<SupMembership_<<std::endl;
  logfileptr->OFS()<<"xsuper "<<Xsuper_<<std::endl;

//  UpdatesCount.Resize(Xsuper_.Size());
//  for(Int I = 1; I<Xsuper_.Size();++I){
//    Int first_col = Xsuper_[I-1];
//    Int last_col = Xsuper_[I]-1;
//    for(Int 
//  }


  Global_.SymbolicFactorization(ETree_,cc,Xsuper_,xlindx_,lindx_);
  logfileptr->OFS()<<"xlindx "<<xlindx_<<std::endl;
  logfileptr->OFS()<<"lindx "<<lindx_<<std::endl;


  GetUpdatingSupernodeCount(UpdateCount_);
  logfileptr->OFS()<<"Supernodal counts are "<<UpdateCount_<<std::endl;


  SupETree_ = ETree_.ToSupernodalETree(Xsuper_);
  logfileptr->OFS()<<"Supernodal Etree is "<<SupETree_<<std::endl;

  Mapping_ = pMapping;

  //copy
    for(Int I=1;I<Xsuper_.m();I++){
      Int fc = Xsuper_(I-1);
      Int lc = Xsuper_(I)-1;
      Int iWidth = lc-fc+1;
      Int fi = xlindx_(I-1);
      Int li = xlindx_(I)-1;
      Int iHeight = li-fi+1;

      Int iDest = pMapping.Map(I-1,I-1);

      //parse the first column to create the NZBlock
      if(iam==iDest){
        LocalSupernodes_.push_back( new SuperNode<T>(I,fc,lc,iHeight));
        SuperNode<T> & snode = *LocalSupernodes_.back();


        for(Int idx = fi; idx<=li;idx++){
          Int iStartRow = lindx_(idx-1);
          Int iPrevRow = iStartRow;
          Int iContiguousRows = 1;
          for(Int idx2 = idx+1; idx2<=li;idx2++){
            Int iCurRow = lindx_(idx2-1);
            if(iStartRow == lindx_(fi-1)){
              if(iCurRow>iStartRow+iWidth-1){
                //enforce the first block to be a square diagonal block
                break;
              }
            }
            
            if(iCurRow==iPrevRow+1){
              idx++;
              ++iContiguousRows;
              iPrevRow=iCurRow;
            }
            else{
              break;
            }
          }

          Int iCurNZcnt = iContiguousRows * iWidth;
#ifdef _DEBUG_
          logfileptr->OFS()<<"Creating a new NZBlock for rows "<<iStartRow<<" to "<<iStartRow + iContiguousRows-1<<" with "<<iCurNZcnt<<" nz."<<std::endl;
#endif
          snode.AddNZBlock(iContiguousRows , iWidth,iStartRow);
if(I==Xsuper_.m()-1){
          logfileptr->OFS()<<"Creating a new NZBlock for rows "<<iStartRow<<" to "<<iStartRow + iContiguousRows-1<<" with "<<iCurNZcnt<<" nz."<<std::endl;
          logfileptr->OFS()<<snode.GetNZBlock(snode.NZBlockCnt()-1)<<endl;
}

        }

        snode.Shrink();
      }

      //Distribute the data

      //look at the owner of the first column
      Int numColFirst = iSize_ / np;
      Int prevOwner = -1;
      DblNumVec aRemoteCol;
      T * pdNzVal = NULL;
      Int iStartIdxCopy = 0;
      //copy the data from A into this Block structure
      for(Int i = fc;i<=lc;i++){

        Int iOwner = std::min((i-1)/numColFirst,np-1);

        if(iOwner != prevOwner){

          prevOwner = iOwner;
          //we need to transfer the bulk of columns
          //first compute the number of cols we need to transfer
          Int iNzTransfered = 0;
          Int iLCTransfered = lc;

          for(Int j =i;j<=lc;++j){
            iOwner = std::min((j-1)/numColFirst,np-1);
            if(iOwner == prevOwner){
              Int nrows = Global_.colptr(j) - Global_.colptr(j-1);
              iNzTransfered+=nrows;
              //              logfileptr->OFS()<<"Looking at col "<<j<<" which is on P"<<iOwner<<std::endl;
            }
            else{
              iLCTransfered = j-1;
              break;
            }
          } 


          //#ifdef _DEBUG_
          //logfileptr->OFS()<<"Column "<<i<<" to "<<iLCTransfered<<" are owned by P"<<prevOwner<<" and should go on P"<<iDest<<std::endl;
          //logfileptr->OFS()<<"They contain "<<iNzTransfered<<" nz"<<std::endl;
          //#endif

          //if data needs to be transfered
          if(iDest!=prevOwner){
            if(iam == iDest){
              aRemoteCol.Resize(iNzTransfered);

              //MPI_Recv
              MPI_Recv(aRemoteCol.Data(),iNzTransfered*sizeof(T),MPI_BYTE,prevOwner,0,pComm,MPI_STATUS_IGNORE);
              iStartIdxCopy = 0;
            } 
            else if (iam == prevOwner){
              Int local_i = (i-(numColFirst)*iam);
              Int iColptrLoc = Local_.colptr(local_i-1);
              Int iRowIndLoc = Local_.rowind(iColptrLoc-1);
              Int iLastRowIndLoc = Local_.rowind(Local_.colptr(local_i)-1-1);
              const T * pdData = &pMat.nzvalLocal(iColptrLoc-1);

              //MPI_send
              MPI_Send(pdData,iNzTransfered*sizeof(T),MPI_BYTE,iDest,0,pComm);
            }
          }
        }

        //copy the data if I own it
        if(iam == iDest){
          SuperNode<T> & snode = *LocalSupernodes_.back();

          //isData transfered or local
          if(iam!=prevOwner){
            pdNzVal = aRemoteCol.Data();
            //logfileptr->OFS()<<"pdNzVal is the remote buffer"<<std::endl;
            //logfileptr->OFS()<<aRemoteCol<<std::endl;
          }
          else{
            Int local_i = (i-(numColFirst)*iam);
            Int iColptrLoc = Local_.colptr(local_i-1);
            pdNzVal = &pMat.nzvalLocal(iColptrLoc-1);
            //logfileptr->OFS()<<"pdNzVal is the local pMat"<<std::endl;
            iStartIdxCopy = 0;
          }

          //Copy the data from pdNzVal in the appropriate NZBlock

          Int iGcolptr = Global_.colptr(i-1);
          Int iRowind = Global_.rowind(iGcolptr-1);
          Int iNrows = Global_.colptr(i) - Global_.colptr(i-1);
          Int idxA = 0;
          Int iLRow = 0;
          Int firstrow = fi + i-fc;
          for(Int idx = firstrow; idx<=li;idx++){
            iLRow = lindx_(idx-1);
            //logfileptr->OFS()<<"Looking at L("<<iLRow<<","<<i<<")"<<std::endl;
            if( iLRow == iRowind){
              Int iNZBlockIdx = snode.FindBlockIdx(iLRow);

              T elem = pdNzVal[iStartIdxCopy + idxA];

              NZBlock<T> & pDestBlock = snode.GetNZBlock(iNZBlockIdx);
              //logfileptr->OFS()<<*pDestBlock<<std::endl;
              
              //find where we should put it
              Int localCol = i - fc;
              Int localRow = iLRow - pDestBlock.GIndex();
#ifdef _DEBUG_
              logfileptr->OFS()<<"Elem is A("<<iRowind<<","<<i<<") = "<<elem<<" at ("<<localRow<<","<<localCol<<") in "<<iNZBlockIdx<<"th NZBlock of L"<< std::endl;
#endif
              pDestBlock.Nzval(localRow,localCol) = elem;
              if(idxA<iNrows){
                idxA++;
                iRowind = Global_.rowind(iGcolptr+idxA-1-1);
              }
            }
          }

        }
        
      }
#ifdef _DEBUG_
      logfileptr->OFS()<<"--------------------------------------------------"<<std::endl;
#endif
    }

}


template <typename T> void SupernodalMatrix<T>::GetUpdatingSupernodeCount(IntNumVec & sc){
  sc.Resize(Xsuper_.m());
  SetValue(sc,I_ZERO);
  IntNumVec marker(Xsuper_.m());
  SetValue(sc,I_ZERO);

  for(Int s = 1; s<Xsuper_.m(); ++s){
    Int first_col = Xsuper_(s-1);
    
    Int fi = xlindx_(s-1);
    Int li = xlindx_(s)-1;

    for(Int row_idx = fi; row_idx<=li;++row_idx){
      Int row = lindx_(row_idx-1);
      Int supno = SupMembership_(row-1);
      
      if(marker(supno-1)!=s && supno!=s){
        ++sc(supno-1);
        marker(supno-1) = s;
      }
    }
  }
}


template <typename T> SparseMatrixStructure SupernodalMatrix<T>::GetLocalStructure() const {
    return Local_;
  }

template <typename T> SparseMatrixStructure SupernodalMatrix<T>::GetGlobalStructure(){
    if(!globalAllocated){
      Local_.ToGlobal(Global_);
      globalAllocated= true;
    }
    return Global_;
  }

//FIXME write this function also in terms of SparseMatrixStructure and supno rather than Supernode object.
template <typename T> inline bool SupernodalMatrix<T>::FindNextUpdate(Int src_snode_id, Int & src_first_row, Int & src_last_row, Int & tgt_snode_id){
  Int src_fc = Xsuper_(src_snode_id-1);
  Int src_lc = Xsuper_(src_snode_id)-1;

  //look in the global structure for the next nnz below row src_lc  
  Int first_row_ptr = xlindx_(src_snode_id-1);
  Int last_row_ptr = xlindx_(src_snode_id)-1;
  Int src_last_row_ptr = 0;
  Int src_first_row_ptr = 0;

  Int subdiag_row_cnt = last_row_ptr - first_row_ptr;

    Int first_row = lindx_(first_row_ptr-1);
    Int last_row = lindx_(last_row_ptr-1);

//    logfileptr->OFS()<<"prev src_first_row = "<<src_first_row<<endl;
//    logfileptr->OFS()<<"prev src_last_row = "<<src_last_row<<endl;
//    logfileptr->OFS()<<"first_row = "<<first_row<<endl;
//    logfileptr->OFS()<<"last_row = "<<last_row<<endl;
//    logfileptr->OFS()<<"first_row_ptr = "<<first_row_ptr<<endl;
//    logfileptr->OFS()<<"last_row_ptr = "<<last_row_ptr<<endl;
//    logfileptr->OFS()<<"src_last_row_ptr = "<<src_last_row_ptr<<endl;
//    logfileptr->OFS()<<"subdiag_row_cnt = "<<subdiag_row_cnt<<endl;


  //if tgt_snode_id == 0 , this is the first call to the function
  if(tgt_snode_id == 0){

    if(subdiag_row_cnt == 0 ){
      return false;
    }
    
    Int first_row = lindx_(first_row_ptr);
    tgt_snode_id = SupMembership_(first_row-1);
    src_first_row = first_row;
    src_last_row_ptr = first_row_ptr;
  }
  else{
    
    //find the block corresponding to src_last_row
    //src_nzblk_idx = src_snode.FindBlockIdx(src_last_row);

    //src_last_row_ptr = src_last_row - lindx_(first_row_ptr-1) + first_row_ptr;
    src_last_row_ptr = std::find(&lindx_(first_row_ptr-1),&lindx_(last_row_ptr), src_last_row) - &lindx_(first_row_ptr-1) + first_row_ptr;
    if(src_last_row_ptr == last_row_ptr){
      return false;
    }
    else{
      src_first_row_ptr = src_last_row_ptr+1;
      if(src_first_row_ptr > last_row_ptr){
        return false;
      }
      else{
        Int first_row = lindx_(src_first_row_ptr-1);
        tgt_snode_id = SupMembership_(first_row-1);
        src_first_row = first_row;
      }
    }
  }

  //Now we try to find src_last_row
  src_first_row_ptr = src_last_row_ptr+1;
  Int src_fr = src_first_row;
  
  //Find the last contiguous row
  Int src_lr = src_first_row;
  for(Int i = src_first_row_ptr+1; i<=last_row_ptr; ++i){
    if(src_lr+1 == lindx_(i-1)){ ++src_lr; }
    else{ break; }
  }

  Int tgt_snode_id_first = SupMembership_(src_fr-1);
  Int tgt_snode_id_last = SupMembership_(src_lr-1);
  if(tgt_snode_id_first == tgt_snode_id_last){
    //this can be a zero row in the src_snode
    tgt_snode_id = tgt_snode_id_first;

//    src_last_row = min(src_lr,Xsuper_(tgt_snode_id_first)-1);

    //Find the last row in src_snode updating tgt_snode_id
    for(Int i = src_first_row_ptr +1; i<=last_row_ptr; ++i){
      Int row = lindx_(i-1);
      if(SupMembership_(row-1) != tgt_snode_id){ break; }
      else{ src_last_row = row; }
    }

  }
  else{
    src_last_row = Xsuper_(tgt_snode_id_first)-1;
    tgt_snode_id = tgt_snode_id_first;
  }

  return true;

}


template <typename T> inline bool SupernodalMatrix<T>::FindNextUpdate(SuperNode<T> & src_snode, Int & src_nzblk_idx, Int & src_first_row, Int & src_last_row, Int & tgt_snode_id){
  //src_nzblk_idx is the last nzblock index examined
  //src_first_row is the first row updating the supernode examined
  //src_last_row is the last row updating the supernode examined
//if(src_snode.Id() == 17){gdb_lock();}

  //if tgt_snode_id == 0 , this is the first call to the function
  if(tgt_snode_id == 0){
    
    //find the first sub diagonal block
    src_nzblk_idx = -1;
    for(Int blkidx = 0; blkidx < src_snode.NZBlockCnt(); ++blkidx){
      if(src_snode.GetNZBlock(blkidx).GIndex() > src_snode.LastCol()){
        src_nzblk_idx = blkidx;
        break;
      }
    }

    if(src_nzblk_idx == -1 ){
      return false;
    }
    else{
      src_first_row = src_snode.GetNZBlock(src_nzblk_idx).GIndex();
    }
  }
  else{
    //find the block corresponding to src_last_row
    src_nzblk_idx = src_snode.FindBlockIdx(src_last_row);
    if(src_nzblk_idx==src_snode.NZBlockCnt()){
      return false;
    }
    else{
      NZBlock<T> & nzblk = src_snode.GetNZBlock(src_nzblk_idx);
      Int src_lr = nzblk.GIndex()+nzblk.NRows()-1;
      if(src_last_row == src_lr){
        src_nzblk_idx++;
        if(src_nzblk_idx==src_snode.NZBlockCnt()){
          return false;
        }
        else{
          src_first_row = src_snode.GetNZBlock(src_nzblk_idx).GIndex();
        }
      }
      else{
        src_first_row = src_last_row+1;
      }
    }
  }

  //Now we try to find src_last_row
  NZBlock<T> & nzblk = src_snode.GetNZBlock(src_nzblk_idx);
  Int src_fr = max(src_first_row,nzblk.GIndex());
  Int src_lr = nzblk.GIndex()+nzblk.NRows()-1;

  Int tgt_snode_id_first = SupMembership_(src_fr-1);
  Int tgt_snode_id_last = SupMembership_(src_lr-1);
  if(tgt_snode_id_first == tgt_snode_id_last){
    //this can be a zero row in the src_snode
    tgt_snode_id = tgt_snode_id_first;

    src_last_row = Xsuper_(tgt_snode_id_first)-1;

    //look into other nzblk
//if(src_snode.Id() == 8){gdb_lock();}

    //Find the last row in src_snode updating tgt_snode_id
    Int new_blk_idx = src_snode.FindBlockIdx(src_last_row);
    if(new_blk_idx>=src_snode.NZBlockCnt()){
      //src_last_row is the last row of the last nzblock
      new_blk_idx = src_snode.NZBlockCnt()-1; 
    }

    NZBlock<T> & last_block = src_snode.GetNZBlock(new_blk_idx); 
    src_last_row = min(src_last_row,last_block.GIndex() + last_block.NRows() - 1);
    assert(src_last_row<= Xsuper_(tgt_snode_id_first)-1);

//    //this can be a zero row in the src_snode
//    tgt_snode_id = tgt_snode_id_first;
//
//    for(Int i = src_first_row_ptr +1; i<=last_row_ptr; ++i){
//      Int row = lindx_(i-1);
//      if(SupMembership_(row-1) != tgt_snode_id){ break; }
//      else{ src_last_row = row; }
//    }
  }
  else{
    src_last_row = Xsuper_(tgt_snode_id_first)-1;
    tgt_snode_id = tgt_snode_id_first;
  }


  assert(src_last_row>=src_first_row);
  assert(src_first_row>= Xsuper_(tgt_snode_id-1));
  assert(src_last_row<= Xsuper_(tgt_snode_id)-1);
  assert(src_first_row >= src_fr);
   
  return true;

}





template <typename T> inline bool SupernodalMatrix<T>::FindPivot(SuperNode<T> & src_snode, SuperNode<T> & tgt_snode,Int & pivot_idx, Int & pivot_fr, Int & pivot_lr){
  //Determine which columns will be updated
  pivot_idx = 0;
  pivot_fr = 0;
  pivot_lr = 0;
  for(int blkidx=0;blkidx<src_snode.NZBlockCnt();++blkidx){
    NZBlock<T> & nzblk = src_snode.GetNZBlock(blkidx);
    Int src_fr = nzblk.GIndex();
    Int src_lr = nzblk.GIndex()+nzblk.NRows()-1;

    if(src_lr>=tgt_snode.FirstCol() && src_fr<=tgt_snode.FirstCol() ){
      //found the "pivot" nzblk
      
      pivot_fr = max(tgt_snode.FirstCol(),src_fr);
      pivot_lr = min(tgt_snode.LastCol(),src_lr);

      pivot_idx = blkidx;
      return true;
    }
  }
}

template <typename T> void SupernodalMatrix<T>::UpdateSuperNode(SuperNode<T> & src_snode, SuperNode<T> & tgt_snode, Int &pivot_idx, Int  pivot_fr = I_ZERO){
    NZBlock<T> & pivot_nzblk = src_snode.GetNZBlock(pivot_idx);
    if(pivot_fr ==I_ZERO){
      pivot_fr = pivot_nzblk.GIndex();
    }
    assert(pivot_fr >= pivot_nzblk.GIndex());
    Int pivot_lr = min(pivot_nzblk.GIndex() +pivot_nzblk.NRows() -1, pivot_fr + tgt_snode.Size() -1);
    T * pivot = & pivot_nzblk.Nzval(pivot_fr-pivot_nzblk.GIndex(),0);

    Int tgt_updated_fc =  pivot_fr - tgt_snode.FirstCol();
    Int tgt_updated_lc =  pivot_fr - tgt_snode.FirstCol();
    logfileptr->OFS()<<"pivotidx = "<<pivot_idx<<" Cols "<<tgt_snode.FirstCol()+tgt_updated_fc<<" to "<< tgt_snode.FirstCol()+tgt_updated_lc <<" will be updated"<<std::endl;

    for(int src_idx=pivot_idx;src_idx<src_snode.NZBlockCnt();++src_idx){

      NZBlock<T> & src_nzblk = src_snode.GetNZBlock(src_idx);
      Int src_fr = max(pivot_fr, src_nzblk.GIndex()) ;
      Int src_lr = src_nzblk.GIndex()+src_nzblk.NRows()-1;
      
      do{
        Int tgt_idx = tgt_snode.FindBlockIdx(src_fr);
        NZBlock<T> & tgt_nzblk = tgt_snode.GetNZBlock(tgt_idx);
        Int tgt_fr = tgt_nzblk.GIndex();
        Int tgt_lr = tgt_nzblk.GIndex()+tgt_nzblk.NRows()-1;

        Int update_fr = max(tgt_fr, src_fr);
        Int update_lr = min(tgt_lr, src_lr);

        assert(update_fr >= tgt_fr); 
        assert(update_lr >= update_fr); 
        assert(update_lr <= tgt_lr); 

        //Update tgt_nzblk with src_nzblk
        logfileptr->OFS()<<"Updating SNODE "<<tgt_snode.Id()<<" Block "<<tgt_idx<<" ["<<update_fr<<".."<<update_lr<<"] with SNODE "<<src_snode.Id()<<" Block "<<src_idx<<" ["<<update_fr<<".."<<update_lr<<"]"<<endl;

        T * src = &src_nzblk.Nzval(update_fr - src_fr ,0);
        T * tgt = &tgt_nzblk.Nzval(update_fr - tgt_fr ,tgt_updated_fc);

#ifdef ROW_MAJOR
        blas::Gemm('T','N',pivot_lr-pivot_fr+1, update_lr - update_fr + 1,src_snode.Size(),1.0,pivot,pivot_nzblk.LDA(),src,src_nzblk.LDA(),1.0,tgt,tgt_nzblk.LDA());
#else
        blas::Gemm('N','T', update_lr - update_fr + 1,pivot_lr-pivot_fr+1,  src_snode.Size(),1.0,src,src_nzblk.LDA(),pivot,pivot_nzblk.LDA(),1.0,tgt,tgt_nzblk.LDA());
#endif

        if(tgt_idx+1<tgt_snode.NZBlockCnt()){
          NZBlock<T> & next_tgt_nzblk = tgt_snode.GetNZBlock(tgt_idx+1);
          Int next_tgt_fr = next_tgt_nzblk.GIndex();
          src_fr = next_tgt_fr;
        }
        else{
          break;
        }
      }while(src_fr<src_lr);
    }

}




template <typename T> void SupernodalMatrix<T>::Factorize( MPI_Comm & pComm ){
  Int iam,np;
  MPI_Comm_rank(pComm, &iam);
  MPI_Comm_size(pComm, &np);
  IntNumVec UpdatesToDo = UpdateCount_;
 
  //dummy right looking cholesky factorization
  for(Int I=1;I<Xsuper_.m();I++){
    Int src_first_col = Xsuper_(I-1);
    Int src_last_col = Xsuper_(I)-1;
    Int iOwner = Mapping_.Map(I-1,I-1);
    //If I own the column, factor it
    if( iOwner == iam ){
      Int iLocalI = (I-1) / np +1 ;
      SuperNode<T> & src_snode = *LocalSupernodes_[iLocalI -1];
      logfileptr->OFS()<<"Supernode "<<I<<"("<<src_snode.Id()<<") is on P"<<iOwner<<" local index is "<<iLocalI<<std::endl; 
     
      assert(src_snode.Id() == I); 

//      std::set<Int> SuperLRowStruct;
//      Global_.GetSuperLRowStruct(ETree_, Xsuper_, I, SuperLRowStruct);
//        
//      logfileptr->OFS()<<"Row structure of Supernode "<<I<<" is ";
//      for(std::set<Int>::iterator it = SuperLRowStruct.begin(); it != SuperLRowStruct.end(); ++it){
//        logfileptr->OFS()<<*it<<" ";
//      }
//      logfileptr->OFS()<<endl;
//      logfileptr->OFS()<<"Updates count for Supernode "<<I<<" is "<<UpdateCount_(I-1)<<endl;
//
//      assert(SuperLRowStruct.size()==UpdateCount_(I-1));


      //Do all my updates (Local and remote)
      std::vector<char> src_nzblocks;
      while(UpdatesToDo(I-1)>0){
          logfileptr->OFS()<<UpdatesToDo(I-1)<<" updates left"<<endl;

          if(src_nzblocks.size()==0){
            //Create an upper bound buffer  (we have the space to store the first header)
            size_t num_bytes = sizeof(Int);
            for(Int blkidx=0;blkidx<src_snode.NZBlockCnt();++blkidx){
              num_bytes += src_snode.GetNZBlock(blkidx).NRows()*NZBLOCK_ROW_SIZE<T>(src_snode.Size());
            }
            logfileptr->OFS()<<"We allocate a buffer of size "<<num_bytes<<std::endl;
            src_nzblocks.resize(num_bytes);
          }


          //MPI_Recv
          MPI_Status recv_status;


          char * recv_buf = &src_nzblocks[0]+ max(sizeof(Int),NZBLOCK_OBJ_SIZE<T>())-min(sizeof(Int),NZBLOCK_OBJ_SIZE<T>());
          MPI_Recv(recv_buf,&*src_nzblocks.end()-recv_buf,MPI_BYTE,MPI_ANY_SOURCE,I,pComm,&recv_status);
logfileptr->OFS()<<"Received something"<<endl;
          Int src_snode_id = *(Int*)recv_buf;


          //Resize the buffer to the actual number of bytes received
          int bytes_received = 0;
          MPI_Get_count(&recv_status, MPI_BYTE, &bytes_received);
          src_nzblocks.resize(recv_buf+bytes_received - &src_nzblocks[0]);


          //Create the dummy supernode for that data
          SuperNode<T> dist_src_snode(src_snode_id,Xsuper_[src_snode_id-1],Xsuper_[src_snode_id]-1,&src_nzblocks);

      logfileptr->OFS()<<dist_src_snode<<endl;


      //Update everything I own with that factor
      //Update the ancestors
      Int tgt_snode_id = 0;
      Int src_first_row = 0;
      Int src_last_row = 0;
      Int src_nzblk_idx = 0;
      while(FindNextUpdate(dist_src_snode, src_nzblk_idx, src_first_row, src_last_row, tgt_snode_id)){ 
        Int iTarget = Mapping_.Map(tgt_snode_id-1,tgt_snode_id-1);
        if(iTarget == iam){
          logfileptr->OFS()<<"RECV Supernode "<<tgt_snode_id<<" is updated by Supernode "<<dist_src_snode.Id()<<" rows "<<src_first_row<<" to "<<src_last_row<<" "<<src_nzblk_idx<<std::endl;

          Int iLocalJ = (tgt_snode_id-1) / np +1 ;
          SuperNode<T> & tgt_snode = *LocalSupernodes_[iLocalJ -1];

          UpdateSuperNode(dist_src_snode,tgt_snode,src_nzblk_idx, src_first_row);

          --UpdatesToDo(tgt_snode_id-1);
          logfileptr->OFS()<<UpdatesToDo(tgt_snode_id-1)<<" updates left for Supernode "<<tgt_snode_id<<endl;
        }
      }



          //restore to its capacity
          src_nzblocks.resize(src_nzblocks.capacity());

      }
      //clear the buffer
      { vector<char>().swap(src_nzblocks);  }

      logfileptr->OFS()<<"  Factoring node "<<I<<std::endl;

      //Factorize Diagonal block
      NZBlock<T> & diagonalBlock = src_snode.GetNZBlock(0);

#ifdef ROW_MAJOR
      lapack::Potrf( 'U', src_snode.Size(), diagonalBlock.Nzval(), diagonalBlock.LDA());
#else
      lapack::Potrf( 'L', src_snode.Size(), diagonalBlock.Nzval(), diagonalBlock.LDA());
#endif

      logfileptr->OFS()<<"    Diagonal block factored node "<<I<<std::endl;

      for(int blkidx=1;blkidx<src_snode.NZBlockCnt();++blkidx){
        NZBlock<T> & nzblk = src_snode.GetNZBlock(blkidx);

          //Update lower triangular blocks
#ifdef ROW_MAJOR
          blas::Trsm('R','U','T','N',nzblk.NCols(),src_snode.Size(), 1.0,  diagonalBlock.Nzval(), diagonalBlock.LDA(), nzblk.Nzval(), nzblk.LDA());
#else
          blas::Trsm('R','L','T','N',nzblk.NRows(),src_snode.Size(), 1.0,  diagonalBlock.Nzval(), diagonalBlock.LDA(), nzblk.Nzval(), nzblk.LDA());
#endif
//          logfileptr->OFS()<<diagonalBlock<<std::endl;
//          logfileptr->OFS()<<nzblk<<std::endl;
//          logfileptr->OFS()<<"    "<<blkidx<<"th subdiagonal block updated node "<<I<<std::endl;
      }


      //Send my factor to my ancestors. 
      BolNumVec is_factor_sent(np);
      SetValue(is_factor_sent,false);

      Int tgt_snode_id = 0;
      Int src_nzblk_idx = 0;
      Int src_first_row = 0;
      Int src_last_row = 0;
      while(FindNextUpdate(src_snode, src_nzblk_idx, src_first_row, src_last_row, tgt_snode_id)){ 

        Int iTarget = Mapping_.Map(tgt_snode_id-1,tgt_snode_id-1);
        if(iTarget != iam && !is_factor_sent[iTarget]){
          is_factor_sent[iTarget] = true;

          logfileptr->OFS()<<"Supernode "<<tgt_snode_id<<" is updated by Supernode "<<I<<" rows "<<src_first_row<<" to "<<src_last_row<<" "<<src_nzblk_idx<<std::endl;

          Int J = tgt_snode_id;

          Int tgt_first_col = Xsuper_(J-1);
          Int tgt_last_col = Xsuper_(J)-1;

          //Send
          NZBlock<T> * pivot_nzblk = &src_snode.GetNZBlock(src_nzblk_idx);

#ifdef ROW_MAJOR
          int local_first_row = src_first_row-pivot_nzblk->GIndex();

          char * start_buf = reinterpret_cast<char*>(&pivot_nzblk->Nzval(local_first_row,0));
          size_t num_bytes = src_snode.End() - start_buf;

          //Create a header
          NZBlockHeader<T> * header = new NZBlockHeader<T>(pivot_nzblk->NRows()-local_first_row,pivot_nzblk->NCols(),src_first_row);

          MPI_Datatype type;
          int lens[3];
          MPI_Aint disps[3];
          MPI_Datatype types[3];
          Int err = 0;


          /* define a struct that describes all our data */
          lens[0] = sizeof(Int);
          lens[1] = sizeof(NZBlockHeader<T>);
          lens[2] = num_bytes;
          MPI_Address(&I, &disps[0]);
          MPI_Address(header, &disps[1]);
          MPI_Address(start_buf, &disps[2]);
          types[0] = MPI_BYTE;
          types[1] = MPI_BYTE;
          types[2] = MPI_BYTE;
          MPI_Type_struct(3, lens, disps, types, &type);
          MPI_Type_commit(&type);

          //logfileptr->OFS()<<src_snode<<endl;
          
          /* send to target */
          //FIXME : maybe this can be replaced by a scatterv ?
          MPI_Send(MPI_BOTTOM,1,type,iTarget,tgt_snode_id,pComm);

          MPI_Type_free(&type);

          delete header;
          logfileptr->OFS()<<"     Send factor "<<I<<" to node"<<J<<" on P"<<iTarget<<" from blk "<<src_nzblk_idx<<" ("<<num_bytes+sizeof(*header)+sizeof(Int)<<" bytes)"<<std::endl;
#else
#ifdef SEND_WHOLE_BLOCK
#else
          int local_first_row = src_first_row-pivot_nzblk->GIndex();

          char * start_buf = reinterpret_cast<char*>(src_snode.NZBlockCnt()>src_nzblk_idx+1?&src_snode.GetNZBlock(src_nzblk_idx+1):src_snode.End());
          size_t num_bytes = src_snode.End() - start_buf;

          //Create a NZBlock
          Int nrows = pivot_nzblk->NRows()-local_first_row;
          Int ncols = pivot_nzblk->NCols();
          std::vector<char> tmpStorage(NZBLOCK_HEADER_SIZE<T>() + nrows*ncols*sizeof(T));
          NZBlock<T> * newBlock = new NZBlock<T>(nrows,ncols,src_first_row, &tmpStorage[0]);
          //Fill the block
          for(Int col = 0; col< pivot_nzblk->NCols(); ++col){
            for(Int row = local_first_row; row< pivot_nzblk->NRows(); ++row){
              newBlock->Nzval(row-local_first_row,col) = pivot_nzblk->Nzval(local_first_row,col);
            }
          }

//          logfileptr->OFS()<<"New block is "<<newBlock<<endl;


          MPI_Datatype type;
          int lens[3];
          MPI_Aint disps[3];
          MPI_Datatype types[3];
          Int err = 0;


          /* define a struct that describes all our data */
          lens[0] = sizeof(Int);
          //don't send the object itself because it will be created again on the receiver side
          lens[1] = NZBLOCK_HEADER_SIZE<T>()+newBlock->ByteSize(); 
          lens[2] = num_bytes;
          MPI_Address(&I, &disps[0]);
          MPI_Address(&tmpStorage[0], &disps[1]);
          MPI_Address(start_buf, &disps[2]);
          types[0] = MPI_BYTE;
          types[1] = MPI_BYTE;
          types[2] = MPI_BYTE;
          MPI_Type_struct(3, lens, disps, types, &type);
          MPI_Type_commit(&type);

          MPI_Send(MPI_BOTTOM,1,type,iTarget,tgt_snode_id,pComm);

          delete newBlock;
          logfileptr->OFS()<<"     Send factor "<<I<<" to node"<<J<<" on P"<<iTarget<<" from blk "<<src_nzblk_idx<<" ("<<lens[0] + lens[1] + lens[2]<<" bytes)"<<std::endl;
#endif // end ifdef SEND_WHOLE_BLOCK

#endif // end ifdef ROW_MAJOR


        }
      }

      //Update my ancestors right away. 
      tgt_snode_id = 0;
//      if(I == 7){ gdb_lock(); }
      while(FindNextUpdate(src_snode, src_nzblk_idx, src_first_row, src_last_row, tgt_snode_id)){ 

        Int iTarget = Mapping_.Map(tgt_snode_id-1,tgt_snode_id-1);
        if(iTarget == iam){
          logfileptr->OFS()<<"LOCAL Supernode "<<tgt_snode_id<<" is updated by Supernode "<<I<<" rows "<<src_first_row<<" to "<<src_last_row<<" "<<src_nzblk_idx<<std::endl;

          Int iLocalJ = (tgt_snode_id-1) / np +1 ;
          SuperNode<T> & tgt_snode = *LocalSupernodes_[iLocalJ -1];

          UpdateSuperNode(src_snode,tgt_snode,src_nzblk_idx, src_first_row);

          --UpdatesToDo(tgt_snode_id-1);
          logfileptr->OFS()<<UpdatesToDo(tgt_snode_id-1)<<" updates left"<<endl;
        }
      }
    }
///////    else{
///////      //Update the ancestors
///////      Int tgt_snode_id = 0;
///////      Int src_first_row = 0;
///////      Int src_last_row = 0;
///////      while(FindNextUpdate(I, src_first_row, src_last_row, tgt_snode_id)){ 
///////
///////          Int iTarget = Mapping_.Map(tgt_snode_id-1,tgt_snode_id-1);
///////          if(iTarget == iam){
///////            logfileptr->OFS()<<"Supernode "<<tgt_snode_id<<" is updated by Supernode "<<I<<" rows "<<src_first_row<<" to "<<src_last_row<<std::endl;
///////
///////
///////
///////      std::vector<char> src_nzblocks;
///////      {
///////          logfileptr->OFS()<<UpdatesToDo(I-1)<<" updates left"<<endl;
///////
///////          if(src_nzblocks.size()==0){
///////            //Create an upper bound buffer  (we have the space to store the first header)
///////            size_t num_bytes = sizeof(Int);
///////            for(Int blkidx=0;blkidx<src_snode.NZBlockCnt();++blkidx){
///////              num_bytes += src_snode.GetNZBlock(blkidx).NRows()*NZBLOCK_ROW_SIZE<T>(src_snode.Size());
///////            }
///////            logfileptr->OFS()<<"We allocate a buffer of size "<<num_bytes<<std::endl;
///////            src_nzblocks.resize(num_bytes);
///////          }
///////
///////
///////          //MPI_Recv
///////          MPI_Status recv_status;
///////
///////
///////          char * recv_buf = &src_nzblocks[0]+ max(sizeof(Int),NZBLOCK_OBJ_SIZE<T>())-min(sizeof(Int),NZBLOCK_OBJ_SIZE<T>());
///////          MPI_Recv(recv_buf,&*src_nzblocks.end()-recv_buf,MPI_BYTE,iOwner,I,pComm,&recv_status);
///////
///////          Int src_snode_id = *(Int*)recv_buf;
///////
///////
///////          //Resize the buffer to the actual number of bytes received
///////          int bytes_received = 0;
///////          MPI_Get_count(&recv_status, MPI_BYTE, &bytes_received);
///////          src_nzblocks.resize(recv_buf+bytes_received - &src_nzblocks[0]);
///////
///////
///////
///////          logfileptr->OFS()<<"Supernode "<<I<<" is updated by Supernode "<<src_snode_id<<", received "<<bytes_received<<" bytes, buffer of size "<<src_nzblocks.size()<<std::endl;
///////          //Create the dummy supernode for that data
///////          SuperNode<T> dist_src_snode(src_snode_id,Xsuper_[src_snode_id-1],Xsuper_[src_snode_id]-1,&src_nzblocks);
/////////
/////////          //logfileptr->OFS()<<dist_src_snode<<endl;
/////////
///////          Int idx = 0;
///////          UpdateSuperNode(dist_src_snode,src_snode,idx);
///////
///////
///////          //restore to its capacity
///////          src_nzblocks.resize(src_nzblocks.capacity());
///////
///////          --UpdatesToDo(I-1);
///////      }
///////      //clear the buffer
///////      { vector<char>().swap(src_nzblocks);  }
///////
///////
///////
///////
///////
///////
///////
///////
///////
///////
///////
///////          }
///////
///////      }
///////
///////    }
///
///
///    else{
///      //If I have something updated by this column, update it
///      // i.e do I have ancestors of I ? 
///
///
///
///
///
/////Not possible
/////      //Update my ancestors. 
/////      tgt_snode_id = 0;
/////      while(FindNextUpdate(src_snode, src_nzblk_idx, src_first_row, src_last_row, tgt_snode_id)){ 
/////
/////        Int iTarget = Mapping_.Map(tgt_snode_id-1,tgt_snode_id-1);
/////        if(iTarget == iam){
/////          logfileptr->OFS()<<"Supernode "<<tgt_snode_id<<" is updated by Supernode "<<I<<" rows "<<src_first_row<<" to "<<src_last_row<<" "<<src_nzblk_idx<<std::endl;
/////
/////          Int iLocalJ = (tgt_snode_id-1) / np +1 ;
/////          SuperNode<T> & tgt_snode = *LocalSupernodes_[iLocalJ -1];
/////
/////          UpdateSuperNode(src_snode,tgt_snode,src_nzblk_idx, src_first_row);
/////        }
/////      }
///
///
///
///
///
///
///  
///      Int J = SupETree_.Parent(I-1);
///      while(J != 0){
///
///        //Is it updated by I ? Is I nnz in LRowStruct ?
///
///        Int iAOwner = Mapping_.Map(J-1,J-1);
///        //If I own the ancestor supernode, update it
///        if( iAOwner == iam ){
///          Int iLocalJ = (J-1) / np +1 ;
///          SuperNode<T> & tgt_snode = *LocalSupernodes_[iLocalJ -1];
///          //upper bound, can be replaced by something tighter
///          size_t num_bytes = tgt_snode.BytesToEnd(0);
///          std::vector<char> src_nzblocks(num_bytes);
///          //Recv
///          MPI_Status recv_status;
///          MPI_Recv(&src_nzblocks[0],num_bytes,MPI_BYTE,iOwner,TAG_FACTOR,pComm,&recv_status);
///          int bytes_received = 0;
///          MPI_Get_count(&recv_status, MPI_BYTE, &bytes_received);
///          src_nzblocks.resize(bytes_received);
///  
///
///          SuperNode<T> src_snode(I,src_first_col,src_last_col,&src_nzblocks);
///
///          logfileptr->OFS()<<"     Recv factor "<<I<<" from P"<<iOwner<<" to update snode"<<J<<" on P"<<iam<<std::endl;       
///          //Update
///
///          UpdateSuperNode(src_snode,tgt_snode,iLocalJ);
///        }
///        J = SupETree_.Parent(J-1);
///      }
///    }
///

    MPI_Barrier(pComm);
  }
}


} // namespace LIBCHOLESKY


#endif 
