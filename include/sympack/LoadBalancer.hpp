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
#ifndef _LOAD_BALANCER_HPP_
#define _LOAD_BALANCER_HPP_

#include "sympack/Environment.hpp"
#include "sympack/ETree.hpp"



namespace symPACK{


  class LoadBalancer{
    protected:
      std::vector<Int> procMap_;
      Int np_;
      Int n_;

    public:
      void Dump(){
        logfileptr->OFS()<<"Resulting proc mapping: "<<std::endl;
        for(auto it = procMap_.begin(); it!= procMap_.end();++it){
          logfileptr->OFS()<<*it<<" ";
        }
        logfileptr->OFS()<<std::endl;
      }

      LoadBalancer(Int np, Int n){
        n_=n;
        np_=np;
        procMap_.resize(n);
      };

      virtual std::vector<Int> & GetMap() =0;
  };



  class NNZBalancer: public LoadBalancer{
    protected:
      std::vector<Int> & Xsuper_;
      std::vector<Int> & cc_;
    public:
      NNZBalancer(Int np, std::vector<Int> & Xsuper, std::vector<Int> & pCc):Xsuper_(Xsuper),cc_(pCc),LoadBalancer(np,Xsuper.size()-1){
      };


      virtual std::vector<Int> & GetMap(){
        std::vector<double> load(np_,0.0);

        for(Int i = 1; i< Xsuper_.size();  ++i){
          //find least loaded processor
          std::vector<double>::iterator it = std::min_element(load.begin(),load.end());
          Int proc = (Int)(it - load.begin());
          Int width = Xsuper_[i] - Xsuper_[i-1];
          Int height = cc_[i-1];
          *it += (double)(width*height);
          procMap_[i-1] = proc;
        } 

        return procMap_;

      }
  };

  class WorkBalancer: public LoadBalancer{
    protected:
      std::vector<Int> & Xsuper_;
      std::vector<Int> & cc_;
    public:
      WorkBalancer(Int np, std::vector<Int> & Xsuper, std::vector<Int> & pCc):Xsuper_(Xsuper),cc_(pCc),LoadBalancer(np,Xsuper.size()-1){
      };


      virtual std::vector<Int> & GetMap(){
        std::vector<double> load(np_,0.0);

        for(Int i = 1; i< Xsuper_.size();  ++i){
          //find least loaded processor
          std::vector<double>::iterator it = std::min_element(load.begin(),load.end());
          Int proc = (Int)(it - load.begin());
          Int width = Xsuper_[i] - Xsuper_[i-1];
          Int height = cc_[i-1];

          double local_load = CHOLESKY_COST(height,width);
          *it += local_load;
          procMap_[i-1] = proc;
        } 

        return procMap_;

      }
  };




  class TreeLoadBalancer: public LoadBalancer{
    public:
      class ProcGroup{
        protected:
          std::vector<Int> ranks_;
          double total_load_;
        public:
          std::vector<Int> & Ranks(){return ranks_;}
          double & Load(){return total_load_;}

          ProcGroup():ranks_(),total_load_(0){}

          friend std::ostream& operator<<( std::ostream& os,  ProcGroup& group){
            os<<"Members are: "<<std::endl;
            for(Int ranks =0; ranks<group.Ranks().size();++ranks){
              os<<"P"<<group.Ranks()[ranks]<<" ";
            }
            os<<std::endl;
            return os;
          }

      };

    protected:
      std::vector< ProcGroup > procGroups_;
      ETree supETree_;

      std::vector< Int > groupIdx_;
      std::vector< Int > groupWorker_;
      std::vector< ProcGroup > levelGroups_;

      virtual double factor_cost(Int m, Int n) = 0;
      virtual double update_cost(Int m, Int n, Int k)=0;


    public:
      TreeLoadBalancer(Int np, ETree& supETree):supETree_(supETree),LoadBalancer(np,supETree.Size()){
        groupIdx_.resize(n_+1,0);
        groupWorker_.resize(n_+1,0);
      };

      std::vector< ProcGroup > & ProcGroups(){return procGroups_;}

      std::vector< ProcGroup > & LevelGroups(){return levelGroups_;}
      std::vector< Int > & GroupIdx(){return groupIdx_;}
      std::vector< Int > & GroupWorker(){return groupWorker_;}
      ETree & SupETree(){return supETree_;}


  };

  class SubtreeToSubcube: public TreeLoadBalancer{

    protected:
      bool fan_in_;
      std::vector<Int> & Xsuper_;
      std::vector<Int> & XsuperDist_;
      std::vector<Int> & SupMembership_;
      PtrVec & Xlindx_;
      IdxVec & Lindx_;
      MPI_Comm comm;
      std::vector<Int> & cc_;


      double factor_cost(Int m, Int n){
        return CHOLESKY_COST(m,n);
      }

      double update_cost(Int m, Int n, Int k){
        return 2.0*m*n*k;
      }



    public:
      SubtreeToSubcube(Int np, ETree & supETree,std::vector<Int> & Xsuper, std::vector<Int> & XsuperDist, std::vector<Int> & SupMembership,PtrVec & Xlindx, IdxVec & Lindx, std::vector<Int> & pCc, MPI_Comm & aComm, bool fan_in = true):Xsuper_(Xsuper),XsuperDist_(XsuperDist),SupMembership_(SupMembership),cc_(pCc),Xlindx_(Xlindx),Lindx_(Lindx),comm(aComm),TreeLoadBalancer(np,supETree){
        fan_in_=fan_in;
      };


      virtual std::vector<Int> & GetMap(){
        if(levelGroups_.size()==0){
    int iam =0;
    int np =1;
    MPI_Comm_rank(comm,&iam);
    MPI_Comm_size(comm,&np);

          //compute number of children and load
          Int numLevels = 1;
          std::vector<double> SubTreeLoad(supETree_.Size()+1,0.0);
          std::vector<double> NodeLoad(supETree_.Size()+1,0.0);
          std::vector<Int> children(supETree_.Size()+1,0);

#if 1
          {
            //          std::vector<double> SubTreeLoad(supETree_.Size()+1,0.0);
            //          std::vector<double> NodeLoad(supETree_.Size()+1,0.0);

            //      Int numLocSnode = ( (Xsuper_.size()-1) / np);
            //      Int firstSnode = iam*numLocSnode + 1;

            Int numLocSnode = XsuperDist_[iam+1]-XsuperDist_[iam];
            Int firstSnode = XsuperDist_[iam];



            for(Int locsupno = 1; locsupno<Xlindx_.size(); ++locsupno){
              Idx I = locsupno + firstSnode-1;

              Idx fc = Xsuper_[I-1];
              Idx lc = Xsuper_[I]-1;

              Int width = lc - fc + 1;
              Int height = cc_[fc-1];

              //cost of factoring curent panel
              double local_load = factor_cost(height,width);
              SubTreeLoad[I]+=local_load;
              NodeLoad[I]+=local_load;

              //cost of updating ancestors
              {
                Ptr lfi = Xlindx_[locsupno-1];
                Ptr lli = Xlindx_[locsupno]-1;

                //parse rows
                Int tgt_snode_id = I;
                Idx tgt_fr = fc;
                Idx tgt_lr = fc;
                Ptr tgt_fr_ptr = 0;
                Ptr tgt_lr_ptr = 0;
                for(Ptr rowptr = lfi; rowptr<=lli;rowptr++){
                  Idx row = Lindx_[rowptr-1]; 
                  if(SupMembership_[row-1]==tgt_snode_id){ continue;}

                  //we have a new tgt_snode_id
                  tgt_lr_ptr = rowptr-1;
                  tgt_lr = Lindx_[rowptr-1 -1];
                  if(tgt_snode_id!=I){
                    Int m = lli-tgt_fr_ptr+1;
                    Int n = width;
                    Int k = tgt_lr_ptr - tgt_fr_ptr+1; 
                    double cost = update_cost(m,n,k);
                    if(fan_in_){
                      SubTreeLoad[I]+=cost;
                      NodeLoad[I]+=cost;
                    }
                    else{
                      SubTreeLoad[tgt_snode_id]+=cost;
                      NodeLoad[tgt_snode_id]+=cost;
                    }
                  }
                  tgt_fr = row;
                  tgt_fr_ptr = rowptr;
                  tgt_snode_id = SupMembership_[row-1];
                }



                //do the last update supernode
                //we have a new tgt_snode_id
                tgt_lr_ptr = lli;
                tgt_lr = Lindx_[lli -1];
                if(tgt_snode_id!=I){
                  Int m = lli-tgt_fr_ptr+1;
                  Int n = width;
                  Int k = tgt_lr_ptr - tgt_fr_ptr+1; 
                  double cost = update_cost(m,n,k);
                  if(fan_in_){
                    SubTreeLoad[I]+=cost;
                    NodeLoad[I]+=cost;
                  }
                  else{
                    SubTreeLoad[tgt_snode_id]+=cost;
                    NodeLoad[tgt_snode_id]+=cost;
                  }
                }

              }
            }

            //Allreduce SubTreeLoad and NodeLoad
            MPI_Allreduce(MPI_IN_PLACE,&NodeLoad[0],NodeLoad.size(),MPI_DOUBLE,MPI_SUM,comm);
            MPI_Allreduce(MPI_IN_PLACE,&SubTreeLoad[0],SubTreeLoad.size(),MPI_DOUBLE,MPI_SUM,comm);

            for(Int I=1;I<=supETree_.Size();I++){
              Int parent = supETree_.Parent(I-1);
              ++children[parent];
              SubTreeLoad[parent]+=SubTreeLoad[I];
            }

            //logfileptr->OFS()<<"NodeLoad: "<<NodeLoad<<std::endl;
            //logfileptr->OFS()<<"SubTreeLoad: "<<SubTreeLoad<<std::endl;
          }

#else
          for(Int I=1;I<=supETree_.Size();I++){
            Int parent = supETree_.Parent(I-1);
            ++children[parent];
            Idx fc = Xsuper_[I-1];
            Int width = Xsuper_[I] - Xsuper_[I-1];
            Int height = cc_[fc-1];
            //cost of factoring curent panel
            double local_load = factor_cost(height,width);
            SubTreeLoad[I]+=local_load;
            NodeLoad[I]+=local_load;
            //cost of updating ancestors
            {
              Ptr fi = Xlindx_[I-1];
              Ptr li = Xlindx_[I]-1;

              //parse rows
              Int tgt_snode_id = I;
              Idx tgt_fr = fc;
              Idx tgt_lr = fc;
              Ptr tgt_fr_ptr = 0;
              Ptr tgt_lr_ptr = 0;
              for(Ptr rowptr = fi; rowptr<=li;rowptr++){
                Idx row = Lindx_[rowptr-1]; 
                if(SupMembership_[row-1]==tgt_snode_id){ continue;}

                //we have a new tgt_snode_id
                tgt_lr_ptr = rowptr-1;
                tgt_lr = Lindx_[rowptr-1 -1];
                if(tgt_snode_id!=I){
                  Int m = li-tgt_fr_ptr+1;
                  Int n = width;
                  Int k = tgt_lr_ptr - tgt_fr_ptr+1; 
                  double cost = update_cost(m,n,k);
                  if(fan_in_){
                    SubTreeLoad[I]+=cost;
                    NodeLoad[I]+=cost;
                  }
                  else{
                    SubTreeLoad[tgt_snode_id]+=cost;
                    NodeLoad[tgt_snode_id]+=cost;
                  }
                }
                tgt_fr = row;
                tgt_fr_ptr = rowptr;
                tgt_snode_id = SupMembership_[row-1];
              }
              //do the last update supernode
              //we have a new tgt_snode_id
              tgt_lr_ptr = li;
              tgt_lr = Lindx_[li -1];
              if(tgt_snode_id!=I){
                Int m = li-tgt_fr_ptr+1;
                Int n = width;
                Int k = tgt_lr_ptr - tgt_fr_ptr+1; 
                double cost = update_cost(m,n,k);
                if(fan_in_){
                  SubTreeLoad[I]+=cost;
                  NodeLoad[I]+=cost;
                }
                else{
                  SubTreeLoad[tgt_snode_id]+=cost;
                  NodeLoad[tgt_snode_id]+=cost;
                }
              }

            }
            SubTreeLoad[parent]+=SubTreeLoad[I];
          }

          logfileptr->OFS()<<"NodeLoad: "<<NodeLoad<<std::endl;
          logfileptr->OFS()<<"SubTreeLoad: "<<SubTreeLoad<<std::endl;

#endif

          std::vector<Int> levels(supETree_.Size()+1,0);
          for(Int I=n_; I>= 1;I--){ 
            Int parent = supETree_.Parent(I-1);
            if(parent==0){levels[I]=0;}
            else{ levels[I] = levels[parent]+1; numLevels = std::max(numLevels,levels[I]);}
          }
          numLevels++;

#ifdef _DEBUG_LOAD_BALANCER_
          logfileptr->OFS()<<"levels : "; 
          for(Int i = 0; i<levels.size();++i){logfileptr->OFS()<<levels.at(i)<<" ";}
          logfileptr->OFS()<<std::endl;
          logfileptr->OFS()<<"SubTreeLoad is "<<SubTreeLoad<<std::endl;
#endif

          //procmaps[0]/pstart[0] represents the complete list
          procGroups_.resize(n_+1);
          procGroups_[0].Ranks().reserve(np);
          for(Int p = 0;p<np;++p){ procGroups_[0].Ranks().push_back(p);}


          std::vector<Int> pstart(n_+1,0);
          levelGroups_.reserve(numLevels);
          levelGroups_.push_back(ProcGroup());//reserve(numLevels);
          levelGroups_[0].Ranks().reserve(np);
          for(Int p = 0;p<np;++p){levelGroups_[0].Ranks().push_back(p);}


          std::vector<double> load(n_+1,0.0);
          for(Int I=n_; I>= 1;I--){ 
            Int parent = supETree_.Parent(I-1);

            //split parent's proc list
            double parent_load = 0.0;

            if(parent!=0){
              Int fc = Xsuper_[parent-1];
              Int width = Xsuper_[parent] - Xsuper_[parent-1];
              Int height = cc_[fc-1];
              parent_load = NodeLoad[parent];// factor_cost(height,width);
              load[parent] = parent_load;
            }


            double proportion = std::min(1.0,SubTreeLoad[I]/(SubTreeLoad[parent]-parent_load));
            Int npParent = levelGroups_[groupIdx_[parent]].Ranks().size();
            Int pFirstIdx = std::min(pstart[parent],npParent-1);
            Int npIdeal =(Int)std::round(npParent*proportion);
            Int numProcs = std::max(1,std::min(npParent-pFirstIdx,npIdeal));
            Int pFirst = levelGroups_[groupIdx_[parent]].Ranks().at(pFirstIdx);

            //Int npParent = procGroups_[parent].Ranks().size();
            //Int pFirst = procGroups_[parent].Ranks().at(pFirstIdx);
#ifdef _DEBUG_LOAD_BALANCER_
            logfileptr->OFS()<<I<<" npParent = "<<npParent<<" pstartParent = "<<pstart[parent]<<" childrenParent = "<<children[parent]<<" pFirst = "<<pFirst<<" numProcs = "<<numProcs<<" proportion = "<<proportion<<std::endl; 
            logfileptr->OFS()<<I<<" npIdeal = "<<npIdeal<<" pFirstIdx = "<<pFirstIdx<<std::endl; 
#endif
            pstart[parent]+= numProcs;

            if(npParent!=numProcs){
              //if(iam>=pFirst && iam<pFirst+numProcs)
              {
                levelGroups_.push_back(ProcGroup());//reserve(numLevels);
                levelGroups_.back().Ranks().reserve(numProcs);
                //              for(Int p = pFirst; p<pFirst+numProcs;++p){ levelGroups_.back().Ranks().push_back(p);}

                std::vector<Int> & parentRanks = levelGroups_[groupIdx_[parent]].Ranks();
                levelGroups_.back().Ranks().insert(levelGroups_.back().Ranks().begin(),parentRanks.begin()+pFirstIdx,parentRanks.begin()+pFirstIdx+numProcs);

                groupIdx_[I] = levelGroups_.size()-1;
#ifdef _DEBUG_LOAD_BALANCER_
                logfileptr->OFS()<<"DEBUG "<<I<<" = "<<groupIdx_[I]<<" | "<<std::endl<<levelGroups_[groupIdx_[I]]<<std::endl;//for(Int p = pFirst; p<pFirst+numProcs;++p){ logfileptr->OFS()<<p<<" ";} logfileptr->OFS()<<std::endl;
#endif
              }
            }
            else{
              groupIdx_[I] = groupIdx_[parent];
              //logfileptr->OFS()<<"DEBUG "<<I<<" = "<<groupIdx_[I]<<" | ";for(Int p = pFirst; p<pFirst+numProcs;++p){ logfileptr->OFS()<<p<<" ";} logfileptr->OFS()<<std::endl;

#ifdef _DEBUG_LOAD_BALANCER_
              logfileptr->OFS()<<"DEBUG "<<I<<" = "<<groupIdx_[I]<<" | "<<std::endl<<levelGroups_[groupIdx_[I]]<<std::endl;//for(Int p = pFirst; p<pFirst+numProcs;++p){ logfileptr->OFS()<<p<<" ";} logfileptr->OFS()<<std::endl;
#endif
            }


            //            procGroups_[I].Ranks().reserve(numProcs);
            //            for(Int p = pFirst; p<pFirst+numProcs;++p){ procGroups_[I].Ranks().push_back(p);}
            //
            //pstart[parent]++;

            //            logfileptr->OFS()<<I<<": "; 
            //            for(Int i = 0; i<procGroups_[I].Ranks().size();++i){logfileptr->OFS()<<procGroups_[I].Ranks().at(i)<<" ";}
            //            logfileptr->OFS()<<std::endl;
          }


          //now choose which processor to get
          //std::vector<double> load(np,0.0);
          load.assign(np,0.0);
          for(Int I=1;I<=supETree_.Size();I++){
            Int minLoadP= -1;
            double minLoad = -1;
            ProcGroup & group = levelGroups_[groupIdx_[I]];
            for(Int i = 0; i<group.Ranks().size();++i){
              Int proc = group.Ranks()[i];
              if(load[proc]<minLoad || minLoad==-1){
                minLoad = load[proc];
                minLoadP = proc;
              }
            }

#ifdef _DEBUG_LOAD_BALANCER_
            logfileptr->OFS()<<"MinLoadP "<<minLoadP<<std::endl;
            logfileptr->OFS()<<"group of "<<I<<std::endl;
            for(Int i = 0; i<group.Ranks().size();++i){
              Int proc = group.Ranks()[i];
              logfileptr->OFS()<<proc<<" ["<<load[proc]<<"] ";
            }
            logfileptr->OFS()<<std::endl;
#endif
            groupWorker_[I] = minLoadP;

            //procGroups_[I].Worker() = minLoadP;


            Int fc = Xsuper_[I-1];
            Int width = Xsuper_[I] - Xsuper_[I-1];
            Int height = cc_[fc-1];
            //cost of factoring current node + updating ancestors (how about fanboth ?)
            //this is exactly the cost of FAN-In while 
            //CHOLESKY_COST(height,width) + SUM_child SubtreeLoads - SUM_descendant CHOLESKY_COSTS
            // would be the cost of Fan-Out
            double local_load = NodeLoad[I];//width*height*height;

            load[minLoadP]+=local_load;
          }


#ifdef _DEBUG_LOAD_BALANCER_
          logfileptr->OFS()<<"Proc load: "<<load<<std::endl;
#endif

          for(Int I=1;I<=supETree_.Size();I++){
            //for(Int I = 1; I<procGroups_.size();++I){ 
            //procMap_[I-1] = procGroups_[I].Worker();
            procMap_[I-1] = groupWorker_[I];
          }

#ifdef _DEBUG_LOAD_BALANCER_
          for(Int I = 0; I<levelGroups_.size();++I){ 
            logfileptr->OFS()<<"Group "<<I<<": "<<std::endl<<levelGroups_[I]<<std::endl;
          }
#endif

          }

          return procMap_;
        }

      };

      //TODO
      class SubtreeToSubcubeVolume: public TreeLoadBalancer{
        protected:
          bool fan_in_;
          std::vector<Int> & Xsuper_;
          std::vector<Int> & XsuperDist_;
          std::vector<Int> & SupMembership_;
          PtrVec & Xlindx_;
          IdxVec & Lindx_;
          MPI_Comm comm;
          std::vector<Int> & cc_;


          double factor_cost(Int m, Int n){
            return 0;//CHOLESKY_COST(m,n);
          }

          double update_cost(Int m, Int n, Int k){
            return m*n;
          }


        public:
          SubtreeToSubcubeVolume(Int np, ETree & supETree,std::vector<Int> & Xsuper,std::vector<Int> & XsuperDist, std::vector<Int> & SupMembership,PtrVec & Xlindx, IdxVec & Lindx, std::vector<Int> & pCc, MPI_Comm & aComm, bool fan_in = true):Xsuper_(Xsuper),XsuperDist_(XsuperDist),SupMembership_(SupMembership),cc_(pCc),Xlindx_(Xlindx),Lindx_(Lindx),comm(aComm),TreeLoadBalancer(np,supETree){
            fan_in_=fan_in;
          };


          virtual std::vector<Int> & GetMap(){
            if(levelGroups_.size()==0){

    int iam =0;
    int np =1;
    MPI_Comm_rank(comm,&iam);
    MPI_Comm_size(comm,&np);
              //compute number of children and load
              Int numLevels = 1;
              std::vector<double> SubTreeLoad(supETree_.Size()+1,0.0);
              std::vector<double> NodeLoad(supETree_.Size()+1,0.0);
              std::vector<Int> children(supETree_.Size()+1,0);


#if 1
              {
                //      Int numLocSnode = ( (Xsuper_.size()-1) / np);
                //      Int firstSnode = iam*numLocSnode + 1;

                Int numLocSnode = XsuperDist_[iam+1]-XsuperDist_[iam];
                Int firstSnode = XsuperDist_[iam];
                for(Int locsupno = 1; locsupno<Xlindx_.size(); ++locsupno){
                  Idx I = locsupno + firstSnode-1;

                  Idx fc = Xsuper_[I-1];
                  Idx lc = Xsuper_[I]-1;

                  Int width = lc - fc + 1;
                  Int height = cc_[fc-1];

                  //cost of factoring curent panel
                  double local_load = factor_cost(height,width);
                  SubTreeLoad[I]+=local_load;
                  NodeLoad[I]+=local_load;

                  //cost of updating ancestors
                  {
                    Ptr lfi = Xlindx_[locsupno-1];
                    Ptr lli = Xlindx_[locsupno]-1;

                    //parse rows
                    Int tgt_snode_id = I;
                    Idx tgt_fr = fc;
                    Idx tgt_lr = fc;
                    Ptr tgt_fr_ptr = 0;
                    Ptr tgt_lr_ptr = 0;
                    for(Ptr rowptr = lfi; rowptr<=lli;rowptr++){
                      Idx row = Lindx_[rowptr-1]; 
                      if(SupMembership_[row-1]==tgt_snode_id){ continue;}

                      //we have a new tgt_snode_id
                      tgt_lr_ptr = rowptr-1;
                      tgt_lr = Lindx_[rowptr-1 -1];
                      if(tgt_snode_id!=I){
                        Int m = lli-tgt_fr_ptr+1;
                        Int n = width;
                        Int k = tgt_lr_ptr - tgt_fr_ptr+1; 
                        double cost = update_cost(m,n,k);
                        if(fan_in_){
                          SubTreeLoad[I]+=cost;
                          NodeLoad[I]+=cost;
                        }
                        else{
                          SubTreeLoad[tgt_snode_id]+=cost;
                          NodeLoad[tgt_snode_id]+=cost;
                        }
                      }
                      tgt_fr = row;
                      tgt_fr_ptr = rowptr;
                      tgt_snode_id = SupMembership_[row-1];
                    }



                    //do the last update supernode
                    //we have a new tgt_snode_id
                    tgt_lr_ptr = lli;
                    tgt_lr = Lindx_[lli -1];
                    if(tgt_snode_id!=I){
                      Int m = lli-tgt_fr_ptr+1;
                      Int n = width;
                      Int k = tgt_lr_ptr - tgt_fr_ptr+1; 
                      double cost = update_cost(m,n,k);
                      if(fan_in_){
                        SubTreeLoad[I]+=cost;
                        NodeLoad[I]+=cost;
                      }
                      else{
                        SubTreeLoad[tgt_snode_id]+=cost;
                        NodeLoad[tgt_snode_id]+=cost;
                      }
                    }

                  }
                }

                //Allreduce SubTreeLoad and NodeLoad
                MPI_Allreduce(MPI_IN_PLACE,&NodeLoad[0],NodeLoad.size(),MPI_DOUBLE,MPI_SUM,comm);
                MPI_Allreduce(MPI_IN_PLACE,&SubTreeLoad[0],SubTreeLoad.size(),MPI_DOUBLE,MPI_SUM,comm);

                for(Int I=1;I<=supETree_.Size();I++){
                  Int parent = supETree_.Parent(I-1);
                  ++children[parent];
                  SubTreeLoad[parent]+=SubTreeLoad[I];
                }

                //logfileptr->OFS()<<"NodeLoad: "<<NodeLoad<<std::endl;
                //logfileptr->OFS()<<"SubTreeLoad: "<<SubTreeLoad<<std::endl;
              }
#else

              for(Int I=1;I<=supETree_.Size();I++){
                Int parent = supETree_.Parent(I-1);
                ++children[parent];
                Idx fc = Xsuper_[I-1];
                Int width = Xsuper_[I] - Xsuper_[I-1];
                Int height = cc_[fc-1];
                //cost of factoring curent panel
                double local_load = factor_cost(height,width);
                SubTreeLoad[I]+=local_load;
                NodeLoad[I]+=local_load;
                //cost of updating ancestors
                {
                  Ptr fi = Xlindx_[I-1];
                  Ptr li = Xlindx_[I]-1;

                  //parse rows
                  Int tgt_snode_id = I;
                  Idx tgt_fr = fc;
                  Idx tgt_lr = fc;
                  Ptr tgt_fr_ptr = 0;
                  Ptr tgt_lr_ptr = 0;
                  for(Ptr rowptr = fi; rowptr<=li;rowptr++){
                    Idx row = Lindx_[rowptr-1]; 
                    if(SupMembership_[row-1]==tgt_snode_id){ continue;}

                    //we have a new tgt_snode_id
                    tgt_lr_ptr = rowptr-1;
                    tgt_lr = Lindx_[rowptr-1 -1];
                    if(tgt_snode_id!=I){
                      Int m = li-tgt_fr_ptr+1;
                      Int n = width;
                      Int k = tgt_lr_ptr - tgt_fr_ptr+1; 
                      double cost = update_cost(m,n,k);
                      if(fan_in_){
                        SubTreeLoad[I]+=cost;
                        NodeLoad[I]+=cost;
                      }
                      else{
                        SubTreeLoad[tgt_snode_id]+=cost;
                        NodeLoad[tgt_snode_id]+=cost;
                      }
                    }
                    tgt_fr = row;
                    tgt_fr_ptr = rowptr;
                    tgt_snode_id = SupMembership_[row-1];
                  }
                  //do the last update supernode
                  //we have a new tgt_snode_id
                  tgt_lr_ptr = li;
                  tgt_lr = Lindx_[li -1];
                  if(tgt_snode_id!=I){
                    Int m = li-tgt_fr_ptr+1;
                    Int n = width;
                    Int k = tgt_lr_ptr - tgt_fr_ptr+1; 
                    double cost = update_cost(m,n,k);
                    if(fan_in_){
                      SubTreeLoad[I]+=cost;
                      NodeLoad[I]+=cost;
                    }
                    else{
                      SubTreeLoad[tgt_snode_id]+=cost;
                      NodeLoad[tgt_snode_id]+=cost;
                    }
                  }
                }
                SubTreeLoad[parent]+=SubTreeLoad[I];
              }
#endif









              std::vector<Int> levels(supETree_.Size()+1,0);
              for(Int I=n_; I>= 1;I--){ 
                Int parent = supETree_.Parent(I-1);
                if(parent==0){levels[I]=0;}
                else{ levels[I] = levels[parent]+1; numLevels = std::max(numLevels,levels[I]);}
              }
              numLevels++;
#ifdef _DEBUG_LOAD_BALANCER_
              logfileptr->OFS()<<"levels : "; 
              for(Int i = 0; i<levels.size();++i){logfileptr->OFS()<<levels.at(i)<<" ";}
              logfileptr->OFS()<<std::endl;
              logfileptr->OFS()<<"SubTreeLoad is "<<SubTreeLoad<<std::endl;
#endif

              //procmaps[0]/pstart[0] represents the complete list
              procGroups_.resize(n_+1);
              procGroups_[0].Ranks().reserve(np);
              for(Int p = 0;p<np;++p){ procGroups_[0].Ranks().push_back(p);}


              std::vector<Int> pstart(n_+1,0);
              levelGroups_.push_back(ProcGroup());//reserve(numLevels);
              levelGroups_[0].Ranks().reserve(np);
              for(Int p = 0;p<np;++p){levelGroups_[0].Ranks().push_back(p);}


              std::vector<double> load(n_+1,0.0);
              for(Int I=n_; I>= 1;I--){ 
                Int parent = supETree_.Parent(I-1);

                //split parent's proc list
                double parent_load = 0.0;

                if(parent!=0){
                  Int fc = Xsuper_[parent-1];
                  Int width = Xsuper_[parent] - Xsuper_[parent-1];
                  Int height = cc_[fc-1];
                  parent_load = NodeLoad[parent];// factor_cost(height,width);
                  load[parent] = parent_load;
                }


                double proportion = std::min(1.0,SubTreeLoad[I]/(SubTreeLoad[parent]-parent_load));
                Int npParent = levelGroups_[groupIdx_[parent]].Ranks().size();
                Int pFirstIdx = std::min(pstart[parent],npParent-1);
                Int npIdeal =(Int)std::round(npParent*proportion);
                Int numProcs = std::max(1,std::min(npParent-pFirstIdx,npIdeal));
                Int pFirst = levelGroups_[groupIdx_[parent]].Ranks().at(pFirstIdx);

                //Int npParent = procGroups_[parent].Ranks().size();
                //Int pFirst = procGroups_[parent].Ranks().at(pFirstIdx);
#ifdef _DEBUG_LOAD_BALANCER_
                logfileptr->OFS()<<I<<" npParent = "<<npParent<<" pstartParent = "<<pstart[parent]<<" childrenParent = "<<children[parent]<<" pFirst = "<<pFirst<<" numProcs = "<<numProcs<<" proportion = "<<proportion<<std::endl; 
                logfileptr->OFS()<<I<<" npIdeal = "<<npIdeal<<" pFirstIdx = "<<pFirstIdx<<std::endl; 
#endif
                pstart[parent]+= numProcs;

                if(npParent!=numProcs){
                  //if(iam>=pFirst && iam<pFirst+numProcs)
                  {
                    levelGroups_.push_back(ProcGroup());//reserve(numLevels);
                    levelGroups_.back().Ranks().reserve(numProcs);
                    //              for(Int p = pFirst; p<pFirst+numProcs;++p){ levelGroups_.back().Ranks().push_back(p);}

                    std::vector<Int> & parentRanks = levelGroups_[groupIdx_[parent]].Ranks();
                    levelGroups_.back().Ranks().insert(levelGroups_.back().Ranks().begin(),parentRanks.begin()+pFirstIdx,parentRanks.begin()+pFirstIdx+numProcs);

                    groupIdx_[I] = levelGroups_.size()-1;
#ifdef _DEBUG_LOAD_BALANCER_
                    logfileptr->OFS()<<"DEBUG "<<I<<" = "<<groupIdx_[I]<<" | "<<std::endl<<levelGroups_[groupIdx_[I]]<<std::endl;//for(Int p = pFirst; p<pFirst+numProcs;++p){ logfileptr->OFS()<<p<<" ";} logfileptr->OFS()<<std::endl;
#endif
                  }
                }
                else{
                  groupIdx_[I] = groupIdx_[parent];
                  //logfileptr->OFS()<<"DEBUG "<<I<<" = "<<groupIdx_[I]<<" | ";for(Int p = pFirst; p<pFirst+numProcs;++p){ logfileptr->OFS()<<p<<" ";} logfileptr->OFS()<<std::endl;
#ifdef _DEBUG_LOAD_BALANCER_
                  logfileptr->OFS()<<"DEBUG "<<I<<" = "<<groupIdx_[I]<<" | "<<std::endl<<levelGroups_[groupIdx_[I]]<<std::endl;//for(Int p = pFirst; p<pFirst+numProcs;++p){ logfileptr->OFS()<<p<<" ";} logfileptr->OFS()<<std::endl;
#endif
                }


                //            procGroups_[I].Ranks().reserve(numProcs);
                //            for(Int p = pFirst; p<pFirst+numProcs;++p){ procGroups_[I].Ranks().push_back(p);}
                //
                //pstart[parent]++;

                //            logfileptr->OFS()<<I<<": "; 
                //            for(Int i = 0; i<procGroups_[I].Ranks().size();++i){logfileptr->OFS()<<procGroups_[I].Ranks().at(i)<<" ";}
                //            logfileptr->OFS()<<std::endl;
              }


              //now choose which processor to get
              //std::vector<double> load(np,0.0);
              load.assign(np,0.0);
              for(Int I=1;I<=supETree_.Size();I++){
                Int minLoadP= -1;
                double minLoad = -1;
                ProcGroup & group = levelGroups_[groupIdx_[I]];
                for(Int i = 0; i<group.Ranks().size();++i){
                  Int proc = group.Ranks()[i];
                  if(load[proc]<minLoad || minLoad==-1){
                    minLoad = load[proc];
                    minLoadP = proc;
                  }
                }

#ifdef _DEBUG_LOAD_BALANCER_
                logfileptr->OFS()<<"MinLoadP "<<minLoadP<<std::endl;
                logfileptr->OFS()<<"group of "<<I<<std::endl;
                for(Int i = 0; i<group.Ranks().size();++i){
                  Int proc = group.Ranks()[i];
                  logfileptr->OFS()<<proc<<" ["<<load[proc]<<"] ";
                }
                logfileptr->OFS()<<std::endl;
#endif
                groupWorker_[I] = minLoadP;

                //procGroups_[I].Worker() = minLoadP;


                Int fc = Xsuper_[I-1];
                Int width = Xsuper_[I] - Xsuper_[I-1];
                Int height = cc_[fc-1];
                //cost of factoring current node + updating ancestors (how about fanboth ?)
                //this is exactly the cost of FAN-In while 
                //CHOLESKY_COST(height,width) + SUM_child SubtreeLoads - SUM_descendant CHOLESKY_COSTS
                // would be the cost of Fan-Out
                double local_load = NodeLoad[I];//width*height*height;

                load[minLoadP]+=local_load;
              }


#ifdef _DEBUG_LOAD_BALANCER_
              logfileptr->OFS()<<"Proc load: "<<load<<std::endl;
#endif


              for(Int I=1;I<=supETree_.Size();I++){
                //for(Int I = 1; I<procGroups_.size();++I){ 
                //procMap_[I-1] = procGroups_[I].Worker();
                procMap_[I-1] = groupWorker_[I];
              }

#ifdef _DEBUG_LOAD_BALANCER_
              for(Int I = 0; I<levelGroups_.size();++I){ 
                logfileptr->OFS()<<"Group "<<I<<": "<<std::endl<<levelGroups_[I]<<std::endl;
              }
#endif

              }

              return procMap_;
            }


            //  virtual inline std::vector<Int> & GetMap(){
            //      if(procGroups_.size()==0){
            //
            ////////gdb_lock();
            //////
            //////          //compute number of children and load
            //////          Int numLevels = 1;
            //////          std::vector<double> SubTreeLoad(supETree_.Size()+1,0.0);
            //////          std::vector<double> NodeLoad(supETree_.Size()+1,0.0);
            //////          std::vector<Int> children(supETree_.Size()+1,0);
            //////          for(Int I=1;I<=supETree_.Size();I++){
            //////            Int parent = supETree_.Parent(I-1);
            //////            ++children[parent];
            //////            Idx fc = Xsuper_[I-1];
            //////            Int width = Xsuper_[I] - Xsuper_[I-1];
            //////            Int height = cc_[fc-1];
            //////            //cost of factoring curent panel
            //////            double local_load = 0;//CHOLESKY_COST(height,width);
            //////            SubTreeLoad[I]+=local_load;
            //////            NodeLoad[I]+=local_load;
            //////            //cost of updating ancestors
            //////#if 1
            //////            {
            //////              Ptr fi = Xlindx_[I-1];
            //////              Ptr li = Xlindx_[I]-1;
            //////
            //////              //parse rows
            //////              Int tgt_snode_id = I;
            //////              Idx tgt_fr = fc;
            //////              Idx tgt_lr = fc;
            //////              Ptr tgt_fr_ptr = 0;
            //////              Ptr tgt_lr_ptr = 0;
            //////              for(Ptr rowptr = fi; rowptr<=li;rowptr++){
            //////                Idx row = Lindx_[rowptr-1]; 
            //////                if(SupMembership_[row-1]==tgt_snode_id){ continue;}
            //////
            //////                //we have a new tgt_snode_id
            //////                tgt_lr_ptr = rowptr-1;
            //////                tgt_lr = Lindx_[rowptr-1 -1];
            //////                if(tgt_snode_id!=I){
            //////                  Int m = li-tgt_fr_ptr+1;
            //////                  Int n = width;
            //////                  Int k = tgt_lr_ptr - tgt_fr_ptr+1; 
            //////                  //update cost is the volume
            //////                  double update_cost = m*k;
            //////                  SubTreeLoad[I]+=update_cost;
            //////                  NodeLoad[I]+=update_cost;
            //////                }
            //////                tgt_fr = row;
            //////                tgt_fr_ptr = rowptr;
            //////                tgt_snode_id = SupMembership_[row-1];
            //////              }
            //////              //do the last update supernode
            //////              //we have a new tgt_snode_id
            //////              tgt_lr_ptr = li;
            //////              tgt_lr = Lindx_[li -1];
            //////              if(tgt_snode_id!=I){
            //////                Int m = li-tgt_fr_ptr+1;
            //////                Int n = width;
            //////                Int k = tgt_lr_ptr - tgt_fr_ptr+1; 
            //////                //update cost is the volume
            //////                double update_cost = m*k;
            //////                SubTreeLoad[I]+=update_cost;
            //////                NodeLoad[I]+=update_cost;
            //////              }
            //////            }
            //////            SubTreeLoad[parent]+=SubTreeLoad[I];
            //////#else
            //////            SubTreeLoad[parent]+=SubTreeLoad[I];
            //////#endif
            //////          }
            //////
            //////          std::vector<Int> levels(supETree_.Size()+1,0);
            //////          for(Int I=n_; I>= 1;I--){ 
            //////            Int parent = supETree_.Parent(I-1);
            //////            if(parent==0){levels[I]=0;}
            //////            else{ levels[I] = levels[parent]+1; numLevels = max(numLevels,levels[I]);}
            //////          }
            //////          numLevels++;
            //////            logfileptr->OFS()<<"levels : "; 
            //////            for(Int i = 0; i<levels.size();++i){logfileptr->OFS()<<levels.at(i)<<" ";}
            //////            logfileptr->OFS()<<std::endl;
            //////
            //////
            //////          logfileptr->OFS()<<"SubTreeLoad is "<<SubTreeLoad<<std::endl;
            //////
            //////
            //////          //procmaps[0]/pstart[0] represents the complete list
            //////          procGroups_.resize(n_+1);
            //////          std::vector<Int> pstart(n_+1,0);
            //////          procGroups_[0].Ranks().reserve(np);
            //////          for(Int p = 0;p<np;++p){ procGroups_[0].Ranks().push_back(p);}
            //////
            //////
            //////          levelGroups_.push_back(ProcGroup());//reserve(numLevels);
            //////          levelGroups_[0].Ranks().reserve(np);
            //////          for(Int p = 0;p<np;++p){levelGroups_[0].Ranks().push_back(p);}
            //////
            //////
            //////          for(Int I=n_; I>= 1;I--){ 
            //////            Int parent = supETree_.Parent(I-1);
            //////
            //////            //split parent's proc list
            //////            double parent_load = 0.0;
            //////
            //////            if(parent!=0){
            //////              Int fc = Xsuper_[parent-1];
            //////              Int width = Xsuper_[parent] - Xsuper_[parent-1];
            //////              Int height = cc_[fc-1];
            //////              parent_load = 0;//width*height;//CHOLESKY_COST(height,width);
            //////              procGroups_[parent].Load() = parent_load;
            //////            }
            //////
            //////
            //////            double proportion = min(1.0,SubTreeLoad[I]/(SubTreeLoad[parent]-parent_load));
            //////            Int npParent = procGroups_[parent].Ranks().size();
            //////            Int pFirstIdx = min(pstart[parent],npParent-1);
            //////            Int npIdeal =(Int)std::round(npParent*proportion);
            //////            Int numProcs = max(1,min(npParent-pFirstIdx,npIdeal));
            //////            Int pFirst = procGroups_[parent].Ranks().at(pFirstIdx);
            //////
            //////            logfileptr->OFS()<<I<<" npParent = "<<npParent<<" pstartParent = "<<pstart[parent]<<" childrenParent = "<<children[parent]<<" pFirst = "<<pFirst<<" numProcs = "<<numProcs<<" proportion = "<<proportion<<std::endl; 
            //////            pstart[parent]+= numProcs;
            //////
            //////            if(npParent!=numProcs){
            //////            if(iam<pFirst || iam>=pFirst+numProcs){
            //////
            //////            }
            //////            else{
            //////              levelGroups_.push_back(ProcGroup());//reserve(numLevels);
            //////              levelGroups_.back().Ranks().reserve(numProcs);
            //////              for(Int p = pFirst; p<pFirst+numProcs;++p){ levelGroups_.back().Ranks().push_back(p);}
            //////              groupIdx_[I] = levelGroups_.size()-1;
            //////              logfileptr->OFS()<<"DEBUG "<<I<<" = "<<groupIdx_[I]<<" | ";for(Int p = pFirst; p<pFirst+numProcs;++p){ logfileptr->OFS()<<p<<" ";} logfileptr->OFS()<<std::endl;
            //////            }
            //////            }
            //////            else{
            //////              groupIdx_[I] = groupIdx_[parent];
            //////              logfileptr->OFS()<<"DEBUG "<<I<<" = "<<groupIdx_[I]<<" | ";for(Int p = pFirst; p<pFirst+numProcs;++p){ logfileptr->OFS()<<p<<" ";} logfileptr->OFS()<<std::endl;
            //////            }
            //////
            //////            procGroups_[I].Ranks().reserve(numProcs);
            //////            for(Int p = pFirst; p<pFirst+numProcs;++p){ procGroups_[I].Ranks().push_back(p);}
            //////
            //////            pstart[parent]++;
            //////
            //////            logfileptr->OFS()<<I<<": "; 
            //////            for(Int i = 0; i<procGroups_[I].Ranks().size();++i){logfileptr->OFS()<<procGroups_[I].Ranks().at(i)<<" ";}
            //////            logfileptr->OFS()<<std::endl;
            //////          }
            //////
            //////
            //////          //now choose which processor to get
            //////          std::vector<double> load(np,0.0);
            //////          for(Int I=1;I<=supETree_.Size();I++){
            //////            Int minLoadP= -1;
            //////            double minLoad = -1;
            //////            for(Int i = 0; i<procGroups_[I].Ranks().size();++i){
            //////              Int proc = procGroups_[I].Ranks().at(i);
            //////              if(load[proc]<minLoad || minLoad==-1){
            //////                minLoad = load[proc];
            //////                minLoadP = proc;
            //////              }
            //////            }
            //////
            //////            procGroups_[I].Worker() = minLoadP;
            //////
            //////
            //////            Int fc = Xsuper_[I-1];
            //////            Int width = Xsuper_[I] - Xsuper_[I-1];
            //////            Int height = cc_[fc-1];
            //////            //cost of factoring current node + updating ancestors (how about fanboth ?)
            //////            //this is exactly the cost of FAN-In while 
            //////            //CHOLESKY_COST(height,width) + SUM_child SubtreeLoads - SUM_descendant CHOLESKY_COSTS
            //////            // would be the cost of Fan-Out
            //////            double local_load = NodeLoad[I];//width*height*height;
            //////
            //////            load[minLoadP]+=local_load;
            //////          }
            //////
            //////
            //////          logfileptr->OFS()<<"Proc load: "<<load<<std::endl;
            //////
            //////
            //////          for(Int I = 1; I<procGroups_.size();++I){ 
            //////            procMap_[I-1] = procGroups_[I].Worker();
            //////          }
            //////
            //////          for(Int I = 0; I<levelGroups_.size();++I){ 
            //////            logfileptr->OFS()<<"Group "<<I<<": "<<std::endl<<levelGroups_[I]<<std::endl;
            //////          }
            //////
            //////
            //        }
            //
            //        return procMap_;
            //  }

          };


          class SubtreeToSubcubeGlobal: public TreeLoadBalancer{

            protected:
              bool fan_in_;
              std::vector<Int> & Xsuper_;
              std::vector<Int> & SupMembership_;
              PtrVec & Xlindx_;
              IdxVec & Lindx_;
              MPI_Comm comm;
              std::vector<Int> & cc_;


              double factor_cost(Int m, Int n){
                return CHOLESKY_COST(m,n);
              }

              double update_cost(Int m, Int n, Int k){
                return 2.0*m*n*k;
              }



            public:

              SubtreeToSubcubeGlobal(Int np, ETree & supETree,std::vector<Int> & Xsuper, std::vector<Int> & SupMembership,PtrVec & Xlindx, IdxVec & Lindx, std::vector<Int> & pCc, MPI_Comm & aComm, bool fan_in = true):Xsuper_(Xsuper),SupMembership_(SupMembership),cc_(pCc),Xlindx_(Xlindx),Lindx_(Lindx),comm(aComm),TreeLoadBalancer(np,supETree){
                fan_in_=fan_in;
              };


              virtual std::vector<Int> & GetMap(){
                if(levelGroups_.size()==0){


    int iam =0;
    int np =1;
    MPI_Comm_rank(comm,&iam);
    MPI_Comm_size(comm,&np);


                  //compute number of children and load
                  Int numLevels = 1;
                  std::vector<double> SubTreeLoad(supETree_.Size()+1,0.0);
                  std::vector<double> NodeLoad(supETree_.Size()+1,0.0);
                  std::vector<Int> children(supETree_.Size()+1,0);

                  for(Int I=1;I<=supETree_.Size();I++){
                    Int parent = supETree_.Parent(I-1);
                    ++children[parent];
                    Idx fc = Xsuper_[I-1];
                    Int width = Xsuper_[I] - Xsuper_[I-1];
                    Int height = cc_[fc-1];
                    //cost of factoring curent panel
                    double local_load = factor_cost(height,width);
                    SubTreeLoad[I]+=local_load;
                    NodeLoad[I]+=local_load;
                    //cost of updating ancestors
                    {
                      Ptr fi = Xlindx_[I-1];
                      Ptr li = Xlindx_[I]-1;

                      //parse rows
                      Int tgt_snode_id = I;
                      Idx tgt_fr = fc;
                      Idx tgt_lr = fc;
                      Ptr tgt_fr_ptr = 0;
                      Ptr tgt_lr_ptr = 0;
                      for(Ptr rowptr = fi; rowptr<=li;rowptr++){
                        Idx row = Lindx_[rowptr-1]; 
                        if(SupMembership_[row-1]==tgt_snode_id){ continue;}

                        //we have a new tgt_snode_id
                        tgt_lr_ptr = rowptr-1;
                        tgt_lr = Lindx_[rowptr-1 -1];
                        if(tgt_snode_id!=I){
                          Int m = li-tgt_fr_ptr+1;
                          Int n = width;
                          Int k = tgt_lr_ptr - tgt_fr_ptr+1; 
                          double cost = update_cost(m,n,k);
                          if(fan_in_){
                            SubTreeLoad[I]+=cost;
                            NodeLoad[I]+=cost;
                          }
                          else{
                            SubTreeLoad[tgt_snode_id]+=cost;
                            NodeLoad[tgt_snode_id]+=cost;
                          }
                        }
                        tgt_fr = row;
                        tgt_fr_ptr = rowptr;
                        tgt_snode_id = SupMembership_[row-1];
                      }
                      //do the last update supernode
                      //we have a new tgt_snode_id
                      tgt_lr_ptr = li;
                      tgt_lr = Lindx_[li -1];
                      if(tgt_snode_id!=I){
                        Int m = li-tgt_fr_ptr+1;
                        Int n = width;
                        Int k = tgt_lr_ptr - tgt_fr_ptr+1; 
                        double cost = update_cost(m,n,k);
                        if(fan_in_){
                          SubTreeLoad[I]+=cost;
                          NodeLoad[I]+=cost;
                        }
                        else{
                          SubTreeLoad[tgt_snode_id]+=cost;
                          NodeLoad[tgt_snode_id]+=cost;
                        }
                      }

                    }
                    SubTreeLoad[parent]+=SubTreeLoad[I];
                  }

                  logfileptr->OFS()<<"NodeLoad: "<<NodeLoad<<std::endl;
                  logfileptr->OFS()<<"SubTreeLoad: "<<SubTreeLoad<<std::endl;


                  std::vector<Int> levels(supETree_.Size()+1,0);
                  for(Int I=n_; I>= 1;I--){ 
                    Int parent = supETree_.Parent(I-1);
                    if(parent==0){levels[I]=0;}
                    else{ levels[I] = levels[parent]+1; numLevels = std::max(numLevels,levels[I]);}
                  }
                  numLevels++;

#ifdef _DEBUG_LOAD_BALANCER_
                  logfileptr->OFS()<<"levels : "; 
                  for(Int i = 0; i<levels.size();++i){logfileptr->OFS()<<levels.at(i)<<" ";}
                  logfileptr->OFS()<<std::endl;
                  logfileptr->OFS()<<"SubTreeLoad is "<<SubTreeLoad<<std::endl;
#endif

                  //procmaps[0]/pstart[0] represents the complete list
                  procGroups_.resize(n_+1);
                  procGroups_[0].Ranks().reserve(np);
                  for(Int p = 0;p<np;++p){ procGroups_[0].Ranks().push_back(p);}


                  std::vector<Int> pstart(n_+1,0);
                  levelGroups_.reserve(numLevels);
                  levelGroups_.push_back(ProcGroup());//reserve(numLevels);
                  levelGroups_[0].Ranks().reserve(np);
                  for(Int p = 0;p<np;++p){levelGroups_[0].Ranks().push_back(p);}


                  std::vector<double> load(n_+1,0.0);
                  for(Int I=n_; I>= 1;I--){ 
                    Int parent = supETree_.Parent(I-1);

                    //split parent's proc list
                    double parent_load = 0.0;

                    if(parent!=0){
                      Int fc = Xsuper_[parent-1];
                      Int width = Xsuper_[parent] - Xsuper_[parent-1];
                      Int height = cc_[fc-1];
                      parent_load = NodeLoad[parent];// factor_cost(height,width);
                      load[parent] = parent_load;
                    }


                    double proportion = std::min(1.0,SubTreeLoad[I]/(SubTreeLoad[parent]-parent_load));
                    Int npParent = levelGroups_[groupIdx_[parent]].Ranks().size();
                    Int pFirstIdx = std::min(pstart[parent],npParent-1);
                    Int npIdeal =(Int)std::round(npParent*proportion);
                    Int numProcs = std::max(1,std::min(npParent-pFirstIdx,npIdeal));
                    Int pFirst = levelGroups_[groupIdx_[parent]].Ranks().at(pFirstIdx);

                    //Int npParent = procGroups_[parent].Ranks().size();
                    //Int pFirst = procGroups_[parent].Ranks().at(pFirstIdx);
#ifdef _DEBUG_LOAD_BALANCER_
                    logfileptr->OFS()<<I<<" npParent = "<<npParent<<" pstartParent = "<<pstart[parent]<<" childrenParent = "<<children[parent]<<" pFirst = "<<pFirst<<" numProcs = "<<numProcs<<" proportion = "<<proportion<<std::endl; 
                    logfileptr->OFS()<<I<<" npIdeal = "<<npIdeal<<" pFirstIdx = "<<pFirstIdx<<std::endl; 
#endif
                    pstart[parent]+= numProcs;

                    if(npParent!=numProcs){
                      //if(iam>=pFirst && iam<pFirst+numProcs)
                      {
                        levelGroups_.push_back(ProcGroup());//reserve(numLevels);
                        levelGroups_.back().Ranks().reserve(numProcs);
                        //              for(Int p = pFirst; p<pFirst+numProcs;++p){ levelGroups_.back().Ranks().push_back(p);}

                        std::vector<Int> & parentRanks = levelGroups_[groupIdx_[parent]].Ranks();
                        levelGroups_.back().Ranks().insert(levelGroups_.back().Ranks().begin(),parentRanks.begin()+pFirstIdx,parentRanks.begin()+pFirstIdx+numProcs);

                        groupIdx_[I] = levelGroups_.size()-1;
#ifdef _DEBUG_LOAD_BALANCER_
                        logfileptr->OFS()<<"DEBUG "<<I<<" = "<<groupIdx_[I]<<" | "<<std::endl<<levelGroups_[groupIdx_[I]]<<std::endl;//for(Int p = pFirst; p<pFirst+numProcs;++p){ logfileptr->OFS()<<p<<" ";} logfileptr->OFS()<<std::endl;
#endif
                      }
                    }
                    else{
                      groupIdx_[I] = groupIdx_[parent];

#ifdef _DEBUG_LOAD_BALANCER_
                      logfileptr->OFS()<<"DEBUG "<<I<<" = "<<groupIdx_[I]<<" | "<<std::endl<<levelGroups_[groupIdx_[I]]<<std::endl;
#endif
                    }


                  }


                  //now choose which processor to get
                  //std::vector<double> load(np,0.0);
                  load.assign(np,0.0);
                  for(Int I=1;I<=supETree_.Size();I++){
                    Int minLoadP= -1;
                    double minLoad = -1;
                    ProcGroup & group = levelGroups_[groupIdx_[I]];
                    for(Int i = 0; i<group.Ranks().size();++i){
                      Int proc = group.Ranks()[i];
                      if(load[proc]<minLoad || minLoad==-1){
                        minLoad = load[proc];
                        minLoadP = proc;
                      }
                    }

#ifdef _DEBUG_LOAD_BALANCER_
                    logfileptr->OFS()<<"MinLoadP "<<minLoadP<<std::endl;
                    logfileptr->OFS()<<"group of "<<I<<std::endl;
                    for(Int i = 0; i<group.Ranks().size();++i){
                      Int proc = group.Ranks()[i];
                      logfileptr->OFS()<<proc<<" ["<<load[proc]<<"] ";
                    }
                    logfileptr->OFS()<<std::endl;
#endif
                    groupWorker_[I] = minLoadP;

                    //procGroups_[I].Worker() = minLoadP;


                    Int fc = Xsuper_[I-1];
                    Int width = Xsuper_[I] - Xsuper_[I-1];
                    Int height = cc_[fc-1];
                    //cost of factoring current node + updating ancestors (how about fanboth ?)
                    //this is exactly the cost of FAN-In while 
                    //CHOLESKY_COST(height,width) + SUM_child SubtreeLoads - SUM_descendant CHOLESKY_COSTS
                    // would be the cost of Fan-Out
                    double local_load = NodeLoad[I];//width*height*height;

                    load[minLoadP]+=local_load;
                  }


#ifdef _DEBUG_LOAD_BALANCER_
                  logfileptr->OFS()<<"Proc load: "<<load<<std::endl;
#endif

                  for(Int I=1;I<=supETree_.Size();I++){
                    //for(Int I = 1; I<procGroups_.size();++I){ 
                    //procMap_[I-1] = procGroups_[I].Worker();
                    procMap_[I-1] = groupWorker_[I];
                  }

#ifdef _DEBUG_LOAD_BALANCER_
                  for(Int I = 0; I<levelGroups_.size();++I){ 
                    logfileptr->OFS()<<"Group "<<I<<": "<<std::endl<<levelGroups_[I]<<std::endl;
                  }
#endif

                  }

                  return procMap_;
                }

              };


          }

#endif //_LOAD_BALANCER_HPP_
