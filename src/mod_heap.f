********************************************************************************
***  void mod_heap  (int *heap, int  *heap_size, int *vtx_to_heap, int vtx, int val )
***  {
***  		extern void move_up (int *, int , int ,   int *);
***  		extern void move_down (int *, int , int ,   int *);
***  		int i,j, k, p;
***  
***  		i = vtx_to_heap[vtx] ;
***  		j= (i <<1) -1 ;
***  		heap [j] = val;
***  
***  		p = (i >1) ? i >>1 : 0;
***  
***  		k =0;
***  		if ( p != 0) {
***  			if ( heap [ (p <<1) -1] > val) {
***  			k =1;
***  			move_up (heap, *heap_size, 
***  				i,  vtx_to_heap);
***  			}
***  		}
***  		if (k != 1) {
***  			j = i <<1;
***  			p = (j <= (*heap_size)) ? j: 0;
***  			if (p != 0) {
***  				if ( heap [ (p <<1) -1] < val) {
***  					k =2;
***  					move_down (heap, *heap_size, 
***  					i,  vtx_to_heap);
***  				}
***  			}
***  			if (k != 2){
***  				j++;
***  				p = (j <= (*heap_size)) ? j: 0;
***  				if (p != 0) {
***  					if ( heap [ (p <<1) -1] < val) 
***  					  move_down (heap, *heap_size, 
***  						i,  vtx_to_heap);
***  				}
***  			     } /*if k != 2 */
***  		}/*if k != 1 */	
***  
***  }
********************************************************************************
*

      subroutine  mod_heap
     &      ( heap, heap_size, vtx_to_heap, vtx, val )
*
        integer     heap(*)
        integer     heap_size
        integer     vtx_to_heap(*)
        integer     vtx
        integer     val
*
        integer     i, j, k, p
*
        i = vtx_to_heap(vtx)
        j = i*2 - 1
        heap(j) = val
*
        if  ( i .gt. 1 )  then
            p = i/2
        else
            p = 0
        endif
*
        k = 0
        if  ( p .ne. 0 )  then
            if  ( heap(p*2-1) .gt. val )  then
                k = 1
                call  move_up ( heap, heap_size, i, vtx_to_heap )
            endif
        endif
        if  ( k .ne. 1 )  then
            j = i*2
            if  ( j .le. heap_size )  then
                p = j
            else
                p = 0
            endif
            if  ( p .ne. 0 )  then
                if  ( heap(p*2-1) .lt. val )  then
                    k = 2
                    call  move_down ( heap, heap_size, i, vtx_to_heap )
                endif
            endif
            if  ( k .ne. 2 )  then
                j = j + 1
                if  ( j .le. heap_size )  then
                    p = j
                else
                    p = 0
                endif
                if  ( p .ne. 0 )  then
                    if  ( heap(p*2-1) .lt. val )  then
                        call  move_down ( heap, heap_size, i,
     &                                      vtx_to_heap )
                    endif
                endif
            endif
        endif
        return
      end
