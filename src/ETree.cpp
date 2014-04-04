/// @file etree.cpp
/// @brief Implementation of the elimination-tree related algorithms.
/// @author Mathias Jacquelin
/// @date 2012-08-31
#include "ETree.hpp"
#include "utility.hpp"

namespace LIBCHOLESKY{

  void DisjointSet::Initialize(Int n){
    pp_.Resize(n);
    SetValue(pp_,I_ZERO);
    root_.Resize(n);
  }

  void DisjointSet::Finalize(){
    pp_.Resize(0);
  }

  Int DisjointSet::makeSet(Int i){
    pp_(i-1)=i;
    return i;
  }

  Int DisjointSet::link(Int s, Int t){
    pp_(s-1)=t;

    return t;
  }

  Int DisjointSet::find(Int i){
    Int p, gp;

    p = pp_(i-1);
    gp=pp_(p-1);

    while(gp!=p){
      i = makeSet(gp);
      p = pp_(i-1);
      gp = pp_(p-1);
    }

    return p;
  }


  void DisjointSet::Union(Int s, Int t){
    Int tSet= find(t);
    Int sSet= find(s);
    sSet = link(sSet, tSet );
    root_(sSet-1) = t;
  }



  ETree::ETree(){

  }

  ETree::ETree(SparseMatrixStructure & aGlobal){
    ConstructETree(aGlobal);
  }

  void ETree::PostOrderTree(){
    if(n_>0 && !bIsPostOrdered_){


      IntNumVec fson(n_);
      SetValue(fson, I_ZERO);
      IntNumVec brother(n_);
      SetValue(brother, I_ZERO);

      Int lroot = n_;
      for(Int vertex=n_-1; vertex>0; vertex--){
        Int curParent = parent_(vertex-1);
        if(curParent==0 || curParent == vertex){
          brother(lroot-1) = vertex;
          lroot = vertex;
        }
        else{
          brother(vertex-1) = fson(curParent-1);
          fson(curParent-1) = vertex;
        }
      }


//TODO RECYCLE FOR WHEN WE REORDER THE CHILD
/*
      IntNumVec lson(n_);
      SetValue(lson, I_ZERO);

      //Get Binary tree representation
      Int lroot = n_;
      for(Int vertex=n_-1; vertex>0; vertex--){
        Int curParent = parent_(vertex-1);
        if(curParent==0 || curParent == vertex){
          brother(lroot-1) = vertex;
          lroot = vertex;
        }
        else{
          Int ndlson = lson(curParent-1);
          if(ndlson > 0){
             if  ( cc(vertex-1) >= cc(ndlson-1) ) {                                                                                                                                       
                brother(vertex-1) = fson(curParent-1);
                fson(curParent-1) = vertex;
             }
             else{                                                                                                                                            
                brother(ndlson-1) = vertex;
                lson(curParent-1) = vertex;
             }                                                                                                                                                               
          }
          else{
             fson(curParent-1) = vertex;
             lson(curParent-1) = vertex;
          }
        }
      }
*/


      logfileptr->OFS()<<"parent "<<parent_<<std::endl;
      logfileptr->OFS()<<"fson "<<fson<<std::endl;
      logfileptr->OFS()<<"brother "<<brother<<std::endl;


      //Do a depth first search to construct the postordered tree
      IntNumVec stack(n_);
      postNumber_.Resize(n_);
      invPostNumber_.Resize(n_);

      Int stacktop=0, vertex=n_,m=0;
      bool exit = false;
      while( m<n_){
        do{
          stacktop++;
          stack(stacktop-1) = vertex;
          vertex = fson(vertex-1);
        }while(vertex>0);

        while(vertex==0){

          if(stacktop<=0){
            exit = true;
            break;
          }
          vertex = stack(stacktop-1);
          stacktop--;
          m++;

          postNumber_(vertex-1) = m;
          invPostNumber_(m-1) = vertex;

          vertex = brother(vertex-1);
        }

        if(exit){
          break;
        }
      }








      //      postParent_.Resize(n_);
      //modify the parent list ?
      // node i is now node postNumber(i-1)
            for(Int i=1; i<=n_;i++){
              Int nunode = postNumber_(i-1);
              Int ndpar = parent_(i-1);
              if(ndpar>0){
                ndpar = postNumber_(ndpar-1);
              }
              brother(nunode-1) = ndpar;
            }

      logfileptr->OFS()<<"new parent: "<<brother<<std::endl;
      logfileptr->OFS()<<"postNumber: "<<postNumber_<<std::endl;
      logfileptr->OFS()<<"invPostNumber: "<<invPostNumber_<<std::endl;

      bIsPostOrdered_ = true;
    }

  }




  void ETree::ConstructETree(SparseMatrixStructure & aGlobal){
    TIMER_START(ConstructETree);

    //Expand to symmetric storage
    aGlobal.ExpandSymmetric();

    n_ = aGlobal.size;


    parent_.Resize(n_);
    SetValue(parent_,I_ZERO );

    DisjointSet sets;
    sets.Initialize(n_);

    Int cset,rset,rroot,row;
    for (Int col = 1; col <= n_; col++) {
      parent_(col-1)=col; //1 based indexes
      cset = sets.makeSet (col);
      sets.Root(cset-1) = col;
      parent_(col-1) = 0; 

#ifdef _DEBUG_
      logfileptr->OFS()<<"Examining col "<<col<<std::endl;
#endif
      for (Int p = aGlobal.expColptr(col-1); p < aGlobal.expColptr(col); p++) {
        row = aGlobal.expRowind(p-1);

#ifdef _DEBUG_
        logfileptr->OFS()<<"Row = "<<row<<" vs col = "<<col<<std::endl;
#endif


        if (row >= col) continue;

        rset = sets.find(row);
        rroot = sets.Root(rset-1);
#ifdef _DEBUG_
        logfileptr->OFS()<<"Row "<<row<<" is in set "<<rset<<" represented by "<<rroot<<std::endl;
#endif

        if (rroot != col) {
          parent_(rroot-1) = col;
          cset = sets.link(cset, rset);
          sets.Root(cset-1) = col;
#ifdef _DEBUG_
          logfileptr->OFS()<<"Parent of "<<rroot<<" is "<<col<<" which now represents set"<<cset<<std::endl;
#endif
        }
      }

    }


    parent_(n_-1) = 0;

    TIMER_STOP(ConstructETree);
  }


  ETree ETree::ToSupernodalETree(IntNumVec & aXsuper) const{
    ETree newTree;
    newTree.n_ = aXsuper.m()-1;
    newTree.parent_.Resize(aXsuper.m()-1);
    
    IntNumVec colToSup(this->parent_.m());
    for(Int i=1; i<aXsuper.m(); ++i){
      for(Int j = aXsuper(i-1); j< aXsuper(i); ++j){
        colToSup(j-1) = i;
      }
    }
    colToSup(this->parent_.m()-1) = 0;

//    logfileptr->OFS()<<aXsuper<<std::endl;
//    logfileptr->OFS()<<this->parent_<<std::endl;
//    logfileptr->OFS()<<colToSup<<std::endl;


    for(Int i=1; i<=colToSup.m(); ++i){
        Int curSnode = colToSup(i-1);
        Int parentSnode = (this->parent_(i-1) == 0) ? 0:colToSup(this->parent_(i-1)-1);

        if( curSnode != parentSnode){
          newTree.parent_(curSnode-1) = parentSnode;
          logfileptr->OFS()<<"parent of curSnode "<<curSnode<<" is "<<parentSnode<<std::endl;
        }
    } 

    newTree.PostOrderTree();

    return newTree;

//      //translate from columns to supernodes etree using supIdx
//      etree_supno.resize(this->NumSuper());
//      for(Int i = 0; i < superNode->etree.m(); ++i){
//        Int curSnode = superNode->superIdx[i];
//        Int parentSnode = (superNode->etree[i]>= superNode->etree.m()) ?this->NumSuper():superNode->superIdx[superNode->etree[i]];
//        if( curSnode != parentSnode){
//          etree_supno[curSnode] = parentSnode;
//        }
//      }



    return newTree;
  }



}
