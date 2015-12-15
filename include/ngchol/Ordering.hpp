#ifndef _ORDERING_HPP_ 
#define _ORDERING_HPP_

#include  <stdlib.h>

#include "ngchol/Environment.hpp"
#include "ngchol/NumVec.hpp"
#include "ngchol/SparseMatrixStructure.hpp"

namespace LIBCHOLESKY{
class SparseMatrixStructure;
class Ordering{
  public:
    IntNumVec perm;
    IntNumVec invp;
    SparseMatrixStructure * pStructure;

    Ordering():pStructure(NULL){};
    
    void SetStructure(SparseMatrixStructure & aGlobal);
    void MMD();
    void AMD();
    void NDBOX();
    void NDGRID();

#ifdef USE_PARMETIS
  //FIXME: currently, colptr and rowind are not distributed
  void PARMETIS(MPI_Comm & comm);
#endif

#ifdef USE_PTSCOTCH
  //FIXME: currently, colptr and rowind are not distributed
  void PTSCOTCH(MPI_Comm & comm);
#endif

#ifdef USE_METIS
    void METIS();
#endif

#ifdef USE_SCOTCH
    void SCOTCH();
#endif
    void Compose(IntNumVec & invp2);
};

}
#endif // _ORDERING_HPP_
