/* * * * NO CODE WITHIN THIS FILE IS MY OWN ORIGINAL WORK. * * * *
 *
 * README:
 * This file serves as a place to store any and all code within the indexer project that is not my own work.
 * 
 * The file will contain ALL lines of code that meet ALL of the following criteria: 
 *  1. not written by the submission author
 *  3. not imported through default libraries such as <stdio.h>
 *  2. not included in the precode
 * 
 * If a piece of code is in this folder, consider it to be copied in its entirety from the source provided, 
 * regardless of whether modifications may or may not have been made when comparing the present code to its original state.
 * 
*/

#include <stdio.h>

/*
 * * * * NO CODE WITHIN THIS FILE IS MY OWN ORIGINAL WORK. * * * *
 * fast_compare:
 *  Source: <https://mgronhol.github.io/fast-strcmp/>
 *  Author: Markus Grönholm, Alshain ltd., 2014
 * 
 * Own notes:
 *  What it does: provides faster string comparison than the "strncmp" function within <string.h>
 *  How it does it: character comparison through bitwise XOR (?)
 * 
 */
int fast_compare( const char *ptr0, const char *ptr1, int len ){
    int fast = len/sizeof(size_t) + 1;
    int offset = (fast-1)*sizeof(size_t);
    int current_block = 0;

    if( len <= sizeof(size_t)){ fast = 0; }


    size_t *lptr0 = (size_t*)ptr0;
    size_t *lptr1 = (size_t*)ptr1;

    while( current_block < fast ){
        if( (lptr0[current_block] ^ lptr1[current_block] )){
            int pos;
            for(pos = current_block*sizeof(size_t); pos < len ; ++pos ){
                    if( (ptr0[pos] ^ ptr1[pos]) || (ptr0[pos] == 0) || (ptr1[pos] == 0) ){
                    return  (int)((unsigned char)ptr0[pos] - (unsigned char)ptr1[pos]);
                }
            }
        }

        ++current_block;
    }

    while( len > offset ){
        if( (ptr0[offset] ^ ptr1[offset] )){ 
            return (int)((unsigned char)ptr0[offset] - (unsigned char)ptr1[offset]); 
        }
        ++offset;
    }


    return 0;
}