
/* 
//  comparison: ´while (1)´ VS ´for (;;)´

#include <stdio.h>
int main() {
    for (;;) {
        printf("hello world\n");
    }
    // while (1) {
    //     printf("hello world\n");
    // }
}

// for (;;) -> ´´´
//.LC0:
//        .string "hello world"
//main:
//        push    rbp
//        mov     rbp, rsp
//.L2:
//        mov     edi, OFFSET FLAT:.LC0
//        call    puts
//        jmp     .L2 ´´´

// while (1) -> ´´´
//.LC0:
//        .string "hello world"
//main:
//        push    rbp
//        mov     rbp, rsp
//.L2:
//        mov     edi, OFFSET FLAT:.LC0
//        call    puts
//        jmp     .L2´´´

//  result: identical, at least for Observable code
*/