	.syntax unified
	.cpu cortex-m3
	.fpu softvfp
	.thumb

.global fork
fork:
	push {r7}
	mov r7, #0x1
	svc 0
	nop
	pop {r7}
	bx lr
.global getpid
getpid:
	push {r7}
	mov r7, #0x2
	svc 0
	nop
	pop {r7}
	bx lr
.global write
write:
	push {r7}
	mov r7, #0x3
	svc 0
	nop
	pop {r7}
	bx lr
.global read
read:
	push {r7}
	mov r7, #0x4
	svc 0
	nop
	pop {r7}
	bx lr
.global wait_interrupt
wait_interrupt:
	push {r7}
	mov r7, #0x5
	svc 0
	nop
	pop {r7}
	bx lr
.global getpriority
getpriority:
	push {r7}
	mov r7, #0x6
	svc 0
	nop
	pop {r7}
	bx lr
.global setpriority
setpriority:
	push {r7}
	mov r7, #0x7
	svc 0
	nop
	pop {r7}
	bx lr
.global mknod
mknod:
	push {r7}
	mov r7, #0x8
	svc 0
	nop
	pop {r7}
	bx lr
.global sleep
sleep:
	push {r7}
	mov r7, #0x9
	svc 0
	nop
	pop {r7}
	bx lr

.global process_snapshot
process_snapshot:
        push {r7}
        mov r7, #0xA
        svc 0
        nop
        pop {r7}
        bx lr

.global get_tick_count
get_tick_count:
        push {r7}
        mov r7, #0xB
        svc 0
        nop
        pop {r7}
        bx lr
